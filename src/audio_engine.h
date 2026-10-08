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
    bool StartMonitor(int dev_idx, double volume, int sample_rate, int channels, int* error);
    void SetMonitorVolume(double volume);
    void StopMonitor();

    bool IsPlaying();
    /* Name of the DLL needed to play the file, or empty if it can already play. */
    std::wstring MissingPlaybackPlugin(const std::wstring& file_path);
    bool PlayFile(const std::wstring& file_path, int* error);
    void StopPlayback();

    bool StartRecording(const std::wstring& file_path, const std::wstring& format_type,
                        const std::wstring& bitrate, int sample_rate,
                        const std::wstring& channels, const std::vector<Source>& sources,
                        bool separate_files, int flac_nivel, int* error);
    void PauseRecording();
    void ResumeRecording();
    void StopRecording();
    void Cleanup();

    bool is_recording() const { return recording_; }
    bool is_paused() const { return paused_; }
    const std::vector<std::wstring>& output_files() const { return output_files_; }

private:
    struct Input;
    struct Output;

    bool LoadDlls();
    void LoadPlaybackPlugins();
    DWORD ApplyEncoder(DWORD handle, const std::wstring& path);
    void Teardown(bool flush);

    static BOOL CALLBACK RecordProc(DWORD handle, const void* buffer, DWORD length, void* user);
    static BOOL CALLBACK MonitorProc(DWORD handle, const void* buffer, DWORD length, void* user);
    static DWORD WINAPI PumpThread(void* self);
    void Pump();
    double RecordingSeconds();
    void Advance(Output& out, long long due, bool final_flush);
    bool Feed(Output& out, DWORD frames, bool final_flush, bool may_wait);

    BassApi b_ = {};
    std::wstring lib_path_;
    std::map<std::wstring, DWORD> plugins_;
    DWORD default_play_buffer_ = 0;
    int buffer_ms_ = BUFFER_DEFAULT;

    DWORD monitor_rec_ = 0;
    std::atomic<DWORD> monitor_stream_{0};
    DWORD play_stream_ = 0;

    // Recording in progress.
    std::atomic<bool> recording_{false};
    std::atomic<bool> paused_{false};
    std::vector<std::wstring> output_files_;
    std::vector<std::unique_ptr<Input>> inputs_;
    std::vector<std::unique_ptr<Output>> outputs_;
    std::wstring format_, options_;
    int rec_buffer_ms_ = BUFFER_DEFAULT;
    HANDLE pump_thread_ = nullptr;
    HANDLE pump_stop_ = nullptr;
    std::mutex clock_mutex_;
    LONGLONG qpc_freq_ = 1;
    LONGLONG start_ticks_ = 0;
    LONGLONG paused_ticks_ = 0;
    LONGLONG pause_began_ = 0;
    std::vector<float> scratch_;
};
