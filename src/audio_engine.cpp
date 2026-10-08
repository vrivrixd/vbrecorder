/*
 * VB Recorder - Windows audio recorder
 * Copyright (C) 2026 Vitor Bruski
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY. See the GNU General Public License for more details.
 *
 * A copy of the license comes with this program, in the LICENSE file. If not,
 * see <https://www.gnu.org/licenses/>.
 */

/*
 * How recording works
 *
 * Each source (device or process) delivers audio at its own pace, on its own
 * thread, into a queue of its own (a BASS push stream). A separate thread, the
 * "pump", takes from the queues the audio to be mixed and encoded at the pace
 * of the computer clock, with a delay equal to the buffer chosen in the
 * settings.
 *
 * That delay is the slack: if a source is late by less than the buffer,
 * nothing is lost, because the pump has not reached that stretch yet. If it is
 * later than that, the pump waits for it, for up to one more full buffer. Only
 * when a source stalls for longer (a process that stopped playing, for
 * example) does the missing stretch become silence.
 *
 * Sound card clocks never match the computer clock exactly. The pump watches
 * the size of each queue and adjusts the read speed of that source by
 * fractions of a thousandth, enough to keep the queue from draining or growing
 * forever, and too little for anyone to hear.
 *
 * In the previous version, the mixer pulled a fixed-size block on every device
 * callback, or every 20 ms when there were only processes. When the audio of a
 * process arrived a little after that moment, that block went out without it:
 * that was the stutter. The buffer setting had no effect on it.
 */

#include "audio_engine.h"

#include <algorithm>
#include <cmath>

#include "i18n.h"
#include "process_audio.h"
#include "util.h"

namespace {

/* Subfolder that holds the BASS DLLs. */
const wchar_t* const LIB_DIR = L"lib";

/* Formats BASS only plays with an extra plugin in the program folder. */
const std::pair<const wchar_t*, const wchar_t*> PLAYBACK_PLUGINS[] = {
    {L"flac", L"bassflac.dll"},
    {L"opus", L"bassopus.dll"},
};

/* How often devices deliver audio, in ms. */
const DWORD RECORD_PERIOD = 20;

/* Maximum adjustment of a source's read speed: 0.2%. */
const double MAX_DRIFT = 0.002;

/* Opting out of power saving keeps Windows from slowing the program down while
   it records (EcoQoS) and from delaying its timers. */
void SetPowerThrottling(bool allow) {
    struct ThrottlingState {
        ULONG Version, ControlMask, StateMask;
    };
    typedef BOOL(WINAPI * SetProcessInformationFn)(HANDLE, int, LPVOID, DWORD);
    static SetProcessInformationFn fn = (SetProcessInformationFn)(void*)GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "SetProcessInformation");
    if (!fn) return;
    const ULONG execution_speed = 0x1, ignore_timer_resolution = 0x4;
    ThrottlingState state = {1, allow ? 0 : (execution_speed | ignore_timer_resolution), 0};
    fn(GetCurrentProcess(), 4 /* ProcessPowerThrottling */, &state, sizeof(state));
}

std::wstring SourceSafeName(const std::wstring& original) {
    std::wstring name = original;
    // Removes the " (PID 1234)" from process names.
    for (;;) {
        size_t at = name.find(L"(PID ");
        if (at == std::wstring::npos) break;
        size_t digits = at + 5, end = digits;
        while (end < name.size() && iswdigit(name[end])) ++end;
        if (end == digits || end >= name.size() || name[end] != L')') break;
        size_t start = at;
        while (start > 0 && iswspace(name[start - 1])) --start;
        name.erase(start, end + 1 - start);
    }
    name = Trim(name);
    std::wstring safe;
    for (wchar_t c : name)
        if (!wcschr(L"\\/:*?\"<>|", c)) safe += c;
    safe = Trim(safe);
    return safe.empty() ? tr("fonte") : safe;
}

}  // namespace

/* A recording source: its queue and whatever fills it. */
struct AudioEngine::Input : public CaptureSink {
    AudioEngine* engine = nullptr;
    bool is_device = false;
    int id = 0;
    DWORD push = 0;
    DWORD rec = 0;
    std::unique_ptr<ProcessLoopbackCapture> cap;
    int rate = 0;
    int chans = 0;
    float volume = 1.0f;
    std::vector<float> scratch;  // used only by the thread that delivers the audio

    // Used only by the pump.
    double level_avg = -1.0;
    double factor = 1.0;

    void Put(const float* data, DWORD samples, bool silent) {
        if (engine->paused_ || !push || !samples) return;
        if (silent || volume != 1.0f) {
            scratch.resize(samples);
            if (silent) {
                std::fill(scratch.begin(), scratch.end(), 0.0f);
            } else {
                for (DWORD i = 0; i < samples; ++i) scratch[i] = data[i] * volume;
            }
            data = scratch.data();
        }
        engine->b_.StreamPutData(push, data, samples * sizeof(float));
    }

    void OnAudio(const float* data, DWORD samples, bool silent) override {
        Put(data, samples, silent);
    }

    double BytesPerSecond() const { return (double)rate * chans * sizeof(float); }
};

/* An output file: the mixer that joins its sources, and the encoder. */
struct AudioEngine::Output {
    DWORD mixer = 0;
    DWORD encoder = 0;
    int rate = 0;
    int chans = 0;
    std::vector<Input*> inputs;
    long long frames_done = 0;
};

AudioEngine::AudioEngine() {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    qpc_freq_ = f.QuadPart;
    pump_stop_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
}

AudioEngine::~AudioEngine() { CloseHandle(pump_stop_); }

bool AudioEngine::LoadDlls() {
    std::wstring base = ExeDir();
    lib_path_ = JoinPath(base, LIB_DIR);
    // The DLLs live in lib; if they are not there, look next to the program.
    if (!FileExists(JoinPath(lib_path_, L"bass.dll"))) lib_path_ = base;

    auto load = [&](const wchar_t* name) {
        return LoadLibraryExW(JoinPath(lib_path_, name).c_str(), nullptr,
                              LOAD_WITH_ALTERED_SEARCH_PATH);
    };
    HMODULE bass = load(L"bass.dll");
    HMODULE enc = load(L"bassenc.dll");
    HMODULE mp3 = load(L"bassenc_mp3.dll");
    HMODULE ogg = load(L"bassenc_ogg.dll");
    HMODULE flac = load(L"bassenc_flac.dll");
    HMODULE mix = load(L"bassmix.dll");
    HMODULE opus = load(L"bassenc_opus.dll");
    if (!bass || !enc || !mp3 || !ogg || !flac || !mix || !opus) return false;

    bool ok = true;
    auto get = [&](HMODULE dll, const char* name, auto& fn, bool required = true) {
        fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(
            (void*)GetProcAddress(dll, name));
        if (!fn && required) ok = false;
    };
    get(bass, "BASS_SetConfig", b_.SetConfig);
    get(bass, "BASS_GetConfig", b_.GetConfig);
    get(bass, "BASS_ErrorGetCode", b_.ErrorGetCode);
    get(bass, "BASS_Init", b_.Init);
    get(bass, "BASS_Free", b_.Free);
    get(bass, "BASS_RecordInit", b_.RecordInit);
    get(bass, "BASS_RecordSetDevice", b_.RecordSetDevice);
    get(bass, "BASS_RecordGetDeviceInfo", b_.RecordGetDeviceInfo);
    get(bass, "BASS_RecordStart", b_.RecordStart);
    get(bass, "BASS_StreamCreate", b_.StreamCreate);
    get(bass, "BASS_StreamCreateFile", b_.StreamCreateFile);
    get(bass, "BASS_StreamFree", b_.StreamFree);
    get(bass, "BASS_StreamPutData", b_.StreamPutData);
    get(bass, "BASS_ChannelPlay", b_.ChannelPlay);
    get(bass, "BASS_ChannelStop", b_.ChannelStop);
    get(bass, "BASS_ChannelGetData", b_.ChannelGetData);
    get(bass, "BASS_ChannelSetAttribute", b_.ChannelSetAttribute);
    get(bass, "BASS_ChannelIsActive", b_.ChannelIsActive);
    get(bass, "BASS_PluginLoad", b_.PluginLoad);
    get(enc, "BASS_Encode_Start", b_.EncodeStart);
    get(enc, "BASS_Encode_StartPCMFile", b_.EncodeStartPCMFile);
    get(enc, "BASS_Encode_Stop", b_.EncodeStop);
    get(enc, "BASS_Encode_StopEx", b_.EncodeStopEx, false);
    get(mp3, "BASS_Encode_MP3_StartFile", b_.EncodeMP3StartFile);
    get(ogg, "BASS_Encode_OGG_StartFile", b_.EncodeOGGStartFile);
    get(flac, "BASS_Encode_FLAC_StartFile", b_.EncodeFLACStartFile);
    get(opus, "BASS_Encode_OPUS_StartFile", b_.EncodeOPUSStartFile);
    get(mix, "BASS_Mixer_StreamCreate", b_.MixerStreamCreate);
    get(mix, "BASS_Mixer_StreamAddChannel", b_.MixerStreamAddChannel);
    return ok;
}

bool AudioEngine::Load() {
    if (!LoadDlls()) return false;
    // Device names in UTF-8, so they work in any language.
    b_.SetConfig(BASS_CONFIG_UNICODE, TRUE);
    // Keep the factory BASS playback buffer before changing it: it is the
    // floor used in SetBuffer.
    default_play_buffer_ = b_.GetConfig(BASS_CONFIG_BUFFER);
    SetBuffer(BUFFER_DEFAULT);
    b_.Init(-1, 44100, 0, nullptr, nullptr);
    LoadPlaybackPlugins();
    return true;
}

int AudioEngine::SetBuffer(int milliseconds) {
    int ms = std::max(BUFFER_MIN, std::min(BUFFER_MAX, milliseconds));
    buffer_ms_ = ms;
    b_.SetConfig(BASS_CONFIG_REC_BUFFER, ms);
    // The playback buffer never goes below the BASS default. It must be larger
    // than the update period (100 ms), or the test sound breaks up, which is
    // exactly the opposite of what is wanted here.
    b_.SetConfig(BASS_CONFIG_BUFFER, std::max<DWORD>(ms, default_play_buffer_));
    return ms;
}

void AudioEngine::LoadPlaybackPlugins() {
    for (auto& p : PLAYBACK_PLUGINS) {
        std::wstring path = JoinPath(lib_path_, p.second);
        if (!FileExists(path)) continue;
        DWORD handle = b_.PluginLoad(path.c_str(), BASS_UNICODE);
        if (handle) plugins_[p.first] = handle;
    }
}

bool AudioEngine::InitDevice(int dev_idx) {
    if (dev_idx < 0) return true;
    if (!b_.RecordInit(dev_idx) && b_.ErrorGetCode() != BASS_ERROR_ALREADY) return false;
    return true;
}

std::vector<std::pair<int, std::wstring>> AudioEngine::GetInputDevices() {
    std::vector<std::pair<int, std::wstring>> devices;
    BASS_DEVICEINFO info;
    for (DWORD i = 0; b_.RecordGetDeviceInfo(i, &info); ++i) {
        if (!(info.flags & BASS_DEVICE_ENABLED)) continue;
        std::wstring name = Widen(info.name ? info.name : "");
        if (name == L"Default") name = tr("Dispositivo de Entrada Padrão");
        if (info.flags & BASS_DEVICE_LOOPBACK) name += L" (Loopback)";
        devices.emplace_back((int)i, name);
    }
    return devices;
}

void AudioEngine::PlayTone(DWORD frequency, DWORD duration) { Beep(frequency, duration); }

BOOL CALLBACK AudioEngine::MonitorProc(DWORD, const void* buffer, DWORD length, void* user) {
    AudioEngine* self = static_cast<AudioEngine*>(user);
    DWORD stream = self->monitor_stream_.load();
    if (stream && buffer && length > 0) self->b_.StreamPutData(stream, buffer, length);
    return TRUE;
}

bool AudioEngine::StartMonitor(int dev_idx, double volume, int sample_rate, int channels,
                               int* error) {
    StopMonitor();
    if (!InitDevice(dev_idx)) {
        *error = b_.ErrorGetCode();
        return false;
    }
    b_.RecordSetDevice(dev_idx);

    int code = 0;
    const std::pair<int, int> attempts[] = {{sample_rate, channels}, {44100, 2}};
    for (auto& t : attempts) {
        DWORD stream = b_.StreamCreate(t.first, t.second, BASS_SAMPLE_FLOAT, STREAMPROC_PUSH,
                                       nullptr);
        if (!stream) {
            code = b_.ErrorGetCode();
            continue;
        }
        monitor_stream_ = stream;
        SetMonitorVolume(volume);
        DWORD rec = b_.RecordStart(t.first, t.second, BASS_SAMPLE_FLOAT, MonitorProc, this);
        if (rec) {
            monitor_rec_ = rec;
            b_.ChannelPlay(stream, FALSE);
            *error = 0;
            return true;
        }
        code = b_.ErrorGetCode();
        monitor_stream_ = 0;
        b_.StreamFree(stream);
    }
    *error = code;
    return false;
}

void AudioEngine::SetMonitorVolume(double volume) {
    DWORD stream = monitor_stream_.load();
    if (stream) b_.ChannelSetAttribute(stream, BASS_ATTRIB_VOL, (float)volume);
}

void AudioEngine::StopMonitor() {
    if (monitor_rec_) {
        b_.ChannelStop(monitor_rec_);
        monitor_rec_ = 0;
    }
    DWORD stream = monitor_stream_.exchange(0);
    if (stream) {
        b_.ChannelStop(stream);
        b_.StreamFree(stream);
    }
}

bool AudioEngine::IsPlaying() {
    return play_stream_ && b_.ChannelIsActive(play_stream_) != BASS_ACTIVE_STOPPED;
}

std::wstring AudioEngine::MissingPlaybackPlugin(const std::wstring& file_path) {
    std::wstring base, ext;
    SplitExt(file_path, &base, &ext);
    std::wstring fmt = ToLower(ext.empty() ? ext : ext.substr(1));
    for (auto& p : PLAYBACK_PLUGINS)
        if (fmt == p.first && !plugins_.count(p.first)) return p.second;
    return std::wstring();
}

bool AudioEngine::PlayFile(const std::wstring& file_path, int* error) {
    StopPlayback();
    DWORD stream = b_.StreamCreateFile(FALSE, file_path.c_str(), 0, 0, BASS_UNICODE);
    if (!stream) {
        *error = b_.ErrorGetCode();
        return false;
    }
    play_stream_ = stream;
    if (!b_.ChannelPlay(stream, FALSE)) {
        *error = b_.ErrorGetCode();
        StopPlayback();
        return false;
    }
    *error = 0;
    return true;
}

void AudioEngine::StopPlayback() {
    if (play_stream_) {
        DWORD stream = play_stream_;
        play_stream_ = 0;
        b_.ChannelStop(stream);
        b_.StreamFree(stream);
    }
}

DWORD AudioEngine::ApplyEncoder(DWORD handle, const std::wstring& path) {
    // QUEUE moves encoding and disk writing to a thread of the encoder itself:
    // a slow disk, or one that was asleep, does not hold up the mixing.
    const DWORD flags = BASS_ENCODE_AUTOFREE | BASS_ENCODE_QUEUE | BASS_UNICODE;
    const wchar_t* p = path.c_str();
    const wchar_t* o = options_.c_str();
    // Capture is in float; FP_16BIT writes wav and aiff as 16-bit PCM.
    if (format_ == L"wav")
        return b_.EncodeStart(handle, p, flags | BASS_ENCODE_PCM | BASS_ENCODE_FP_16BIT, nullptr,
                              nullptr);
    if (format_ == L"aiff")
        return b_.EncodeStartPCMFile(handle, flags | BASS_ENCODE_AIFF | BASS_ENCODE_FP_16BIT, p);
    if (format_ == L"mp3") return b_.EncodeMP3StartFile(handle, o, flags, p);
    if (format_ == L"ogg") return b_.EncodeOGGStartFile(handle, o, flags, p);
    if (format_ == L"opus") return b_.EncodeOPUSStartFile(handle, o, flags, p);
    if (format_ == L"flac") return b_.EncodeFLACStartFile(handle, o, flags, p);
    return 0;
}

BOOL CALLBACK AudioEngine::RecordProc(DWORD, const void* buffer, DWORD length, void* user) {
    Input* in = static_cast<Input*>(user);
    if (buffer && length > 0)
        in->Put(static_cast<const float*>(buffer), length / sizeof(float), false);
    return TRUE;
}

bool AudioEngine::StartRecording(const std::wstring& file_path, const std::wstring& format_type,
                                 const std::wstring& bitrate, int sample_rate,
                                 const std::wstring& channels, const std::vector<Source>& sources,
                                 bool separate_files, int flac_nivel, int* error) {
    *error = 0;
    if (recording_) return false;
    if (sources.empty()) {
        *error = -1;
        return false;
    }

    Teardown(false);
    output_files_.clear();
    const int ch = channels == L"mono" ? 1 : 2;
    rec_buffer_ms_ = buffer_ms_;
    format_ = format_type;
    options_.clear();
    if (format_type == L"mp3" || format_type == L"ogg") options_ = L"-b " + bitrate;
    else if (format_type == L"opus") options_ = L"--bitrate " + bitrate;
    else if (format_type == L"flac") options_ = L"-" + std::to_wstring(flac_nivel);

    auto fail = [&]() {
        int code = b_.ErrorGetCode();
        Teardown(false);
        *error = code;
        return false;
    };

    // Processes first: activating the capture takes a moment.
    for (auto& src : sources) {
        auto in = std::make_unique<Input>();
        in->engine = this;
        in->is_device = src.type == L"device";
        in->id = src.id;
        in->volume = (float)src.volume;
        if (!in->is_device) {
            in->cap = std::make_unique<ProcessLoopbackCapture>((DWORD)src.id, rec_buffer_ms_);
            in->cap->Start();
        }
        inputs_.push_back(std::move(in));
    }
    for (auto& in : inputs_)
        if (in->cap) in->cap->WaitReady(3000);

    for (auto& in : inputs_) {
        if (in->cap && in->cap->sample_rate()) {
            in->rate = in->cap->sample_rate();
            in->chans = in->cap->channels();
        } else {
            in->rate = sample_rate;
            in->chans = ch;
        }
        in->push = b_.StreamCreate(in->rate, in->chans, BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT,
                                   STREAMPROC_PUSH, nullptr);
        if (!in->push) return fail();
    }

    auto make_output = [&](std::vector<Input*> ins, const std::wstring& path) {
        auto out = std::make_unique<Output>();
        out->rate = sample_rate;
        out->chans = ch;
        out->inputs = ins;
        out->mixer = b_.MixerStreamCreate(sample_rate, ch,
                                          BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE | BASS_MIXER_NONSTOP);
        bool ok = out->mixer != 0;
        for (Input* in : ins) {
            ok = ok && b_.MixerStreamAddChannel(out->mixer, in->push,
                                                BASS_MIXER_CHAN_DOWNMIX | BASS_MIXER_CHAN_NORAMPIN);
        }
        if (ok) out->encoder = ApplyEncoder(out->mixer, path);
        ok = ok && out->encoder != 0;
        outputs_.push_back(std::move(out));
        return ok;
    };

    if (separate_files && sources.size() > 1) {
        std::wstring base, ext;
        SplitExt(file_path, &base, &ext);
        for (size_t i = 0; i < sources.size(); ++i) {
            std::wstring path_i = base + L" (" + SourceSafeName(sources[i].name) + L")" + ext;
            output_files_.push_back(path_i);
            if (!make_output({inputs_[i].get()}, path_i)) return fail();
        }
    } else {
        std::vector<Input*> all;
        for (auto& in : inputs_) all.push_back(in.get());
        output_files_.push_back(file_path);
        if (!make_output(all, file_path)) return fail();
    }

    for (auto& in : inputs_) {
        if (!in->is_device) continue;
        InitDevice(in->id);
        b_.RecordSetDevice(in->id);
        in->rec = b_.RecordStart(sample_rate, ch,
                                 MAKELONG(BASS_RECORD_PAUSE | BASS_SAMPLE_FLOAT, RECORD_PERIOD),
                                 RecordProc, in.get());
        if (!in->rec) return fail();
    }
    for (auto& in : inputs_)
        if (in->cap) in->cap->SetSink(in.get());

    paused_ = false;
    recording_ = true;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    start_ticks_ = now.QuadPart;
    paused_ticks_ = 0;
    SetPowerThrottling(false);
    ResetEvent(pump_stop_);
    pump_thread_ = CreateThread(nullptr, 0, PumpThread, this, 0, nullptr);
    for (auto& in : inputs_)
        if (in->rec) b_.ChannelPlay(in->rec, FALSE);
    return true;
}

void AudioEngine::PauseRecording() {
    if (!recording_ || paused_) return;
    std::lock_guard<std::mutex> lock(clock_mutex_);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    pause_began_ = now.QuadPart;
    paused_ = true;
}

void AudioEngine::ResumeRecording() {
    if (!recording_ || !paused_) return;
    std::lock_guard<std::mutex> lock(clock_mutex_);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    paused_ticks_ += now.QuadPart - pause_began_;
    paused_ = false;
}

double AudioEngine::RecordingSeconds() {
    std::lock_guard<std::mutex> lock(clock_mutex_);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    LONGLONG ticks = (paused_ ? pause_began_ : now.QuadPart) - start_ticks_ - paused_ticks_;
    return (double)ticks / (double)qpc_freq_;
}

DWORD WINAPI AudioEngine::PumpThread(void* self) {
    static_cast<AudioEngine*>(self)->Pump();
    return 0;
}

void AudioEngine::Pump() {
    HANDLE task = EnterAudioThread();
    // A recording in progress keeps the computer from going to sleep.
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
    const double delay = rec_buffer_ms_ / 1000.0;
    while (WaitForSingleObject(pump_stop_, 10) == WAIT_TIMEOUT) {
        if (paused_) continue;
        double t = RecordingSeconds() - delay;
        if (t <= 0) continue;
        for (auto& out : outputs_) {
            long long due = (long long)(t * out->rate);
            if (due > out->frames_done) Advance(*out, due, false);
        }
    }
    SetThreadExecutionState(ES_CONTINUOUS);
    LeaveAudioThread(task);
}

void AudioEngine::Advance(Output& out, long long due, bool final_flush) {
    const long long chunk = std::max(1, out.rate / 4);
    while (out.frames_done < due) {
        long long n = std::min(due - out.frames_done, chunk);
        // The pump may fall up to one full buffer behind the clock waiting for
        // a late source; only after that does what is missing become silence.
        long long lag = due - out.frames_done - n;
        bool may_wait = !final_flush && lag < (long long)rec_buffer_ms_ * out.rate / 1000;
        if (!Feed(out, (DWORD)n, final_flush, may_wait)) break;
    }
}

bool AudioEngine::Feed(Output& out, DWORD frames, bool final_flush, bool may_wait) {
    const double target = rec_buffer_ms_ / 1000.0;
    // First check that every source has the stretch. If one does not yet and
    // there is time to wait, nothing is read now: its audio may be on the way,
    // and reading now would record a hole in its place.
    if (may_wait) {
        for (Input* in : out.inputs) {
            DWORD avail = b_.StreamPutData(in->push, nullptr, 0);
            if (avail == (DWORD)-1) avail = 0;
            DWORD need_frames =
                (DWORD)std::ceil(frames * (double)in->rate * in->factor / out.rate) + 64;
            if (avail < need_frames * in->chans * sizeof(float)) return false;
        }
    }
    for (Input* in : out.inputs) {
        const DWORD frame_bytes = in->chans * sizeof(float);
        DWORD avail = b_.StreamPutData(in->push, nullptr, 0);  // how much is queued
        if (avail == (DWORD)-1) avail = 0;
        // What the mixer will read from this source, with a margin for the resampler.
        DWORD need_frames = (DWORD)std::ceil(frames * (double)in->rate * in->factor / out.rate) + 64;
        DWORD need = need_frames * frame_bytes;
        double level = avail / in->BytesPerSecond();

        if (avail < need) {
            // The source stalled for longer than it was possible to wait (a
            // process that stopped playing, for example): the missing stretch
            // becomes silence, and only that.
            DWORD missing = need - avail;
            std::vector<float>& zeros = scratch_;
            while (missing > 0) {
                DWORD piece = std::min<DWORD>(missing, 65536 * sizeof(float));
                if (zeros.size() * sizeof(float) < piece) zeros.assign(65536, 0.0f);
                else std::fill(zeros.begin(), zeros.begin() + piece / sizeof(float), 0.0f);
                b_.StreamPutData(in->push, zeros.data(), piece);
                missing -= piece;
            }
            in->level_avg = -1.0;
            continue;
        }
        if (final_flush) continue;

        if (level > target * 4 + 1.0) {
            // Queue far above normal: drop the excess at once.
            DWORD excess = (DWORD)((level - target) * in->rate) * frame_bytes;
            std::vector<float> trash(65536);
            while (excess > 0) {
                DWORD piece = std::min<DWORD>(excess, (DWORD)(trash.size() * sizeof(float)));
                b_.ChannelGetData(in->push, trash.data(), piece);
                excess -= piece;
            }
            in->level_avg = -1.0;
            continue;
        }

        // Track the queue and nudge the read speed, to make up for the
        // difference between the clocks.
        in->level_avg = in->level_avg < 0 ? level : in->level_avg + 0.02 * (level - in->level_avg);
        double err = (in->level_avg - target) / target;
        double factor = 1.0 + std::max(-MAX_DRIFT, std::min(MAX_DRIFT, err * MAX_DRIFT));
        if (std::fabs(factor - in->factor) > 0.00001) {
            in->factor = factor;
            b_.ChannelSetAttribute(in->push, BASS_ATTRIB_FREQ, (float)(in->rate * factor));
        }
    }

    size_t samples = (size_t)frames * out.chans;
    if (scratch_.size() < samples) scratch_.resize(samples);
    b_.ChannelGetData(out.mixer, scratch_.data(), (DWORD)(samples * sizeof(float)));
    out.frames_done += frames;
    return true;
}

void AudioEngine::Teardown(bool flush) {
    if (pump_thread_) {
        SetEvent(pump_stop_);
        WaitForSingleObject(pump_thread_, INFINITE);
        CloseHandle(pump_thread_);
        pump_thread_ = nullptr;
    }
    for (auto& in : inputs_) {
        if (in->rec) {
            b_.ChannelStop(in->rec);
            in->rec = 0;
        }
        if (in->cap) {
            in->cap->SetSink(nullptr);
            in->cap->Stop();
        }
    }
    if (flush) {
        // What was still in the slack is recorded too: nothing captured up to
        // the moment of stopping is lost.
        for (auto& out : outputs_) {
            if (!out->encoder) continue;
            double seconds = 0;
            for (Input* in : out->inputs) {
                DWORD avail = b_.StreamPutData(in->push, nullptr, 0);  // how much is queued
                if (avail != (DWORD)-1)
                    seconds = std::max(seconds, avail / in->BytesPerSecond() / in->factor);
            }
            Advance(*out, out->frames_done + (long long)(seconds * out->rate), true);
        }
    }
    for (auto& out : outputs_) {
        if (out->encoder) {
            if (b_.EncodeStopEx) b_.EncodeStopEx(out->encoder, BASS_ENCODE_STOP_WAIT);
            else b_.EncodeStop(out->encoder);
        }
        if (out->mixer) b_.StreamFree(out->mixer);
    }
    for (auto& in : inputs_)
        if (in->push) b_.StreamFree(in->push);
    outputs_.clear();
    inputs_.clear();
    if (recording_) SetPowerThrottling(true);
    recording_ = false;
    paused_ = false;
}

void AudioEngine::StopRecording() { Teardown(recording_); }

void AudioEngine::Cleanup() {
    StopMonitor();
    StopPlayback();
    StopRecording();
    if (b_.Free) b_.Free();
}
