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
 * Convolution reverb: the sound of a real place, recorded as an impulse
 * response (a wav or flac file), put on the source. BASS has none, so it is
 * our own: the response is cut in blocks of BLOCK frames, and each block of
 * sound is multiplied by all of them in the frequency domain (uniformly
 * partitioned overlap-save), so even responses of several seconds cost
 * little. The effect comes out BLOCK frames late; the original sound does
 * not wait.
 */
#pragma once

#include <windows.h>

#include <complex>
#include <string>
#include <vector>

#include "bass_api.h"

/* The longest response used; anything after it is cut. */
const double IMPULSE_MAX_SECONDS = 10.0;

/* Reads an impulse response at the given rate and channels (a mono one goes
   to every channel), interleaved, normalized so the effect is about as loud
   as the sound that goes in. False if the file does not play. */
bool LoadImpulse(const BassApi& bass, const std::wstring& path, int rate, int chans,
                 std::vector<float>* out);

class Convolver {
public:
    static const int BLOCK = 512;

    /* ir: interleaved, frames long, with chans channels. Empty turns it off. */
    void Setup(const std::vector<float>& ir, int chans);
    bool Ready() const { return parts_ > 0; }
    /* frames of interleaved float in; the effect out, the same length. */
    void Process(const float* in, float* wet, DWORD frames);

private:
    typedef std::complex<float> Complex;
    void Fft(Complex* data, bool inverse) const;
    void RunBlock();

    int chans_ = 0;
    size_t parts_ = 0;                 // blocks of the response
    std::vector<Complex> ir_;          // [channel][part][N], the response's spectra
    std::vector<Complex> spectra_;     // [channel][part][N], the last blocks of sound
    size_t newest_ = 0;                // where the newest block of sound is in spectra_
    std::vector<float> prev_, cur_, out_;  // [channel][BLOCK]
    size_t fill_ = 0;
    std::vector<Complex> twiddle_, acc_;
    std::vector<size_t> reverse_;
};
