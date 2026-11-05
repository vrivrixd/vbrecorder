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

/* Recording, mixing, encoding and playback, through BASS. */
#pragma once

#include <windows.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "background.h"
#include "bass_api.h"
#include "config.h"
#include "fx_chain.h"
#include "tags.h"

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    /* Loads the BASS DLLs and starts sound output. Without them, the program
       cannot work. */
    bool Load();

    /* Buffer size, in milliseconds. It applies to capture, from the next
       recording on; the Test button plays with a short buffer of its own,
       so changes are heard at once. It is not a delay: it is the slack
       that absorbs a hiccup of the computer. */
    int SetBuffer(int milliseconds);

    bool InitDevice(int dev_idx);
    /* The WMA bitrates Windows has for a sample rate and channel count, in
       bits per second, from the lowest; 24 bits is WMA Pro. Empty when
       there is no WMA. */
    std::vector<DWORD> WmaBitrates(int sample_rate, int chans, int bits);
    std::vector<std::pair<int, std::wstring>> GetInputDevices();
    void PlayTone(DWORD frequency, DWORD duration);

    /* Live test: captures the device and plays it on the default output. */
    bool IsMonitoring() const { return monitor_rec_ != 0; }
    bool StartMonitor(int dev_idx, double volume, int sample_rate, int channels,
                      const FxSettings& fx, int* error);
    void SetMonitorVolume(double volume);
    /* The balance of the test, kept for the next one too. */
    void SetMonitorPan(double pan);
    /* The test of a stream URL: it plays with the volume, balance and
       effects given. Connecting takes a moment, so it is done on another
       thread, which then calls done with 0, or the BASS error; done is not
       called if the test was stopped meanwhile. channels is the
       recording's, for the balance. */
    void StartUrlTest(const std::wstring& url, double volume, int channels, const FxSettings& fx,
                      std::function<void(int)> done);
    /* The equalizer and noise remover of the test, changed live. */
    void SetMonitorFx(const FxSettings& fx);
    void StopMonitor();

    bool IsPlaying();
    /* Name of the DLL needed to play the file, or empty if it can already play. */
    std::wstring MissingPlaybackPlugin(const std::wstring& file_path);
    bool PlayFile(const std::wstring& file_path, int* error);
    void StopPlayback();

    /* The tags, as typed, with their variables, and in how many minutes the
       recording is split into files (0: never). For the next recording. */
    void SetTags(const TagValues& raw) { tags_raw_ = raw; }
    void SetSplitSeconds(int seconds) { split_seconds_ = seconds > 0 ? seconds : 0; }
    /* A part of a split recording: "name 001.mp3" for part 1 of name.mp3;
       part 0 is the path itself. */
    static std::wstring PartPath(const std::wstring& path, int part);

    /* Waiting for a recording: the sources, background audio aside, are
       captured all the time and their last seconds are kept, so that a
       recording can start from a moment before Record was pressed (the
       pre-recording, or the silence detection's). Called again whenever
       they change, it starts over only if the capture itself must change;
       no sources or 0 seconds stops it. Nothing goes to any file. */
    void SetStandby(const std::vector<Source>& sources, int sample_rate, const std::wstring& channels,
                    double seconds);
    void StopStandby();

    /* Sound for the silence detection: audio at or above this level, in
       dBFS, after the volume, in any source but background audio. -200 or
       less: none is looked for. */
    void SetSoundThreshold(double db);
    /* When the last sound was captured (QueryPerformanceCounter); 0 if never. */
    LONGLONG LastSound() const { return last_sound_.load(); }
    /* When the first sound after WatchForSound began; 0 if none yet. */
    void WatchForSound();
    LONGLONG FirstSound() const { return first_sound_.load(); }
    /* Holds the recording: its files stop where it is now, as in a pause,
       while the sources go on being captured, keeping the last preroll
       seconds. Release goes on from the moment given, if it was captured,
       as if the time between had not happened (0: from now). */
    void Hold(double preroll);
    void Release(LONGLONG from);
    bool is_held() const { return recording_ && holding_; }
    /* How long the files are, without the pauses and holds. */
    double RecordedSeconds();

    /* from: the moment (QueryPerformanceCounter) it starts at, captured
       while waiting; 0 starts now. */
    bool StartRecording(const std::wstring& file_path, const std::wstring& format_type,
                        const std::wstring& bitrate, int sample_rate, int bits,
                        const std::wstring& channels, const std::vector<Source>& sources,
                        bool separate_files, int flac_nivel, int* error, LONGLONG from = 0);
    /* Goes on with the last recording, in the same files, from where it
       stopped, with what was captured since the moment from, as above. On
       failure, failed_file names the file that could not be reopened (in use
       by another program, for example), if that was it. */
    bool ContinueRecording(const std::vector<Source>& sources, int* error,
                           std::wstring* failed_file, LONGLONG from = 0);
    /* Whether the last recording can go on: its files must all be there
       still; one deleted meanwhile would come back as silence. */
    bool CanContinue() const;
    /* The last recording was deleted: there is nothing to continue. */
    void ForgetLastRecording() { last_outputs_.clear(); }

    /* Sources added, removed or changed while recording. A new source joins
       in sync; a removed one first records what is already in its queue. */
    bool AddLiveSource(const Source& src, int* error);
    void RemoveLiveSource(const Source& src);
    void SetSourceVolume(const Source& src, double volume);
    /* The balance: -1 left, 0 center, +1 right. */
    void SetSourcePan(const Source& src, double pan);
    /* New equalizer or noise remover for a source being recorded, and the
       repeat, shuffle and subfolders of a background audio. */
    void SetSourceFx(const Source& src);

    void PauseRecording();
    void ResumeRecording();
    void StopRecording();
    void Cleanup();

    bool is_recording() const { return recording_; }
    bool is_paused() const { return paused_; }
    const std::vector<std::wstring>& output_files() const { return output_files_; }
    /* Where the recording is on its timeline, in seconds, without the pauses. */
    double RecordingSeconds();
    /* The same, for a moment given by QueryPerformanceCounter. */
    double RecordingSecondsAt(LONGLONG qpc);

private:
    struct Input;
    struct Output;
    struct Watch;

    bool LoadDlls();
    void LoadPlaybackPlugins();
    /* Starts the encoder of out's file, at out.path, with its tags. */
    DWORD ApplyEncoder(Output& out);
    void StopEncoder(Output& out);
    /* Reads the mixer into the encoder; a split recording goes on in the
       next part's file at the exact frame. */
    void ReadMixer(Output& out, DWORD frames);
    void NextPart(Output& out);
    void Teardown(bool flush);
    void ResetRecording(int sample_rate, const std::wstring& channels);
    std::unique_ptr<Input> MakeInput(const Source& src, int* error);
    bool StartDevice(Input* in, bool play);
    int DeviceRate(const Source& src) const;
    void StopCapture(Input* in);
    void AttachLive(Input* in, Output* out, double now);
    void Place(Input* in, Output* out, double now);
    void Join(Output& out);
    bool PlaceSource(const Source& src, double now, int* error);
    void DropInput(Input* in);
    void EndInput(Input* in);
    /* A file, from a frame of the timeline: with the recording split, the
       part that holds that frame. */
    Output* MakeOutput(const std::wstring& path, const std::wstring& key, bool add_to_files,
                       long long start = 0);
    Output* OutputFor(const Source& src, double now);
    bool IsLive(const Output* out) const;
    std::wstring SeparatePath(const std::wstring& key) const;
    Input* FindInput(const Source& src);
    Watch* FindWatch(const Source& src);
    void SetRecordingPower(bool on);
    /* The side chain: the reduced sources stay down until then, at least. */
    void HoldDuck(double until);
    void StartPump();
    void StopPump();
    void CheckSound(const float* data, DWORD frames, int chans, int rate, LONGLONG end);
    /* Waiting or held: drops what is older than the seconds kept. */
    void KeepHeld(double now);
    /* The earliest recording time every queue has audio from. */
    double HeldStart();
    /* Drops what the queues have between keep_until and from, and moves the
       timeline so that from becomes target. */
    void Rebase(double keep_until, double from, double target);
    /* The recording starts at a moment captured while waiting, which
       becomes target on its timeline. */
    void StartFrom(LONGLONG from, double target);
    /* The sources captured while waiting join their files. */
    bool AdoptStandby(const std::vector<Source>& sources, double target, int* error);
    void Adopt(Input* in, Output* out);

    static BOOL CALLBACK RecordProc(DWORD handle, const void* buffer, DWORD length, void* user);
    static BOOL CALLBACK MonitorProc(DWORD handle, const void* buffer, DWORD length, void* user);
    static DWORD WINAPI PumpThread(void* self);
    void Pump();
    void Watchdog(double wall);
    void ScanWatches();
    void CollectEnded();
    void CatchUp(Output& out);
    void FeedSilent(Output& out, DWORD frames);
    void Advance(Output& out, long long due, bool final_flush, double now);
    bool QueueFull(Output& out);
    bool Feed(Output& out, DWORD frames, bool final_flush, bool may_wait, double now);

    BassApi b_ = {};
    std::wstring lib_path_;
    std::map<std::wstring, DWORD> plugins_;
    DWORD default_play_buffer_ = 0;
    int buffer_ms_ = BUFFER_DEFAULT;

    DWORD monitor_rec_ = 0;
    std::atomic<DWORD> monitor_stream_{0};
    std::unique_ptr<FxChain> monitor_chain_;
    // The test of a stream URL: its effects run on its playback, and each
    // test started or stopped has a new number, so a connection that
    // arrives late knows it is no longer wanted.
    bool monitor_url_ = false;
    DWORD monitor_url_chans_ = 2;
    int monitor_gen_ = 0;
    std::mutex monitor_mutex_;
    static void CALLBACK MonitorDsp(DWORD handle, DWORD channel, void* buffer, DWORD length, void* user);
    std::vector<float> monitor_buf_;  // used by the capture thread of the test
    int monitor_dev_ = -1, monitor_rate_ = 0, monitor_chans_ = 0;
    int monitor_asked_rate_ = 0, monitor_asked_chans_ = 0;
    double monitor_volume_ = 1.0;
    double monitor_pan_ = 0.0;
    DWORD play_stream_ = 0;

    // Recording in progress.
    std::atomic<bool> recording_{false};
    std::atomic<bool> paused_{false};
    std::vector<std::wstring> output_files_;
    std::vector<std::unique_ptr<Input>> inputs_;
    std::vector<std::unique_ptr<Output>> outputs_;
    std::vector<std::unique_ptr<Watch>> watches_;
    // The files of the last recording and where it stopped, to continue it:
    // its last part, when it was split.
    struct LastOutput {
        std::wstring path, key, base;
        int part = 0;
        bool empty = false;  // a part that starts where it stopped: it has no file yet
    };
    std::vector<LastOutput> last_outputs_;
    double last_end_sec_ = 0;
    // Where each background audio was when the last recording stopped, so
    // continuing it goes on from there too; by path, in lower case.
    std::map<std::wstring, BackgroundPlayer::Place> background_places_;
    bool resuming_ = false;  // the sources being placed continue a recording
    std::wstring format_, options_;
    int bits_ = 16;  // bit depth of wav, aiff, flac and wma, or BITS_FLOAT
    DWORD wma_bitrate_ = 0;  // bits per second
    // The WMA bitrates that open, by (rate, channels, bits); found once.
    std::map<std::tuple<int, int, int>, std::vector<DWORD>> wma_rates_;
    std::wstring base_path_, ext_;
    TagValues tags_raw_;
    SYSTEMTIME rec_start_ = {};  // for the variables of the tags
    int split_seconds_ = 0;
    long long split_frames_ = 0;  // of the recording in progress; 0 when not split
    int m4a_kbps_ = 0;
    int sample_rate_ = 44100;
    int chans_ = 2;
    bool separate_ = false;
    int rec_buffer_ms_ = BUFFER_DEFAULT;
    // Guards the lists of sources and files, shared by the pump and the
    // window (sources added or removed while recording).
    std::mutex graph_mutex_;
    HANDLE power_request_ = nullptr;
    HANDLE pump_thread_ = nullptr;
    HANDLE pump_stop_ = nullptr;
    std::mutex clock_mutex_;
    LONGLONG qpc_freq_ = 1;
    LONGLONG start_ticks_ = 0;
    LONGLONG paused_ticks_ = 0;
    LONGLONG pause_began_ = 0;
    // Recording time until which the side chain keeps the reduced sources down.
    std::atomic<double> duck_until_{-1.0};
    std::vector<float> scratch_;

    // Waiting for a recording, and holding one.
    bool standby_ = false;
    std::wstring standby_key_;
    double standby_keep_ = 0;  // seconds kept in each queue
    std::atomic<bool> holding_{false};
    double hold_sec_ = 0;  // where the files wait
    std::atomic<float> sound_level_{0.0f};  // linear; 0: not looked for
    std::atomic<LONGLONG> last_sound_{0};
    std::atomic<LONGLONG> first_sound_{0};
    // The format of the last recording, to continue it.
    int last_rate_ = 44100, last_chans_ = 2;
};
