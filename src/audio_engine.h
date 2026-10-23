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
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "bass_api.h"
#include "config.h"
#include "fx_chain.h"

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    /* Loads the BASS DLLs and starts sound output. Without them, the program
       cannot work. */
    bool Load();

    /* Buffer size, in milliseconds. It applies to capture and to the live
       playback of the Test button, from the next recording or test on. It is
       not a delay: it is the slack that absorbs a hiccup of the computer. */
    int SetBuffer(int milliseconds);

    bool InitDevice(int dev_idx);
    std::vector<std::pair<int, std::wstring>> GetInputDevices();
    void PlayTone(DWORD frequency, DWORD duration);

    /* Live test: captures the device and plays it on the default output. */
    bool IsMonitoring() const { return monitor_rec_ != 0; }
    bool StartMonitor(int dev_idx, double volume, int sample_rate, int channels,
                      const FxSettings& fx, int* error);
    void SetMonitorVolume(double volume);
    /* The equalizer and noise remover of the test, changed live. */
    void SetMonitorFx(const FxSettings& fx);
    void StopMonitor();

    bool IsPlaying();
    /* Name of the DLL needed to play the file, or empty if it can already play. */
    std::wstring MissingPlaybackPlugin(const std::wstring& file_path);
    bool PlayFile(const std::wstring& file_path, int* error);
    void StopPlayback();

    bool StartRecording(const std::wstring& file_path, const std::wstring& format_type,
                        const std::wstring& bitrate, int sample_rate, int bits,
                        const std::wstring& channels, const std::vector<Source>& sources,
                        bool separate_files, int flac_nivel, int* error);
    /* Goes on with the last recording, in the same files, from where it
       stopped. On failure, failed_file names the file that could not be
       reopened (in use by another program, for example), if that was it. */
    bool ContinueRecording(const std::vector<Source>& sources, int* error,
                           std::wstring* failed_file);
    bool CanContinue() const { return !recording_ && !last_outputs_.empty(); }
    /* The last recording was deleted: there is nothing to continue. */
    void ForgetLastRecording() { last_outputs_.clear(); }

    /* Sources added, removed or changed while recording. A new source joins
       in sync; a removed one first records what is already in its queue. */
    bool AddLiveSource(const Source& src, int* error);
    void RemoveLiveSource(const Source& src);
    void SetSourceVolume(const Source& src, double volume);
    /* New equalizer or noise remover for a source being recorded. */
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

private:
    struct Input;
    struct Output;
    struct Watch;

    bool LoadDlls();
    void LoadPlaybackPlugins();
    DWORD ApplyEncoder(DWORD handle, const std::wstring& path);
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
    Output* MakeOutput(const std::wstring& path, const std::wstring& key, bool add_to_files);
    Output* OutputFor(const Source& src, double now);
    bool IsLive(const Output* out) const;
    std::wstring SeparatePath(const std::wstring& key) const;
    Input* FindInput(const Source& src);
    Watch* FindWatch(const Source& src);
    void SetRecordingPower(bool on);

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
    bool Feed(Output& out, DWORD frames, bool final_flush, bool may_wait, double now);

    BassApi b_ = {};
    std::wstring lib_path_;
    std::map<std::wstring, DWORD> plugins_;
    DWORD default_play_buffer_ = 0;
    int buffer_ms_ = BUFFER_DEFAULT;

    DWORD monitor_rec_ = 0;
    std::atomic<DWORD> monitor_stream_{0};
    std::unique_ptr<FxChain> monitor_chain_;
    std::vector<float> monitor_buf_;  // used by the capture thread of the test
    int monitor_dev_ = -1, monitor_rate_ = 0, monitor_chans_ = 0;
    int monitor_asked_rate_ = 0, monitor_asked_chans_ = 0;
    double monitor_volume_ = 1.0;
    DWORD play_stream_ = 0;

    // Recording in progress.
    std::atomic<bool> recording_{false};
    std::atomic<bool> paused_{false};
    std::vector<std::wstring> output_files_;
    std::vector<std::unique_ptr<Input>> inputs_;
    std::vector<std::unique_ptr<Output>> outputs_;
    std::vector<std::unique_ptr<Watch>> watches_;
    // The files of the last recording and where it stopped, to continue it.
    std::vector<std::pair<std::wstring, std::wstring>> last_outputs_;  // (path, key)
    double last_end_sec_ = 0;
    std::wstring format_, options_;
    int bits_ = 16;  // bit depth of wav, aiff and flac
    std::wstring base_path_, ext_;
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
    std::vector<float> scratch_;
};
