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
 * AAC in an .m4a file, with the encoder that comes with Windows (Media
 * Foundation): its patents are covered by Windows itself, and no library has
 * to come with the program. It takes 16-bit audio at 44100 or 48000 Hz, mono
 * or stereo, at 96, 128, 160 or 192 kbps. The N editions of Windows have no
 * Media Foundation without the Media Feature Pack: there, Available() is
 * false, and Media Foundation is only loaded when it is used, so the program
 * still starts.
 */
#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "tags.h"

struct IMFSinkWriter;

class M4aWriter {
public:
    M4aWriter() = default;
    ~M4aWriter();
    M4aWriter(const M4aWriter&) = delete;
    M4aWriter& operator=(const M4aWriter&) = delete;

    /* Whether Windows has its AAC encoder. */
    static bool Available();
    static const std::vector<int>& Bitrates();  // kbps
    static const std::vector<int>& Rates();     // Hz

    bool Open(const std::wstring& path, int rate, int chans, int kbps);
    /* 16-bit interleaved audio, in any amount. From any thread. */
    void Write(const void* data, DWORD bytes);
    /* Ends the file; the tags, if any, go into it after that. */
    bool Close(const TagValues& tags);

private:
    void Flush(bool all);

    IMFSinkWriter* writer_ = nullptr;
    DWORD stream_ = 0;
    int rate_ = 0, chans_ = 0;
    long long frames_ = 0;  // written, for the time of each sample
    std::vector<BYTE> pending_;
    std::wstring path_;
    CRITICAL_SECTION lock_;
    bool lock_made_ = false;
};

/* Puts iTunes-style tags into an .m4a file Media Foundation wrote, whose
   index (moov) is at its end. False if the file is not like that. */
bool WriteM4aTags(const std::wstring& path, const TagValues& tags);
