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

/* Capturing the audio of a process, through the WASAPI process loopback API. */
#pragma once

#include <windows.h>

#include <atomic>
#include <string>
#include <utility>
#include <vector>

/* Process capture exists from Windows 10 version 2004 on. */
bool ProcessLoopbackAvailable();

/* Processes with an audio session on the default output device: (PID, name).
   The recorder itself never shows up. */
std::vector<std::pair<DWORD, std::wstring>> GetAudioSessions();

/* Every process of the user's session, playing sound or not, sorted by name:
   (PID, name). Services are left out, and so is the recorder itself. */
std::vector<std::pair<DWORD, std::wstring>> GetAllProcesses();

/* Executable name of the process, or empty if it no longer exists. */
std::wstring GetProcessName(DWORD pid);

/* Running processes of an executable (name compared without case), leaving out
   those started by another process of the same executable: capturing a
   process already includes its children, so they would be heard twice. */
std::vector<DWORD> FindMainProcesses(const std::wstring& exe_name);

/* Receives the captured audio, always as interleaved 32-bit float. */
class CaptureSink {
public:
    virtual ~CaptureSink() {}
    virtual void OnAudio(const float* data, DWORD samples, bool silent) = 0;
};

class ProcessLoopbackCapture {
public:
    ProcessLoopbackCapture(DWORD pid, int buffer_ms);
    ~ProcessLoopbackCapture();

    /* The capture always delivers this format; Windows converts to it. */
    static const int RATE = 48000;
    static const int CHANNELS = 2;

    bool Start();
    /* Waits for the capture to start; returns false if it did not start in time. */
    bool WaitReady(DWORD timeout_ms);
    void SetSink(CaptureSink* sink) { sink_.store(sink); }
    void Stop();
    /* The capture ended on its own (the process exited, or its audio device
       went away) or never started. */
    bool Finished() const { return !thread_ || WaitForSingleObject(thread_, 0) == WAIT_OBJECT_0; }

    int sample_rate() const { return sample_rate_; }
    int channels() const { return channels_; }

private:
    static DWORD WINAPI ThreadProc(void* self);
    void Run();
    void Capture();
    struct IAudioClient* Activate();

    DWORD pid_;
    int buffer_ms_;
    std::atomic<int> sample_rate_{0};
    std::atomic<int> channels_{0};
    std::atomic<CaptureSink*> sink_{nullptr};
    HANDLE ready_ = nullptr;
    HANDLE stop_ = nullptr;
    HANDLE thread_ = nullptr;
};
