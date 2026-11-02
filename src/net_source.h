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
 * A stream URL as a source: an internet radio, or a playlist (.m3u, .pls,
 * .m3u8) on the internet or on the computer. BASS connects to it on a thread
 * of this class, so the recording never waits for the network; while there
 * is no connection, the source is silent, and a connection that drops is
 * made again by itself. The sound comes out at the recording's rate and
 * channels, read at the recording's own clock, as background audio is.
 */
#pragma once

#include <windows.h>

#include <memory>
#include <string>

#include "bass_api.h"

class NetSource {
public:
    NetSource(const BassApi& bass, const std::wstring& url, int rate, int chans);
    /* Returns at once, even while BASS is still connecting. */
    ~NetSource();
    NetSource(const NetSource&) = delete;
    NetSource& operator=(const NetSource&) = delete;

    /* frames of interleaved float: the stream, or silence while there is none. */
    void Read(float* out, DWORD frames);

    /* Opens the URL, or the playlist file, as BASS does; the flags are those
       of BASS_StreamCreateURL. Blocks until connected or failed. */
    static DWORD Open(const BassApi& bass, const std::wstring& url, DWORD flags);

private:
    struct State;
    static void Run(std::shared_ptr<State> state);

    int chans_ = 2;
    std::shared_ptr<State> state_;
};
