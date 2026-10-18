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

#include "fx_chain.h"

#include <algorithm>
#include <cmath>

#include "rnnoise.h"

namespace {

const int NOISE_FRAME = 480;  // 10 ms at 48000 Hz, what RNNoise takes at a time
const int EQ_PRIORITY = 100;

}  // namespace

FxChain::FxChain(const BassApi& bass, int rate, int chans) : b_(bass), rate_(rate), chans_(chans) {}

FxChain::~FxChain() {
    if (stream_) b_.StreamFree(stream_);  // the equalizer goes with it
    for (DenoiseState* st : denoise_) rnnoise_destroy(st);
}

void FxChain::Apply(const FxSettings& fx) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Noise remover.
    bool noise = fx.noise.on && rate_ == NOISE_RATE;
    if (noise && !noise_on_) {
        // Starts clean, so no sound left from an earlier use comes back.
        for (DenoiseState* st : denoise_) rnnoise_destroy(st);
        denoise_.clear();
        for (int c = 0; c < chans_; ++c) denoise_.push_back(rnnoise_create(nullptr));
        in_frames_.assign((size_t)NOISE_FRAME * chans_, 0.0f);
        out_frames_.assign((size_t)NOISE_FRAME * chans_, 0.0f);
        dry_frames_.assign((size_t)NOISE_FRAME * chans_, 0.0f);
        fill_ = 0;
    }
    noise_on_ = noise;
    noise_amount_ = std::max(0.0f, std::min(100.0f, fx.noise.amount)) / 100.0f;
    stereo_ = fx.stereo;

    // The BASS_FX equalizer lives on a stream of its own, made the first
    // time it is needed.
    if (!fx.eq.IsFlat() && !stream_ && b_.ChannelSetFX) {
        stream_ = b_.StreamCreate(rate_, chans_, BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT,
                                  STREAMPROC_PUSH, nullptr);
    }
    preamp_ = (float)std::pow(10.0, fx.eq.preamp / 20.0);
    if (!stream_) return;

    // Equalizer: one peaking filter per band, an octave wide.
    if (fx.eq.IsFlat()) {
        if (eq_) b_.ChannelRemoveFX(stream_, eq_);
        eq_ = 0;
    } else {
        if (!eq_) eq_ = b_.ChannelSetFX(stream_, BASS_FX_BFX_PEAKEQ, EQ_PRIORITY);
        if (eq_) {
            for (int i = 0; i < EQ_BANDS; ++i) {
                BASS_BFX_PEAKEQ p = {};
                p.lBand = i;
                p.fBandwidth = 1.0f;
                p.fCenter = std::min(EQ_FREQS[i], rate_ * 0.45f);
                p.fGain = fx.eq.gain[i];
                p.lChannel = BASS_BFX_CHANALL;
                b_.FXSetParameters(eq_, &p);
            }
        }
    }
}

void FxChain::Process(float* data, DWORD frames) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (noise_on_) Denoise(data, frames);
    if (preamp_ != 1.0f) {
        size_t n = (size_t)frames * chans_;
        for (size_t i = 0; i < n; ++i) data[i] *= preamp_;
    }
    if (eq_) {
        // The equalizer runs while the stream is read: what goes in comes
        // right back out, processed.
        DWORD bytes = frames * chans_ * sizeof(float);
        b_.StreamPutData(stream_, data, bytes);
        DWORD got = b_.ChannelGetData(stream_, data, bytes);
        if (got == (DWORD)-1) got = 0;
        if (got < bytes) std::fill((char*)data + got, (char*)data + bytes, 0);
    }
    if (!stereo_ && chans_ > 1) {
        // Mono: every channel gets the average of all of them, so the source
        // keeps its number of channels and the mixer does not change.
        for (DWORD f = 0; f < frames; ++f) {
            float* frame = data + (size_t)f * chans_;
            float sum = 0;
            for (int c = 0; c < chans_; ++c) sum += frame[c];
            std::fill(frame, frame + chans_, sum / chans_);
        }
    }
}

void FxChain::Denoise(float* data, DWORD frames) {
    const float wet = noise_amount_, dry = 1.0f - noise_amount_;
    for (DWORD f = 0; f < frames; ++f) {
        for (int c = 0; c < chans_; ++c) {
            size_t i = (size_t)c * NOISE_FRAME + fill_;
            float& sample = data[(size_t)f * chans_ + c];
            float in = sample * 32768.0f;  // RNNoise works on the 16-bit scale
            sample = (out_frames_[i] * wet + dry_frames_[i] * dry) / 32768.0f;
            in_frames_[i] = in;
        }
        if (++fill_ == (size_t)NOISE_FRAME) {
            for (int c = 0; c < chans_; ++c) {
                float* in = &in_frames_[(size_t)c * NOISE_FRAME];
                std::copy(in, in + NOISE_FRAME, &dry_frames_[(size_t)c * NOISE_FRAME]);
                rnnoise_process_frame(denoise_[c], &out_frames_[(size_t)c * NOISE_FRAME], in);
            }
            fill_ = 0;
        }
    }
}
