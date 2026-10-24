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
 * Background audio: a file, or every audio file of a folder in name order
 * or shuffled, played into the recording as one more source. It is read at the pace the
 * recording asks for, converted to its rate and channels; at the end it starts
 * over, or goes silent.
 */
#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "bass_api.h"

/* The files a background source plays: the file itself, or the audio files
   of the folder, sorted as Explorer sorts them. Empty if there are none. */
std::vector<std::wstring> BackgroundFiles(const std::wstring& path);

/* The audio file types looked for in a folder, for the open dialog: "*.mp3;*.wav;...". */
std::wstring BackgroundPattern();

class BackgroundPlayer {
public:
    /* Where the playback is: which file of the order, and how far into it. */
    struct Place {
        size_t index = 0;
        QWORD pos = 0;              // bytes of the decoded file
        std::vector<size_t> order;  // the files, shuffled or not
    };

    /* shuffle: a folder plays in a random order, a new one at each repeat. */
    BackgroundPlayer(const BassApi& bass, const std::wstring& path, bool loop, bool shuffle,
                     int rate, int chans);
    ~BackgroundPlayer();
    BackgroundPlayer(const BackgroundPlayer&) = delete;
    BackgroundPlayer& operator=(const BackgroundPlayer&) = delete;

    /* Opens the first file that plays, from the given place if there is one.
       False, with the BASS error, if none of them plays. */
    bool Open(const Place* from, int* error);
    /* frames of interleaved float; after the end without repeat, silence. */
    void Read(float* out, DWORD frames);
    Place Where() const;
    /* Repeat turned on after the end starts it over from the first file;
       shuffle counts from the next round on. */
    void SetLoop(bool loop);
    void SetShuffle(bool shuffle) { shuffle_ = shuffle; }

private:
    bool OpenAt(size_t index, QWORD pos);
    void Close();
    /* The next file that plays; false when the list is over. */
    bool Next();
    /* A new order for a round: shuffled, without repeating the file just
       played; or the name order. */
    void NewOrder();

    const BassApi& b_;
    std::vector<std::wstring> files_;
    std::vector<size_t> order_;
    bool loop_, shuffle_;
    int rate_, chans_;
    DWORD mixer_ = 0;   // converts each file to the rate and channels wanted
    DWORD stream_ = 0;  // the file being played
    size_t index_ = 0;  // in order_
    bool done_ = false;
};
