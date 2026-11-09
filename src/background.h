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
   of the folder, sorted as Explorer sorts them; with subfolders, each folder
   inside it comes after the files, in the same order, and so on down. Empty
   if there are none. */
std::vector<std::wstring> BackgroundFiles(const std::wstring& path, bool subfolders);

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

    /* shuffle: a folder plays in a random order, a new one at each repeat.
       subfolders: the folders inside it play too. */
    BackgroundPlayer(const BassApi& bass, const std::wstring& path, bool loop, bool shuffle,
                     bool subfolders, int rate, int chans);
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
    /* The file playing goes on to its end; the new list follows it. */
    void SetSubfolders(bool subfolders);
    /* The crossfade, in seconds, each 0 to leave it out: the next file starts
       `ahead` before the end of the one playing, which goes down over its
       last `fade_out`, while the next one comes up over its first
       `fade_in`. Each works on its own. */
    void SetCrossfade(double ahead, double fade_out, double fade_in);

private:
    /* A file being played, through a mixer of its own that converts it to
       the rate and channels wanted. pos and length are in frames of that
       rate; length is -1 when the file does not say it. */
    struct Voice {
        DWORD mixer = 0, stream = 0;
        size_t index = 0;  // in order_
        long long pos = 0, length = -1;
        bool fade_in = false;  // it came in after another file: it comes up
    };

    bool OpenAt(Voice& v, size_t index, QWORD pos, bool fade_in);
    void CloseVoice(Voice& v);
    /* Reads up to `frames` of the voice, with its fades, adding them to
       out; the frames it had. */
    DWORD ReadVoice(Voice& v, float* out, DWORD frames);
    /* The next file that plays, into v; false when the list is over. */
    bool Next(Voice& v);
    /* A new order for a round: shuffled, without repeating the file just
       played; or the name order. */
    void NewOrder();

    const BassApi& b_;
    std::wstring path_;
    std::vector<std::wstring> files_;
    std::vector<size_t> order_;
    bool loop_, shuffle_, subfolders_;
    int rate_, chans_;
    Voice cur_;   // the file playing
    Voice next_;  // the next one, while the two overlap in a crossfade
    bool last_ = false;  // the file playing has none after it
    size_t index_ = 0;   // in order_: of the newest file playing
    bool done_ = false;
    double ahead_ = 0, fade_out_ = 0, fade_in_ = 0;  // seconds
    std::vector<float> scratch_;
};
