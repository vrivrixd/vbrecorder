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
 * The processing of one source: the noise remover (RNNoise), the equalizer,
 * then mono, if the source is not stereo. The source's volume comes after.
 *
 * The audio goes through as it is captured, in place and with the same
 * length, so the source keeps its place on the recording timeline. The
 * settings can change at any moment, even while the audio flows.
 */
#pragma once

#include <windows.h>

#include <mutex>
#include <vector>

#include "bass_api.h"
#include "fx.h"

struct DenoiseState;

/* The noise remover works on 48000 Hz audio only. */
const int NOISE_RATE = 48000;

class FxChain {
public:
    FxChain(const BassApi& bass, int rate, int chans);
    ~FxChain();
    FxChain(const FxChain&) = delete;
    FxChain& operator=(const FxChain&) = delete;

    void Apply(const FxSettings& fx);
    /* frames of interleaved float, changed in place. */
    void Process(float* data, DWORD frames);

private:
    void Denoise(float* data, DWORD frames);

    const BassApi& b_;
    int rate_, chans_;
    std::mutex mutex_;
    DWORD stream_ = 0;  // decoding push stream carrying the BASS_FX equalizer
    DWORD eq_ = 0;
    float preamp_ = 1.0f;
    bool stereo_ = true;

    // Noise remover: one RNNoise per channel, in frames of 480 samples. The
    // sound comes out one frame later (10 ms), the cleaned and the original
    // alike, so they can be blended.
    bool noise_on_ = false;
    float noise_amount_ = 1.0f;
    std::vector<DenoiseState*> denoise_;
    std::vector<float> in_frames_, out_frames_, dry_frames_;
    size_t fill_ = 0;
};
