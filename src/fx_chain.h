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
 * The processing of one source, in this order: the noise remover (RNNoise),
 * the equalizer, compressor, pitch shift, chorus and flanger (BASS_FX), the
 * delay (our own), the reverb (BASS_FX, with our own pre-delay), then mono,
 * if the source is not stereo. The source's volume comes after.
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
    /* A decoding push stream carrying BASS_FX effects: what goes in comes
       right back out, processed. */
    struct Stage {
        DWORD stream = 0;
        int count = 0;  // effects on it
    };
    /* Adds or removes one effect; returns its handle, 0 when off. */
    DWORD SetFx(Stage& stage, DWORD handle, bool on, DWORD type, int priority);
    /* The chorus or the flanger: the same BASS_FX effect, set from e. */
    DWORD SetChorus(DWORD handle, const EffectSettings& e, int priority);
    void RunStage(Stage& stage, float* data, DWORD frames);
    void Denoise(float* data, DWORD frames);
    void Delay(float* data, DWORD frames);
    void Reverb(float* data, DWORD frames);

    const BassApi& b_;
    int rate_, chans_;
    std::mutex mutex_;
    Stage pre_;   // equalizer, compressor, pitch shift, chorus, flanger
    Stage post_;  // reverb, after the delay
    DWORD eq_ = 0, compressor_ = 0, pitch_ = 0, chorus_ = 0, flanger_ = 0, reverb_ = 0;
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

    // Delay: one line per channel, as long as the longest time allowed.
    bool delay_on_ = false;
    size_t delay_frames_ = 1;
    float delay_feedback_ = 0, delay_wet_ = 0, delay_dry_ = 1;
    bool delay_pingpong_ = false;
    std::vector<float> delay_line_;  // interleaved, like the audio
    size_t delay_pos_ = 0;

    // Reverb: Freeverb has no pre-delay, so it gets only the effect's part
    // of the sound, from a line that holds it back; the original sound is
    // mixed in afterwards, on time.
    float reverb_dry_ = 1;
    size_t predelay_frames_ = 0;
    std::vector<float> predelay_line_, reverb_in_;
    size_t predelay_pos_ = 0;
};
