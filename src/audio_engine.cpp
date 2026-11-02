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
#include <thread>
#include <cmath>

#include "i18n.h"
#include "m4a_writer.h"
#include "net_source.h"
#include "process_audio.h"
#include "util.h"

namespace {

/* The playback buffer of the Test, in seconds: what it plays sits this
   long at most before it is heard, so a change of volume or effects is heard
   almost at once, whatever the recording's buffer is. */
const float TEST_BUFFER = 0.5f;

/* Subfolder that holds the BASS DLLs. */
const wchar_t* const LIB_DIR = L"lib";

/* Formats BASS only plays with an extra plugin in the program folder. */
const std::pair<const wchar_t*, const wchar_t*> PLAYBACK_PLUGINS[] = {
    {L"flac", L"bassflac.dll"},
    {L"opus", L"bassopus.dll"},
};

/* How far ahead of the clock background audio is read, in seconds. */
const double BACKGROUND_LEAD = 0.05;

/* How often devices deliver audio, in ms. */
const DWORD RECORD_PERIOD = 20;

/* Audio that arrives this much later than the end of its queue, in seconds,
   never existed. It is measured from when the audio was captured, not from
   when it arrived: a capture held up delivers what it kept all together, and
   that is not a gap. The margin covers the unevenness of delivery. */
const double GAP_SECONDS = 0.2;

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

/* A recording source: its queue and whatever fills it.

   The queue keeps track of where its end sits on the recording timeline
   (tail). When audio arrives much later than the tail says it should, the
   stretch in between never happened (a loopback with nothing playing, a device
   that slept, a process that went quiet, or a capture that was still starting):
   it becomes silence, so the new audio lands where it belongs in time and the
   queue keeps its slack. */
struct AudioEngine::Input : public CaptureSink {
    AudioEngine* engine = nullptr;
    Source src;
    bool is_device = false;
    DWORD push = 0;
    DWORD rec = 0;
    std::unique_ptr<ProcessLoopbackCapture> cap;
    std::unique_ptr<BackgroundPlayer> player;  // background audio
    std::unique_ptr<NetSource> net;            // a stream URL
    std::unique_ptr<FxChain> chain;
    int rate = 0;
    int chans = 0;
    std::atomic<float> volume{1.0f};
    std::atomic<float> pan{0.0f};  // -1 left, 0 center, +1 right
    Watch* watch = nullptr;    // the executable this process instance belongs to

    // Changed under the engine's graph lock.
    Output* out = nullptr;
    std::atomic<bool> ending{false};  // removed: its queue drains, then it leaves

    // The queue timeline, guarded by queue_mutex.
    std::mutex queue_mutex;
    double base_sec = 0;             // recording time at the start of the queue
    unsigned long long pushed = 0;   // frames pushed since then
    double lateness_avg = 0;
    bool started = false;
    std::vector<float> scratch;
    std::vector<float> zeros;

    // Used only by the pump.
    double level_avg = 0.0;
    bool level_known = false;
    double factor = 1.0;
    double down_since = -1.0;   // when the capture was found stopped
    double last_retry = -1.0;

    // The side chain, changed by the window while recording; and how far
    // this source is turned down, used only by the thread that fills it.
    std::mutex sidechain_mutex;
    SidechainSettings sidechain;
    float duck_gain = 1.0f;

    double TailSeconds() const { return base_sec + (double)pushed / rate; }

    void SetSidechain(const SidechainSettings& sc) {
        std::lock_guard<std::mutex> lock(sidechain_mutex);
        sidechain = sc;
    }

    /* A source that reduces the others, with sound above its threshold, tells
       the engine until when they stay down; a reduced one goes down until
       then, and comes back after. The audio is at the given recording time. */
    void Sidechain(float* data, DWORD frames, double now) {
        SidechainSettings sc;
        {
            std::lock_guard<std::mutex> lock(sidechain_mutex);
            sc = sidechain;
        }
        if (sc.role == SC_REDUCES) {
            float peak = 0.0f;
            for (DWORD i = 0; i < frames * chans; ++i) peak = std::max(peak, std::fabs(data[i]));
            if (peak > 0.0f && 20.0f * std::log10(peak) >= sc.values[SC_THRESHOLD])
                engine->HoldDuck(now + sc.values[SC_HOLD] / 1000.0);
            return;
        }
        float target = 1.0f;
        if (sc.role == SC_REDUCED && now < engine->duck_until_) {
            float db = sc.values[SC_REDUCTION];
            target = db >= SidechainParams()[SC_REDUCTION].max ? 0.0f : std::pow(10.0f, -db / 20.0f);
        }
        if (target == 1.0f && duck_gain == 1.0f) return;
        // Most of the way there in the time set: three time constants.
        float down = 1.0f - std::exp(-3000.0f / (sc.values[SC_ATTACK] * rate));
        float up = 1.0f - std::exp(-3000.0f / (sc.values[SC_RELEASE] * rate));
        for (DWORD f = 0; f < frames; ++f) {
            duck_gain += (target - duck_gain) * (target < duck_gain ? down : up);
            for (int c = 0; c < chans; ++c) data[f * chans + c] *= duck_gain;
        }
        if (std::fabs(duck_gain - target) < 0.0001f) duck_gain = target;
    }

    /* The balance, applied when the audio is captured, as the volume, so a
       change lands where it was made: toward one side, the other one goes
       down, as BASS's own balance does; at the center nothing changes.
       Every source has the recording's channels, so a microphone too. */
    void Balance(float* data, DWORD frames) {
        float p = pan.load();
        // A mono recording has no sides.
        if (p == 0.0f || chans != 2 || engine->chans_ == 1) return;
        float left = p > 0 ? 1.0f - p : 1.0f, right = p < 0 ? 1.0f + p : 1.0f;
        for (DWORD f = 0; f < frames; ++f) {
            data[f * 2] *= left;
            data[f * 2 + 1] *= right;
        }
    }

    void PushLocked(const float* data, DWORD frames) {
        engine->b_.StreamPutData(push, data, frames * chans * sizeof(float));
        pushed += frames;
    }

    void SilenceLocked(unsigned long long frames) {
        const DWORD chunk = 16384;
        if (zeros.size() < (size_t)chunk * chans) zeros.assign((size_t)chunk * chans, 0.0f);
        while (frames > 0) {
            DWORD n = (DWORD)std::min<unsigned long long>(frames, chunk);
            PushLocked(zeros.data(), n);
            frames -= n;
        }
    }

    /* Silence from the end of the queue up to the given recording time. */
    void FillUntil(double seconds) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        double gap = seconds - TailSeconds();
        if (gap > 0) SilenceLocked((unsigned long long)(gap * rate));
    }

    /* Audio whose last frame was captured at the given recording time. */
    void Put(const float* data, DWORD samples, bool silent, double end) {
        if (engine->paused_ || ending || !push || samples < (DWORD)chans) return;
        DWORD frames = samples / chans;
        samples = frames * chans;
        // Noise remover and equalizer first, then the volume, then the side
        // chain, which goes by the level that is recorded.
        scratch.resize(samples);
        if (silent) std::fill(scratch.begin(), scratch.end(), 0.0f);
        else std::copy(data, data + samples, scratch.begin());
        if (chain) chain->Process(scratch.data(), frames);
        float vol = volume.load();
        if (vol != 1.0f)
            for (DWORD i = 0; i < samples; ++i) scratch[i] *= vol;
        Balance(scratch.data(), frames);
        Sidechain(scratch.data(), frames, end - (double)frames / rate);
        data = scratch.data();
        std::lock_guard<std::mutex> lock(queue_mutex);
        double lateness = end - (TailSeconds() + (double)frames / rate);
        if (!started) {
            // The first audio takes its place on the timeline: whatever came
            // before it (a capture still starting) is silence.
            if (lateness > 0.03) SilenceLocked((unsigned long long)(lateness * rate));
            started = true;
        } else if (lateness - lateness_avg > GAP_SECONDS) {
            // More late than any buffer could hold: that audio does not exist.
            SilenceLocked((unsigned long long)((lateness - lateness_avg) * rate));
        } else {
            // Follows the slow drift between the device clock and the computer.
            lateness_avg += 0.005 * (lateness - lateness_avg);
        }
        PushLocked(data, frames);
    }

    void OnAudio(const float* data, DWORD samples, bool silent, LONGLONG captured) override {
        double end = captured ? engine->RecordingSecondsAt(captured) + (double)(samples / chans) / rate
                              : engine->RecordingSeconds();
        Put(data, samples, silent, end);
    }

    /* Background audio fills its queue up to the given recording time, at
       the recording's own clock: it is never late, and never drifts. */
    void FillBackground(double seconds) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        long long want = (long long)((seconds - TailSeconds()) * rate);
        const DWORD chunk = 4096;
        while (want > 0) {
            DWORD n = (DWORD)std::min<long long>(want, chunk);
            scratch.resize((size_t)n * chans);
            if (player) player->Read(scratch.data(), n);
            else net->Read(scratch.data(), n);
            if (chain) chain->Process(scratch.data(), n);
            float vol = volume.load();
            if (vol != 1.0f)
                for (float& v : scratch) v *= vol;
            Balance(scratch.data(), n);
            Sidechain(scratch.data(), n, TailSeconds());
            PushLocked(scratch.data(), n);
            want -= n;
        }
    }

    double BytesPerSecond() const { return (double)rate * chans * sizeof(float); }
    /* Background audio and stream URLs are read at the recording's clock. */
    bool Clocked() const { return player || net; }
};

/* An output file: the mixer that joins its sources, and the encoder. */
struct AudioEngine::Output {
    DWORD mixer = 0;
    DWORD encoder = 0;
    int rate = 0;
    int chans = 0;
    std::wstring path;
    // A split recording: the path of the whole (its parts are numbered
    // after it) and the part being written, from 1; 0 when not split.
    std::wstring base;
    int part = 0;
    TagValues tags;  // of this file, with the variables as they came out
    std::unique_ptr<M4aWriter> m4a;  // the .m4a encoder, which is our own
    std::vector<Input*> inputs;
    long long frames_done = 0;

    // Separate files: the name of its source. With the source removed, the
    // file keeps receiving silence, and the same source added again comes
    // back to it.
    std::wstring key;

    // A file that joins in the middle of the recording first fills itself up
    // to that moment, so it stays in sync with the others: with silence, or,
    // when a recording is continued, with what the file already had. Only then
    // do its sources join.
    long long join_at = 0;
    std::vector<Input*> pending;
    DWORD old_stream = 0;        // the earlier recording, decoded
    std::wstring old_path;       // where it was set aside meanwhile
};

/* A source by executable name: every main process of that executable, from
   the moment it starts until it exits, as many times as it runs. */
struct AudioEngine::Watch {
    Source src;
    Output* out = nullptr;
    std::map<DWORD, Input*> instances;
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
    // WMA only. Without it the program still records the other formats.
    HMODULE wma = load(L"basswma.dll");
    // The equalizer. Without it the program still records; only the
    // equalizer does nothing.
    if (HMODULE fx = load(L"bass_fx.dll")) {
        auto version = (DWORD(WINAPI*)())(void*)GetProcAddress(fx, "BASS_FX_GetVersion");
        if (version) version();  // registers its effects with BASS
    }

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
    get(bass, "BASS_RecordFree", b_.RecordFree);
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
    get(bass, "BASS_ChannelGetPosition", b_.ChannelGetPosition);
    get(bass, "BASS_ChannelSetPosition", b_.ChannelSetPosition);
    get(bass, "BASS_PluginLoad", b_.PluginLoad);
    get(bass, "BASS_ChannelSetFX", b_.ChannelSetFX);
    get(bass, "BASS_ChannelRemoveFX", b_.ChannelRemoveFX);
    get(bass, "BASS_FXSetParameters", b_.FXSetParameters);
    get(bass, "BASS_StreamCreateURL", b_.StreamCreateURL);
    get(bass, "BASS_ChannelSetDSP", b_.ChannelSetDSP);
    get(bass, "BASS_ChannelGetInfo", b_.ChannelGetInfo);
    get(enc, "BASS_Encode_Start", b_.EncodeStart);
    get(enc, "BASS_Encode_StartPCMFile", b_.EncodeStartPCMFile);
    get(enc, "BASS_Encode_StartPCM", b_.EncodeStartPCM, false);
    get(enc, "BASS_Encode_AddChunk", b_.EncodeAddChunk, false);
    get(enc, "BASS_Encode_Stop", b_.EncodeStop);
    get(enc, "BASS_Encode_StopEx", b_.EncodeStopEx, false);
    get(enc, "BASS_Encode_GetCount", b_.EncodeGetCount, false);
    get(mp3, "BASS_Encode_MP3_StartFile", b_.EncodeMP3StartFile);
    get(ogg, "BASS_Encode_OGG_StartFile", b_.EncodeOGGStartFile);
    get(flac, "BASS_Encode_FLAC_StartFile", b_.EncodeFLACStartFile);
    get(opus, "BASS_Encode_OPUS_StartFile", b_.EncodeOPUSStartFile);
    if (wma) {
        get(wma, "BASS_WMA_EncodeGetRates", b_.WmaEncodeGetRates, false);
        get(wma, "BASS_WMA_EncodeOpenFile", b_.WmaEncodeOpenFile, false);
        get(wma, "BASS_WMA_EncodeClose", b_.WmaEncodeClose, false);
        get(wma, "BASS_WMA_EncodeSetTag", b_.WmaEncodeSetTag, false);
    }
    get(mix, "BASS_Mixer_StreamCreate", b_.MixerStreamCreate);
    get(mix, "BASS_Mixer_StreamAddChannel", b_.MixerStreamAddChannel);
    get(mix, "BASS_Mixer_ChannelRemove", b_.MixerChannelRemove);
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
    // Stream URLs: playlists (.m3u, .pls) are followed, on the internet and
    // on the computer, and HLS (.m3u8) comes with its own add-on.
    b_.SetConfig(BASS_CONFIG_NET_PLAYLIST, 2);
    // AAC and AAC+, which many radios use: Windows decodes them too, but
    // takes up to a minute to recognize a live stream; BASS_AAC, at once.
    for (const wchar_t* name : {L"basshls.dll", L"bass_aac.dll"}) {
        std::wstring path = JoinPath(lib_path_, name);
        if (FileExists(path)) b_.PluginLoad(path.c_str(), BASS_UNICODE);
    }
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

std::vector<DWORD> AudioEngine::WmaBitrates(int sample_rate, int chans, int bits) {
    std::vector<DWORD> rates;
    if (!b_.WmaEncodeGetRates || !b_.WmaEncodeOpenFile) return rates;
    auto key = std::make_tuple(sample_rate, chans, bits);
    auto cached = wma_rates_.find(key);
    if (cached != wma_rates_.end()) return cached->second;
    DWORD codec = bits == 24 ? BASS_WMA_ENCODE_PRO | BASS_WMA_ENCODE_24BIT : BASS_WMA_ENCODE_STANDARD;
    const DWORD* r = b_.WmaEncodeGetRates(sample_rate, chans, codec);
    // Windows lists a few that its file writer then refuses: each one is
    // opened once, as a recording would, into a file deleted right away.
    wchar_t dir[MAX_PATH], path[MAX_PATH];
    bool test = GetTempPathW(MAX_PATH, dir) && GetTempFileNameW(dir, L"vbr", 0, path);
    for (; r && *r; ++r) {
        if (test) {
            DWORD mixer = b_.MixerStreamCreate(sample_rate, chans, BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE);
            DWORD enc = mixer ? b_.WmaEncodeOpenFile(mixer, 0, BASS_WMA_ENCODE_SOURCE | codec | BASS_UNICODE, *r, path) : 0;
            if (enc) b_.WmaEncodeClose(enc);
            if (mixer) b_.StreamFree(mixer);
            if (!enc) continue;
        }
        rates.push_back(*r);
    }
    if (test) DeleteFileW(path);
    std::sort(rates.begin(), rates.end());
    wma_rates_[key] = rates;
    return rates;
}

BOOL CALLBACK AudioEngine::MonitorProc(DWORD, const void* buffer, DWORD length, void* user) {
    EnterCallbackAudioThread();
    AudioEngine* self = static_cast<AudioEngine*>(user);
    DWORD stream = self->monitor_stream_.load();
    if (!stream || !buffer || length == 0) return TRUE;
    FxChain* chain = self->monitor_chain_.get();
    // The test sounds like the recording: captured in two channels, as the
    // sources are, with the same channel picked, equalizer and noise remover;
    // then in one channel if the recording is mono.
    size_t samples = length / sizeof(float);
    DWORD frames = (DWORD)(samples / 2);
    std::vector<float>& buf = self->monitor_buf_;
    buf.assign(static_cast<const float*>(buffer), static_cast<const float*>(buffer) + frames * 2);
    if (chain) chain->Process(buf.data(), frames);
    if (self->monitor_chans_ == 1) {
        for (DWORD f = 0; f < frames; ++f) buf[f] = 0.5f * (buf[f * 2] + buf[f * 2 + 1]);
        self->b_.StreamPutData(stream, buf.data(), frames * sizeof(float));
    } else {
        self->b_.StreamPutData(stream, buf.data(), frames * 2 * sizeof(float));
    }
    return TRUE;
}

bool AudioEngine::StartMonitor(int dev_idx, double volume, int sample_rate, int channels,
                               const FxSettings& fx, int* error) {
    StopMonitor();
    if (!InitDevice(dev_idx)) {
        *error = b_.ErrorGetCode();
        return false;
    }
    b_.RecordSetDevice(dev_idx);
    monitor_dev_ = dev_idx;
    monitor_volume_ = volume;
    monitor_asked_rate_ = sample_rate;
    monitor_asked_chans_ = channels;

    int code = 0;
    // The noise remover needs 48000 Hz.
    int rate = fx.noise.on ? NOISE_RATE : sample_rate;
    const std::pair<int, int> attempts[] = {{rate, channels}, {44100, 2}};
    for (auto& t : attempts) {
        monitor_rate_ = t.first;
        monitor_chans_ = t.second;
        monitor_chain_ = std::make_unique<FxChain>(b_, t.first, 2);
        monitor_chain_->SetMonoOutput(t.second == 1);
        monitor_chain_->Apply(fx);
        DWORD stream = b_.StreamCreate(t.first, t.second, BASS_SAMPLE_FLOAT, STREAMPROC_PUSH,
                                       nullptr);
        if (!stream) {
            code = b_.ErrorGetCode();
            continue;
        }
        monitor_stream_ = stream;
        b_.ChannelSetAttribute(stream, BASS_ATTRIB_BUFFER, TEST_BUFFER);
        SetMonitorVolume(volume);
        SetMonitorPan(monitor_pan_);
        DWORD rec = b_.RecordStart(t.first, 2, BASS_SAMPLE_FLOAT, MonitorProc, this);
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
    monitor_volume_ = volume;
    DWORD stream = monitor_stream_.load();
    if (stream) b_.ChannelSetAttribute(stream, BASS_ATTRIB_VOL, (float)volume);
}

void AudioEngine::SetMonitorPan(double pan) {
    monitor_pan_ = pan;
    DWORD stream = monitor_stream_.load();
    // A mono recording has no sides: neither has its test.
    if (stream) b_.ChannelSetAttribute(stream, BASS_ATTRIB_PAN, monitor_chans_ == 1 ? 0.0f : (float)pan);
}

void CALLBACK AudioEngine::MonitorDsp(DWORD, DWORD, void* buffer, DWORD length, void* user) {
    AudioEngine* self = static_cast<AudioEngine*>(user);
    FxChain* chain = self->monitor_chain_.get();
    if (chain) chain->Process(static_cast<float*>(buffer), length / sizeof(float) / self->monitor_url_chans_);
}

void AudioEngine::StartUrlTest(const std::wstring& url, double volume, int channels, const FxSettings& fx,
                               std::function<void(int)> done) {
    StopMonitor();
    int gen;
    {
        std::lock_guard<std::mutex> lock(monitor_mutex_);
        gen = ++monitor_gen_;
        monitor_url_ = true;
        monitor_volume_ = volume;
        monitor_chans_ = channels;
    }
    std::thread([this, url, fx, gen, done] {
        DWORD s = NetSource::Open(b_, url, BASS_SAMPLE_FLOAT);
        int err = s ? 0 : b_.ErrorGetCode();
        {
            std::lock_guard<std::mutex> lock(monitor_mutex_);
            if (gen != monitor_gen_) {
                if (s) b_.StreamFree(s);
                return;
            }
            if (s) {
                BASS_CHANNELINFO info = {};
                b_.ChannelGetInfo(s, &info);
                monitor_url_chans_ = std::max<DWORD>(1, info.chans);
                monitor_chain_ = std::make_unique<FxChain>(b_, info.freq, monitor_url_chans_);
                monitor_chain_->Apply(fx);
                b_.ChannelSetDSP(s, MonitorDsp, this, 0);
                // The effects are applied as the playback buffer is filled: a
                // short one, so a change is heard at once. The network has a
                // buffer of its own for its unevenness.
                b_.ChannelSetAttribute(s, BASS_ATTRIB_BUFFER, TEST_BUFFER);
                monitor_stream_ = s;
                SetMonitorVolume(monitor_volume_);
                SetMonitorPan(monitor_pan_);
                b_.ChannelPlay(s, FALSE);
            }
        }
        done(err);
    }).detach();
}

void AudioEngine::SetMonitorFx(const FxSettings& fx) {
    if (monitor_url_) {
        std::lock_guard<std::mutex> lock(monitor_mutex_);
        if (monitor_chain_) monitor_chain_->Apply(fx);
        return;
    }
    if (!monitor_rec_ || !monitor_chain_) return;
    // Turning the noise remover on or off may change the capture rate: the
    // test starts again, which takes only a moment.
    int rate = fx.noise.on ? NOISE_RATE : monitor_asked_rate_;
    if (rate != monitor_rate_ && (fx.noise.on || monitor_rate_ == NOISE_RATE)) {
        int err = 0;
        StartMonitor(monitor_dev_, monitor_volume_, monitor_asked_rate_, monitor_asked_chans_, fx,
                     &err);
        return;
    }
    monitor_chain_->Apply(fx);
}

void AudioEngine::StopMonitor() {
    std::lock_guard<std::mutex> lock(monitor_mutex_);
    ++monitor_gen_;  // a test of a URL still connecting is no longer wanted
    monitor_url_ = false;
    if (monitor_rec_) {
        b_.ChannelStop(monitor_rec_);
        monitor_rec_ = 0;
    }
    DWORD stream = monitor_stream_.exchange(0);
    if (stream) {
        b_.ChannelStop(stream);
        b_.StreamFree(stream);
    }
    monitor_chain_.reset();
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

namespace {

/* BASSenc hands the .m4a encoder its 16-bit audio, on the encoder's thread. */
void CALLBACK M4aProc(DWORD, DWORD, const void* buffer, DWORD length, QWORD, void* user) {
    static_cast<M4aWriter*>(user)->Write(buffer, length);
}

}  // namespace

std::wstring AudioEngine::PartPath(const std::wstring& path, int part) {
    if (part <= 0) return path;
    std::wstring stem, ext;
    SplitExt(path, &stem, &ext);
    wchar_t num[16];
    swprintf(num, 16, L" %03d", part);
    return stem + num + ext;
}

DWORD AudioEngine::ApplyEncoder(Output& out) {
    // QUEUE moves encoding and disk writing to a thread of the encoder itself:
    // a slow disk, or one that was asleep, does not hold up the mixing.
    const DWORD flags = BASS_ENCODE_AUTOFREE | BASS_ENCODE_QUEUE | BASS_UNICODE;
    const DWORD handle = out.mixer;
    const wchar_t* p = out.path.c_str();
    // The tags of this file: the name and the part are its own.
    TagContext ctx;
    std::wstring stem, dot_ext;
    SplitExt(BaseName(out.path), &stem, &dot_ext);
    ctx.name = stem;
    ctx.format = format_;
    ctx.part = std::max(1, out.part);
    ctx.start = rec_start_;
    out.tags = ExpandTags(tags_raw_, ctx);
    const std::wstring options = options_ + TagOptions(format_, out.tags);
    const wchar_t* o = options.c_str();
    // Capture is in float; the FP flag converts it to integers of the chosen
    // bit depth in wav, aiff and flac. Without it, wav and aiff stay in float.
    const DWORD depth = bits_ == BITS_FLOAT ? 0
                        : bits_ == 8        ? BASS_ENCODE_FP_8BIT
                        : bits_ == 24       ? BASS_ENCODE_FP_24BIT
                        : bits_ == 32       ? BASS_ENCODE_FP_32BIT
                                            : BASS_ENCODE_FP_16BIT;
    // wav and aiff take their tags as chunks before the audio.
    auto chunks = [&](DWORD enc, const std::vector<std::pair<std::string, std::string>>& list) {
        if (enc && b_.EncodeAddChunk)
            for (auto& c : list) b_.EncodeAddChunk(enc, c.first.c_str(), c.second.data(), (DWORD)c.second.size());
        return enc;
    };
    // A WAV file holds up to 4 GB; past that, RF64 goes on where it would
    // stop. Below that size, it is an ordinary WAV.
    if (format_ == L"wav") {
        DWORD enc = b_.EncodeStart(handle, p, flags | BASS_ENCODE_PCM | BASS_ENCODE_RF64 | depth, nullptr, nullptr);
        std::string info = WavInfoChunk(out.tags);
        if (out.tags.Empty()) return enc;
        // INFO for Windows and most players, and an id3 chunk with all the
        // tags, the album artist included, for the players that read it.
        std::vector<std::pair<std::string, std::string>> list = {{"id3 ", Id3Tag(out.tags)}};
        if (!info.empty()) list.insert(list.begin(), std::make_pair(std::string("LIST"), info));
        return chunks(enc, list);
    }
    if (format_ == L"aiff") return chunks(b_.EncodeStartPCMFile(handle, flags | BASS_ENCODE_AIFF | depth, p), AiffChunks(out.tags));
    if (format_ == L"mp3") return b_.EncodeMP3StartFile(handle, o, flags, p);
    if (format_ == L"ogg") return b_.EncodeOGGStartFile(handle, o, flags, p);
    if (format_ == L"opus") return b_.EncodeOPUSStartFile(handle, o, flags, p);
    if (format_ == L"flac") return b_.EncodeFLACStartFile(handle, o, flags | depth, p);
    // WMA reads the mixer by itself; 24 bits is WMA Pro.
    if (format_ == L"wma" && b_.WmaEncodeOpenFile) {
        DWORD codec = bits_ == 24 ? BASS_WMA_ENCODE_PRO | BASS_WMA_ENCODE_24BIT : BASS_WMA_ENCODE_STANDARD;
        DWORD enc = b_.WmaEncodeOpenFile(handle, 0, BASS_WMA_ENCODE_SOURCE | BASS_WMA_ENCODE_QUEUE | codec | BASS_UNICODE,
                                         wma_bitrate_, p);
        if (enc && b_.WmaEncodeSetTag)
            for (auto& t : WmaTags(out.tags)) b_.WmaEncodeSetTag(enc, t.first, t.second.c_str(), BASS_WMA_TAG_UNICODE);
        return enc;
    }
    // AAC by Windows: BASSenc gives it the audio in 16 bits.
    if (format_ == L"m4a" && b_.EncodeStartPCM) {
        out.m4a = std::make_unique<M4aWriter>();
        if (!out.m4a->Open(out.path, out.rate, out.chans, m4a_kbps_)) {
            out.m4a.reset();
            return 0;
        }
        DWORD enc = b_.EncodeStartPCM(handle, flags | BASS_ENCODE_NOHEAD | BASS_ENCODE_FP_16BIT, M4aProc, out.m4a.get());
        if (!enc) {
            out.m4a->Close(TagValues());
            out.m4a.reset();
            DeleteFileW(out.path.c_str());
        }
        return enc;
    }
    return 0;
}

void AudioEngine::StopEncoder(Output& out) {
    if (out.encoder && format_ == L"wma") {
        b_.WmaEncodeClose(out.encoder);
    } else if (out.encoder) {
        // Everything queued is written before the file is closed.
        if (b_.EncodeStopEx) b_.EncodeStopEx(out.encoder, BASS_ENCODE_STOP_WAIT);
        else b_.EncodeStop(out.encoder);
    }
    out.encoder = 0;
    if (out.m4a) {
        out.m4a->Close(out.tags);
        out.m4a.reset();
    }
}

void AudioEngine::ReadMixer(Output& out, DWORD frames) {
    while (frames > 0) {
        DWORD n = frames;
        if (split_frames_ > 0) {
            long long next = (out.frames_done / split_frames_ + 1) * split_frames_;
            n = (DWORD)std::min<long long>(n, next - out.frames_done);
        }
        size_t samples = (size_t)n * out.chans;
        if (scratch_.size() < samples) scratch_.resize(samples);
        b_.ChannelGetData(out.mixer, scratch_.data(), (DWORD)(samples * sizeof(float)));
        out.frames_done += n;
        frames -= n;
        if (split_frames_ > 0 && out.frames_done % split_frames_ == 0) NextPart(out);
    }
}

/* The part ends at the exact frame and the next one takes the mixer from
   there: no audio is lost or repeated between them. */
void AudioEngine::NextPart(Output& out) {
    StopEncoder(out);
    out.part = (int)(out.frames_done / split_frames_) + 1;
    out.path = PartPath(out.base, out.part);
    out.encoder = ApplyEncoder(out);
    output_files_.push_back(out.path);
}

BOOL CALLBACK AudioEngine::RecordProc(DWORD handle, const void* buffer, DWORD length, void* user) {
    // BASS captures each device on a thread of its own, at a priority that
    // other programs can outrun, a game above all; and audio it cannot
    // fetch in time is lost. So that thread is raised like the others.
    EnterCallbackAudioThread();
    Input* in = static_cast<Input*>(user);
    if (buffer && length > 0) {
        // After a hold-up BASS hands over what it kept all at once, so this
        // audio ends now; whatever it may still hold came later.
        DWORD held = in->engine->b_.ChannelGetData(handle, nullptr, BASS_DATA_AVAILABLE);
        if (held == (DWORD)-1) held = 0;
        double end = in->engine->RecordingSeconds() - held / in->BytesPerSecond();
        in->Put(static_cast<const float*>(buffer), length / sizeof(float), false, end);
    }
    return TRUE;
}

std::unique_ptr<AudioEngine::Input> AudioEngine::MakeInput(const Source& src, int* error) {
    auto in = std::make_unique<Input>();
    in->engine = this;
    in->src = src;
    in->is_device = src.type == L"device";
    in->volume = (float)src.volume;
    in->pan = (float)src.pan;
    in->sidechain = src.fx.sidechain;
    if (src.type == L"url") {
        // It connects on its own thread; meanwhile, and whenever the
        // connection drops, the source is silent.
        in->rate = sample_rate_;
        in->chans = 2;
        in->net = std::make_unique<NetSource>(b_, src.name, in->rate, in->chans);
        in->push = b_.StreamCreate(in->rate, in->chans, BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT,
                                   STREAMPROC_PUSH, nullptr);
        if (!in->push) {
            *error = b_.ErrorGetCode();
            return nullptr;
        }
        in->chain = std::make_unique<FxChain>(b_, in->rate, in->chans);
        in->chain->SetMonoOutput(chans_ == 1);
        in->chain->Apply(src.fx);
        return in;
    }
    if (src.type == L"file") {
        // At the recording's rate and channels, from its start or, when the
        // recording is continued, from where it was.
        in->rate = sample_rate_;
        in->chans = 2;  // as every source, so a channel can be picked
        in->player = std::make_unique<BackgroundPlayer>(b_, src.name, src.loop, src.shuffle,
                                                        src.subfolders, in->rate, in->chans);
        auto place = background_places_.find(ToLower(src.name));
        const BackgroundPlayer::Place* from =
            resuming_ && place != background_places_.end() ? &place->second : nullptr;
        if (!in->player->Open(from, error)) return nullptr;
        in->push = b_.StreamCreate(in->rate, in->chans, BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT,
                                   STREAMPROC_PUSH, nullptr);
        if (!in->push) {
            *error = b_.ErrorGetCode();
            return nullptr;
        }
        in->chain = std::make_unique<FxChain>(b_, in->rate, in->chans);
        in->chain->SetMonoOutput(chans_ == 1);
        in->chain->Apply(src.fx);
        return in;
    }
    if (in->is_device) {
        in->rate = DeviceRate(src);
        // Always two channels, even for a mono recording: the mixer joins
        // them, and the left or right one can be picked before that.
        in->chans = 2;
    } else {
        // The capture starts activating now; its first audio finds its place
        // on the timeline by itself.
        in->cap = std::make_unique<ProcessLoopbackCapture>((DWORD)src.id, rec_buffer_ms_);
        in->cap->Start();
        in->rate = ProcessLoopbackCapture::RATE;
        in->chans = ProcessLoopbackCapture::CHANNELS;
    }
    in->push = b_.StreamCreate(in->rate, in->chans, BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT,
                               STREAMPROC_PUSH, nullptr);
    if (!in->push) {
        *error = b_.ErrorGetCode();
        if (in->cap) in->cap->Stop();
        return nullptr;
    }
    in->chain = std::make_unique<FxChain>(b_, in->rate, in->chans);
    in->chain->SetMonoOutput(chans_ == 1);
        in->chain->Apply(src.fx);
    return in;
}

/* A device is captured at the rate of the recording, or at 48000 Hz with the
   noise remover; the mixer converts it to the recording's rate. */
int AudioEngine::DeviceRate(const Source& src) const {
    return src.fx.noise.on ? NOISE_RATE : sample_rate_;
}

bool AudioEngine::StartDevice(Input* in, bool play) {
    InitDevice(in->src.id);
    b_.RecordSetDevice(in->src.id);
    DWORD flags = BASS_SAMPLE_FLOAT | (play ? 0 : BASS_RECORD_PAUSE);
    in->rec = b_.RecordStart(in->rate, in->chans, MAKELONG(flags, RECORD_PERIOD), RecordProc, in);
    return in->rec != 0;
}

void AudioEngine::StopCapture(Input* in) {
    if (in->rec) {
        b_.ChannelStop(in->rec);
        in->rec = 0;
    }
    if (in->cap) {
        in->cap->SetSink(nullptr);
        in->cap->Stop();
    }
    in->player.reset();
    in->net.reset();
}

void AudioEngine::AttachLive(Input* in, Output* out, double now) {
    // The queue starts where the file is now, and silence fills it up to the
    // present: the source joins exactly in time with the others.
    in->base_sec = (double)out->frames_done / out->rate;
    in->FillUntil(now);
    b_.MixerStreamAddChannel(out->mixer, in->push,
                             BASS_MIXER_CHAN_DOWNMIX | BASS_MIXER_CHAN_NORAMPIN);
    out->inputs.push_back(in);
    in->out = out;
}

void AudioEngine::Place(Input* in, Output* out, double now) {
    if (out->frames_done >= out->join_at) {
        AttachLive(in, out, now);
    } else {
        // The file is still filling its start: the source waits, with its
        // queue starting exactly where the file will join the recording.
        in->base_sec = (double)out->join_at / out->rate;
        in->out = out;
        out->pending.push_back(in);
    }
}

void AudioEngine::Join(Output& out) {
    if (out.old_stream) {
        b_.MixerChannelRemove(out.old_stream);
        b_.StreamFree(out.old_stream);
        out.old_stream = 0;
    }
    for (Input* in : out.pending) {
        b_.MixerStreamAddChannel(out.mixer, in->push,
                                 BASS_MIXER_CHAN_DOWNMIX | BASS_MIXER_CHAN_NORAMPIN);
        out.inputs.push_back(in);
    }
    out.pending.clear();
}

AudioEngine::Output* AudioEngine::MakeOutput(const std::wstring& path, const std::wstring& key,
                                             bool add_to_files, long long start) {
    auto out = std::make_unique<Output>();
    out->rate = sample_rate_;
    out->chans = chans_;
    out->key = key;
    out->base = path;
    // Split: the file of the part that holds the start, which begins at
    // the start of that part.
    if (split_frames_ > 0) {
        out->part = (int)(start / split_frames_) + 1;
        out->frames_done = (long long)(out->part - 1) * split_frames_;
    }
    out->path = PartPath(path, out->part);
    out->mixer = b_.MixerStreamCreate(sample_rate_, chans_,
                                      BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE | BASS_MIXER_NONSTOP);
    if (!out->mixer) return nullptr;
    out->encoder = ApplyEncoder(*out);
    if (!out->encoder) {
        b_.StreamFree(out->mixer);
        return nullptr;
    }
    if (add_to_files) output_files_.push_back(out->path);
    outputs_.push_back(std::move(out));
    return outputs_.back().get();
}

std::wstring AudioEngine::SeparatePath(const std::wstring& key) const {
    return base_path_ + L" (" + key + L")" + ext_;
}

bool AudioEngine::IsLive(const Output* out) const {
    if (!out->pending.empty()) return true;
    for (Input* in : out->inputs)
        if (!in->ending) return true;
    for (auto& w : watches_)
        if (w->out == out) return true;
    return false;
}

AudioEngine::Output* AudioEngine::OutputFor(const Source& src, double now) {
    if (!separate_) return outputs_.empty() ? nullptr : outputs_[0].get();
    // Back to the file of this same source, if it was removed before;
    // otherwise a new file, which starts with silence up to now.
    std::wstring name = src.type == L"exe" ? src.name : SourceSafeName(src.name);
    if (src.type == L"url") {
        // The server's name, or the playlist's file name.
        std::wstring u = src.name;
        size_t scheme = u.find(L"://");
        if (scheme != std::wstring::npos) {
            u = u.substr(scheme + 3);
            u = u.substr(0, u.find_first_of(L"/?:"));
        } else {
            std::wstring ext;
            SplitExt(BaseName(u), &u, &ext);
        }
        name = SourceSafeName(u);
    }
    if (src.type == L"file") {
        // The name of the folder, or of the file without its extension.
        std::wstring stem = BaseName(src.name), ext;
        DWORD attr = GetFileAttributesW(src.name.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
            SplitExt(BaseName(src.name), &stem, &ext);
        name = SourceSafeName(stem);
    }
    std::wstring key = name;
    for (int n = 2;; ++n) {
        auto it = std::find_if(outputs_.begin(), outputs_.end(),
                               [&](const std::unique_ptr<Output>& o) { return o->key == key; });
        if (it == outputs_.end()) break;
        if (!IsLive(it->get())) return it->get();
        key = name + L" " + std::to_wstring(n);
    }
    long long at = (long long)(now * sample_rate_);
    Output* out = MakeOutput(SeparatePath(key), key, true, at);
    if (out) out->join_at = at;
    return out;
}

AudioEngine::Input* AudioEngine::FindInput(const Source& src) {
    for (auto& in : inputs_)
        if (!in->ending && !in->watch && SameSource(in->src, src)) return in.get();
    return nullptr;
}

AudioEngine::Watch* AudioEngine::FindWatch(const Source& src) {
    for (auto& w : watches_)
        if (SameSource(w->src, src)) return w.get();
    return nullptr;
}

bool AudioEngine::PlaceSource(const Source& src, double now, int* error) {
    Output* out = nullptr;
    if (src.type == L"exe") {
        out = OutputFor(src, now);
        if (!out) {
            *error = b_.ErrorGetCode();
            return false;
        }
        // Its processes join as they are found running.
        auto w = std::make_unique<Watch>();
        w->src = src;
        w->out = out;
        watches_.push_back(std::move(w));
        return true;
    }
    auto made = MakeInput(src, error);
    if (!made) return false;
    Input* in = made.get();
    out = OutputFor(src, now);
    if (!out) {
        *error = b_.ErrorGetCode();
        StopCapture(in);
        b_.StreamFree(in->push);
        return false;
    }
    Place(in, out, now);
    if (in->is_device) {
        if (!StartDevice(in, true)) {
            *error = b_.ErrorGetCode();
            DropInput(in);
            return false;
        }
    } else if (in->cap) {
        in->cap->SetSink(in);
    }
    inputs_.push_back(std::move(made));
    return true;
}

void AudioEngine::DropInput(Input* in) {
    // Only for a source that never started: it has nothing to drain.
    if (Output* out = in->out) {
        auto p = std::find(out->pending.begin(), out->pending.end(), in);
        if (p != out->pending.end()) out->pending.erase(p);
        auto it = std::find(out->inputs.begin(), out->inputs.end(), in);
        if (it != out->inputs.end()) {
            b_.MixerChannelRemove(in->push);
            out->inputs.erase(it);
        }
    }
    StopCapture(in);
    b_.StreamFree(in->push);
}

void AudioEngine::EndInput(Input* in) {
    // What is already in its queue is still recorded; then it leaves.
    StopCapture(in);
    {
        std::lock_guard<std::mutex> lock(in->queue_mutex);
        b_.StreamPutData(in->push, nullptr, BASS_STREAMPROC_END);
    }
    in->ending = true;
    if (in->watch) {
        in->watch->instances.erase((DWORD)in->src.id);
        in->watch = nullptr;
    }
}

void AudioEngine::ResetRecording(int sample_rate, const std::wstring& channels) {
    Teardown(false);
    sample_rate_ = sample_rate;
    chans_ = channels == L"mono" ? 1 : 2;
    rec_buffer_ms_ = buffer_ms_;
}

bool AudioEngine::StartRecording(const std::wstring& file_path, const std::wstring& format_type,
                                 const std::wstring& bitrate, int sample_rate, int bits,
                                 const std::wstring& channels, const std::vector<Source>& sources,
                                 bool separate_files, int flac_nivel, int* error) {
    *error = 0;
    if (recording_) return false;
    if (sources.empty()) {
        *error = -1;
        return false;
    }

    ResetRecording(sample_rate, channels);
    output_files_.clear();
    last_outputs_.clear();
    background_places_.clear();
    format_ = format_type;
    bits_ = bits;
    options_.clear();
    if (format_type == L"ogg") options_ = L"-b " + bitrate;
    // At low bitrates LAME would lower the sample rate of the file by itself;
    // it keeps the one chosen.
    if (format_type == L"mp3") {
        wchar_t khz[16];
        swprintf(khz, 16, L"%g", sample_rate / 1000.0);
        options_ = L"-b " + bitrate + L" --resample " + khz;
    }
    else if (format_type == L"opus") options_ = L"--bitrate " + bitrate;
    else if (format_type == L"flac") options_ = L"-" + std::to_wstring(flac_nivel);
    if (format_type == L"m4a") {
        // Windows's AAC encoder: its own rates and bitrates only.
        m4a_kbps_ = _wtoi(bitrate.c_str());
        const auto& rates = M4aWriter::Rates();
        const auto& kbps = M4aWriter::Bitrates();
        if (!M4aWriter::Available() || std::find(rates.begin(), rates.end(), sample_rate) == rates.end() ||
            std::find(kbps.begin(), kbps.end(), m4a_kbps_) == kbps.end()) {
            *error = BASS_ERROR_NOTAVAIL;
            return false;
        }
    }
    if (format_type == L"wma") {
        // The exact bitrate Windows has for the kbps chosen.
        wma_bitrate_ = 0;
        int kbps = _wtoi(bitrate.c_str());
        for (DWORD b : WmaBitrates(sample_rate, channels == L"mono" ? 1 : 2, bits))
            if ((int)(b / 1000) == kbps) wma_bitrate_ = b;
        if (!wma_bitrate_) {
            *error = BASS_ERROR_NOTAVAIL;
            return false;
        }
    }
    // The mode is chosen when recording starts; sources added later follow it.
    separate_ = separate_files && sources.size() > 1;
    SplitExt(file_path, &base_path_, &ext_);
    split_frames_ = (long long)split_seconds_ * sample_rate;
    GetLocalTime(&rec_start_);

    auto fail = [&](int code) {
        Teardown(false);
        *error = code;
        return false;
    };

    paused_ = false;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    start_ticks_ = now.QuadPart;
    duck_until_ = -1.0;
    paused_ticks_ = 0;
    if (!separate_ && !MakeOutput(file_path, L"", true)) return fail(b_.ErrorGetCode());
    for (auto& src : sources) {
        int code = 0;
        if (!PlaceSource(src, 0.0, &code)) return fail(code);
    }

    recording_ = true;
    SetRecordingPower(true);
    ResetEvent(pump_stop_);
    pump_thread_ = CreateThread(nullptr, 0, PumpThread, this, 0, nullptr);
    return true;
}

bool AudioEngine::ContinueRecording(const std::vector<Source>& sources, int* error,
                                    std::wstring* failed_file) {
    *error = 0;
    failed_file->clear();
    if (recording_ || last_outputs_.empty()) return false;
    std::vector<LastOutput> prev = last_outputs_;
    const double t0 = last_end_sec_;
    rec_buffer_ms_ = buffer_ms_;

    // Each file is set aside while it is rewritten: first with what it already
    // had, decoded, and then with the new audio. If anything fails, the files
    // go back as they were.
    std::vector<std::pair<std::wstring, std::wstring>> moved;  // (file, set aside as)
    auto fail = [&](int code, const std::wstring& file) {
        Teardown(false);
        for (auto& m : moved) {
            DeleteFileW(m.first.c_str());
            MoveFileExW(m.second.c_str(), m.first.c_str(), MOVEFILE_REPLACE_EXISTING);
        }
        *error = code;
        *failed_file = file;
        return false;
    };
    const long long join = (long long)(t0 * sample_rate_);
    for (auto& p : prev) {
        const std::wstring& path = p.path;
        std::wstring old = path + L".vbpart";
        bool had = FileExists(path);
        if (had) {
            DeleteFileW(old.c_str());
            if (!MoveFileExW(path.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING))
                return fail((int)GetLastError(), path);
            moved.emplace_back(path, old);
        }
        // A split recording goes on in its last part, which starts where
        // that part starts on the timeline.
        Output* out = MakeOutput(p.base, p.key, false, join);
        if (out && out->path != path) {
            // The last part ended exactly where the next one starts.
            output_files_.push_back(out->path);
        }
        if (!out) return fail(b_.ErrorGetCode(), path);
        out->join_at = join;
        if (had) {
            DWORD s = b_.StreamCreateFile(FALSE, old.c_str(), 0, 0,
                                          BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT | BASS_UNICODE);
            if (!s) return fail(b_.ErrorGetCode(), path);
            b_.MixerStreamAddChannel(out->mixer, s,
                                     BASS_MIXER_CHAN_DOWNMIX | BASS_MIXER_CHAN_NORAMPIN);
            out->old_stream = s;
            out->old_path = old;
        }
    }

    // The recording goes on from where it stopped.
    paused_ = false;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    start_ticks_ = now.QuadPart - (LONGLONG)(t0 * (double)qpc_freq_);
    duck_until_ = -1.0;
    paused_ticks_ = 0;
    resuming_ = true;
    for (auto& src : sources) {
        int code = 0;
        if (!PlaceSource(src, t0, &code)) {
            resuming_ = false;
            return fail(code, L"");
        }
    }
    resuming_ = false;

    recording_ = true;
    SetRecordingPower(true);
    ResetEvent(pump_stop_);
    pump_thread_ = CreateThread(nullptr, 0, PumpThread, this, 0, nullptr);
    return true;
}

bool AudioEngine::AddLiveSource(const Source& src, int* error) {
    *error = 0;
    if (!recording_) return false;
    std::lock_guard<std::mutex> lock(graph_mutex_);
    if (FindInput(src) || FindWatch(src)) return true;
    return PlaceSource(src, RecordingSeconds(), error);
}

void AudioEngine::RemoveLiveSource(const Source& src) {
    if (!recording_) return;
    std::lock_guard<std::mutex> lock(graph_mutex_);
    if (Watch* w = FindWatch(src)) {
        std::vector<Input*> instances;
        for (auto& i : w->instances) instances.push_back(i.second);
        for (Input* in : instances) EndInput(in);
        watches_.erase(std::find_if(watches_.begin(), watches_.end(),
                                    [&](const std::unique_ptr<Watch>& x) { return x.get() == w; }));
        return;
    }
    Input* in = FindInput(src);
    if (!in) return;
    Output* out = in->out;
    if (out && std::find(out->pending.begin(), out->pending.end(), in) != out->pending.end()) {
        // It had not joined its file yet; the file goes on without it.
        DropInput(in);
        inputs_.erase(std::find_if(inputs_.begin(), inputs_.end(),
                                   [&](const std::unique_ptr<Input>& i) { return i.get() == in; }));
        return;
    }
    EndInput(in);
}

void AudioEngine::SetSourceVolume(const Source& src, double volume) {
    std::lock_guard<std::mutex> lock(graph_mutex_);
    if (Watch* w = FindWatch(src)) {
        w->src.volume = volume;
        for (auto& i : w->instances) i.second->volume = (float)volume;
    } else if (Input* in = FindInput(src)) {
        in->volume = (float)volume;
    }
}

void AudioEngine::SetSourcePan(const Source& src, double pan) {
    std::lock_guard<std::mutex> lock(graph_mutex_);
    if (Watch* w = FindWatch(src)) {
        w->src.pan = pan;
        for (auto& i : w->instances) {
            i.second->src.pan = pan;
            i.second->pan = (float)pan;
        }
    } else if (Input* in = FindInput(src)) {
        in->src.pan = pan;
        in->pan = (float)pan;
    }
}

void AudioEngine::SetSourceFx(const Source& src) {
    bool again = false;
    {
        std::lock_guard<std::mutex> lock(graph_mutex_);
        if (Watch* w = FindWatch(src)) {
            w->src.fx = src.fx;
            for (auto& i : w->instances) {
                i.second->src.fx = src.fx;
                i.second->chain->Apply(src.fx);
                i.second->SetSidechain(src.fx.sidechain);
            }
            return;
        }
        Input* in = FindInput(src);
        if (!in) return;
        if (in->is_device && DeviceRate(src) != in->rate) {
            again = true;
        } else {
            in->src.fx = src.fx;
            in->chain->SetMonoOutput(chans_ == 1);
        in->chain->Apply(src.fx);
            in->SetSidechain(src.fx.sidechain);
            if (in->player) {
                in->src.loop = src.loop;
                in->src.shuffle = src.shuffle;
                in->src.subfolders = src.subfolders;
                in->player->SetShuffle(src.shuffle);
                in->player->SetSubfolders(src.subfolders);
                in->player->SetLoop(src.loop);
            }
        }
    }
    // The noise remover changes the capture rate: the device leaves the
    // recording and joins again right away, in sync, in the same file.
    if (again) {
        RemoveLiveSource(src);
        int err = 0;
        AddLiveSource(src, &err);
    }
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

void AudioEngine::HoldDuck(double until) {
    double was = duck_until_.load();
    while (was < until && !duck_until_.compare_exchange_weak(was, until)) {
    }
}

double AudioEngine::RecordingSeconds() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return RecordingSecondsAt(now.QuadPart);
}

double AudioEngine::RecordingSecondsAt(LONGLONG qpc) {
    std::lock_guard<std::mutex> lock(clock_mutex_);
    LONGLONG ticks = (paused_ ? std::min(qpc, pause_began_) : qpc) - start_ticks_ - paused_ticks_;
    return (double)ticks / (double)qpc_freq_;
}

void AudioEngine::SetRecordingPower(bool on) {
    if (on) {
        SetPowerThrottling(false);
        // On computers with Modern Standby, this is what keeps Windows from
        // setting the program aside when the screen turns off by itself. The
        // screen may still turn off.
        REASON_CONTEXT reason = {POWER_REQUEST_CONTEXT_VERSION,
                                 POWER_REQUEST_CONTEXT_SIMPLE_STRING};
        reason.Reason.SimpleReasonString = const_cast<LPWSTR>(L"VB Recorder is recording");
        power_request_ = PowerCreateRequest(&reason);
        if (power_request_ == INVALID_HANDLE_VALUE) power_request_ = nullptr;
        if (power_request_) {
            PowerSetRequest(power_request_, PowerRequestSystemRequired);
            PowerSetRequest(power_request_, (POWER_REQUEST_TYPE)3 /* ExecutionRequired */);
        }
        // A little above other programs, for the threads that are not in the
        // audio scheduler.
        SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
    } else {
        if (power_request_) {
            PowerClearRequest(power_request_, (POWER_REQUEST_TYPE)3);
            PowerClearRequest(power_request_, PowerRequestSystemRequired);
            CloseHandle(power_request_);
            power_request_ = nullptr;
        }
        SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
        SetPowerThrottling(true);
    }
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
    ULONGLONG last_watch = 0, last_scan = 0;
    while (WaitForSingleObject(pump_stop_, 10) == WAIT_TIMEOUT) {
        std::lock_guard<std::mutex> lock(graph_mutex_);
        ULONGLONG tick = GetTickCount64();
        if (tick - last_watch >= 500) {
            Watchdog(tick / 1000.0);
            last_watch = tick;
        }
        if (tick - last_scan >= 1000) {
            ScanWatches();
            last_scan = tick;
        }
        CollectEnded();
        if (paused_) continue;
        double now = RecordingSeconds();
        for (auto& in : inputs_)
            if (in->Clocked() && !in->ending) in->FillBackground(now + BACKGROUND_LEAD);
        double t = now - delay;
        for (auto& out : outputs_) {
            if (out->frames_done < out->join_at) {
                CatchUp(*out);
                continue;
            }
            if (out->old_stream || !out->pending.empty()) Join(*out);
            if (t <= 0) continue;
            long long due = (long long)(t * out->rate);
            if (due > out->frames_done) Advance(*out, due, false, now);
        }
    }
    SetThreadExecutionState(ES_CONTINUOUS);
    LeaveAudioThread(task);
}

void AudioEngine::Watchdog(double wall) {
    // A device that went away (it slept, was unplugged, changed format, or
    // another program took it) and a process capture that ended are opened
    // again. The time they were gone becomes silence.
    for (auto& ptr : inputs_) {
        Input* in = ptr.get();
        // Background audio never goes away; a stream URL connects again by itself.
        if (in->ending || in->Clocked()) continue;
        bool healthy;
        if (in->is_device) {
            healthy = in->rec && b_.ChannelIsActive(in->rec) == BASS_ACTIVE_PLAYING;
        } else {
            healthy = in->cap && !in->cap->Finished();
        }
        if (healthy) {
            in->down_since = -1;
            continue;
        }
        if (in->down_since < 0) in->down_since = wall;
        if (wall - in->down_since < 1.5) continue;
        if (in->last_retry >= 0 && wall - in->last_retry < 2.0) continue;
        in->last_retry = wall;
        if (in->is_device) {
            if (in->rec) {
                b_.ChannelStop(in->rec);
                in->rec = 0;
            }
            if (!StartDevice(in, true)) {
                // The device may have to be opened again from scratch.
                b_.RecordSetDevice(in->src.id);
                b_.RecordFree();
                if (b_.RecordInit(in->src.id)) StartDevice(in, true);
            }
        } else if (!GetProcessName((DWORD)in->src.id).empty()) {
            if (in->cap) in->cap->SetSink(nullptr);
            in->cap = std::make_unique<ProcessLoopbackCapture>((DWORD)in->src.id, rec_buffer_ms_);
            in->cap->SetSink(in);
            in->cap->Start();
        }
    }
}

void AudioEngine::ScanWatches() {
    if (watches_.empty() || !ProcessLoopbackAvailable()) return;
    double now = RecordingSeconds();
    for (auto& w : watches_) {
        // Until its file has caught up, the executable waits too.
        if (w->out->frames_done < w->out->join_at) continue;
        std::vector<DWORD> running = FindMainProcesses(w->src.name);
        // Instances that exited leave, after recording what they had.
        std::vector<Input*> gone;
        for (auto& i : w->instances)
            if (std::find(running.begin(), running.end(), i.first) == running.end())
                gone.push_back(i.second);
        for (Input* in : gone) EndInput(in);
        // New ones join right away.
        for (DWORD pid : running) {
            if (w->instances.count(pid)) continue;
            Source s{L"process", (int)pid, w->src.name, w->src.volume, w->src.fx};
            s.pan = w->src.pan;
            int code = 0;
            auto made = MakeInput(s, &code);
            if (!made) continue;
            Input* in = made.get();
            in->watch = w.get();
            AttachLive(in, w->out, now);
            in->cap->SetSink(in);
            w->instances[pid] = in;
            inputs_.push_back(std::move(made));
        }
    }
}

void AudioEngine::CollectEnded() {
    // A removed source leaves once its queue has been recorded.
    for (size_t i = 0; i < inputs_.size();) {
        Input* in = inputs_[i].get();
        DWORD left = in->ending ? b_.StreamPutData(in->push, nullptr, 0) : 1;
        if (in->ending && (left == 0 || left == (DWORD)-1)) {
            if (Output* out = in->out) {
                auto it = std::find(out->inputs.begin(), out->inputs.end(), in);
                if (it != out->inputs.end()) out->inputs.erase(it);
            }
            b_.MixerChannelRemove(in->push);
            b_.StreamFree(in->push);
            inputs_.erase(inputs_.begin() + i);
        } else {
            ++i;
        }
    }
}

void AudioEngine::CatchUp(Output& out) {
    // The start of a file that joins in the middle (silence, or what it
    // already had when a recording is continued), a little at a time, so the
    // encoder's queue never piles up in memory.
    const double bytes_per_sec = (double)out.rate * out.chans * sizeof(float);
    QWORD queued = b_.EncodeGetCount ? b_.EncodeGetCount(out.encoder, BASS_ENCODE_COUNT_QUEUE) : 0;
    if (queued == (QWORD)-1) queued = 0;
    double room = 4 * bytes_per_sec - (double)queued;
    if (room <= 0) return;
    long long frames = std::min<long long>(out.join_at - out.frames_done, out.rate);
    frames = std::min<long long>(frames, (long long)(room / (out.chans * sizeof(float))));
    if (frames > 0) FeedSilent(out, (DWORD)frames);
}

void AudioEngine::FeedSilent(Output& out, DWORD frames) { ReadMixer(out, frames); }

void AudioEngine::Advance(Output& out, long long due, bool final_flush, double now) {
    const long long chunk = std::max(1, out.rate / 4);
    while (out.frames_done < due) {
        long long n = std::min(due - out.frames_done, chunk);
        // The pump may fall up to one full buffer behind the clock waiting for
        // a late source; only after that does what is missing become silence.
        long long lag = due - out.frames_done - n;
        bool may_wait = !final_flush && lag < (long long)rec_buffer_ms_ * out.rate / 1000;
        if (!Feed(out, (DWORD)n, final_flush, may_wait, now)) break;
    }
}

bool AudioEngine::Feed(Output& out, DWORD frames, bool final_flush, bool may_wait, double now) {
    const double target = rec_buffer_ms_ / 1000.0;
    // First check that every source has the stretch. If one does not yet and
    // there is time to wait, nothing is read now: its audio may be on the way,
    // and reading now would record a hole in its place.
    if (may_wait) {
        for (Input* in : out.inputs) {
            if (in->ending) continue;
            DWORD avail = b_.StreamPutData(in->push, nullptr, 0);
            if (avail == (DWORD)-1) avail = 0;
            DWORD need_frames =
                (DWORD)std::ceil(frames * (double)in->rate * in->factor / out.rate) + 64;
            if (avail < need_frames * in->chans * sizeof(float)) return false;
        }
    }
    // Where the files are on the timeline: how much each queue should hold.
    const double expected = now - (double)out.frames_done / out.rate;
    for (Input* in : out.inputs) {
        if (in->ending) continue;  // its last audio simply runs out
        const DWORD frame_bytes = in->chans * sizeof(float);
        DWORD avail = b_.StreamPutData(in->push, nullptr, 0);  // how much is queued
        if (avail == (DWORD)-1) avail = 0;
        // What the mixer will read from this source, with a margin for the resampler.
        DWORD need_frames = (DWORD)std::ceil(frames * (double)in->rate * in->factor / out.rate) + 64;
        DWORD need = need_frames * frame_bytes;

        if (avail < need) {
            // The source stalled for longer than it was possible to wait (a
            // process that stopped playing, for example): the missing stretch
            // becomes silence, and only that.
            std::lock_guard<std::mutex> lock(in->queue_mutex);
            in->SilenceLocked((need - avail) / frame_bytes);
            in->level_known = false;
            continue;
        }
        // Background audio follows the recording's clock already.
        if (final_flush || in->Clocked()) continue;

        double excess = avail / in->BytesPerSecond() - expected;
        if (excess > target * 3 + 1.0) {
            // Queue far above where the timeline is: drop the excess at once.
            DWORD drop = (DWORD)(excess * in->rate) * frame_bytes;
            std::vector<float> trash(65536);
            while (drop > 0) {
                DWORD piece = std::min<DWORD>(drop, (DWORD)(trash.size() * sizeof(float)));
                b_.ChannelGetData(in->push, trash.data(), piece);
                drop -= piece;
            }
            in->level_known = false;
            continue;
        }

        // Track the queue and nudge the read speed, to make up for the
        // difference between the clocks.
        in->level_avg = in->level_known ? in->level_avg + 0.02 * (excess - in->level_avg) : excess;
        in->level_known = true;
        double err = in->level_avg / target;
        double factor = 1.0 + std::max(-MAX_DRIFT, std::min(MAX_DRIFT, err * MAX_DRIFT));
        if (std::fabs(factor - in->factor) > 0.00001) {
            in->factor = factor;
            b_.ChannelSetAttribute(in->push, BASS_ATTRIB_FREQ, (float)(in->rate * factor));
        }
    }

    ReadMixer(out, frames);
    return true;
}

void AudioEngine::Teardown(bool flush) {
    if (pump_thread_) {
        SetEvent(pump_stop_);
        WaitForSingleObject(pump_thread_, INFINITE);
        CloseHandle(pump_thread_);
        pump_thread_ = nullptr;
    }
    // Background audio stops after the flush: where it was is kept first.
    for (auto& in : inputs_)
        if (!in->Clocked()) StopCapture(in.get());
    if (flush) {
        double now = RecordingSeconds();
        // Files that join in the middle finish their start, and their sources
        // join.
        for (auto& out : outputs_) {
            while (out->frames_done < out->join_at)
                FeedSilent(*out, (DWORD)std::min<long long>(out->join_at - out->frames_done, out->rate));
            Join(*out);
        }
        // What was still in the slack is recorded too: nothing captured up to
        // the moment of stopping is lost.
        long long end = 0;
        for (auto& out : outputs_) {
            double seconds = 0;
            for (Input* in : out->inputs) {
                DWORD avail = b_.StreamPutData(in->push, nullptr, 0);
                if (avail != (DWORD)-1)
                    seconds = std::max(seconds, avail / in->BytesPerSecond() / in->factor);
            }
            Advance(*out, out->frames_done + (long long)(seconds * out->rate), true, now);
            end = std::max(end, out->frames_done);
        }
        // Every file ends at the same point, so they stay in sync, and the
        // recording can be continued from there.
        for (auto& out : outputs_) Advance(*out, end, true, now);
        last_outputs_.clear();
        for (auto& out : outputs_) last_outputs_.push_back(LastOutput{out->path, out->key, out->base, out->part});
        last_end_sec_ = (double)end / sample_rate_;
        background_places_.clear();
        for (auto& in : inputs_)
            if (in->player && !in->ending)
                background_places_[ToLower(in->src.name)] = in->player->Where();
    }
    for (auto& in : inputs_)
        if (in->Clocked()) StopCapture(in.get());
    for (auto& out : outputs_) {
        StopEncoder(*out);
        // Stopped exactly where a part ends: the next one, just opened, has
        // nothing in it.
        if (flush && split_frames_ > 0 && out->part > 1 &&
            out->frames_done == (long long)(out->part - 1) * split_frames_) {
            DeleteFileW(out->path.c_str());
            output_files_.erase(std::remove(output_files_.begin(), output_files_.end(), out->path),
                                output_files_.end());
            for (auto& o : last_outputs_)
                if (o.path == out->path) o.empty = true;
        }
        if (out->old_stream) b_.StreamFree(out->old_stream);
        if (out->mixer) b_.StreamFree(out->mixer);
        // A continued file is complete: the copy set aside is no longer needed.
        if (flush && !out->old_path.empty()) DeleteFileW(out->old_path.c_str());
    }
    for (auto& in : inputs_)
        if (in->push) b_.StreamFree(in->push);
    watches_.clear();
    outputs_.clear();
    inputs_.clear();
    if (recording_) SetRecordingPower(false);
    recording_ = false;
    paused_ = false;
}

void AudioEngine::StopRecording() { Teardown(recording_); }

bool AudioEngine::CanContinue() const {
    if (recording_ || last_outputs_.empty()) return false;
    for (auto& o : last_outputs_)
        if (!o.empty && !FileExists(o.path)) return false;
    return true;
}

void AudioEngine::Cleanup() {
    StopMonitor();
    StopPlayback();
    StopRecording();
    if (b_.Free) b_.Free();
}
