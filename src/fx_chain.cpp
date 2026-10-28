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
// Position in a stage: higher runs first.
const int EQ_PRIORITY = 100;
const int COMPRESSOR_PRIORITY = 90;
const int PITCH_PRIORITY = 80;
const int CHORUS_PRIORITY = 70;
const int FLANGER_PRIORITY = 60;
const int REVERB_PRIORITY = 100;
const float DELAY_MAX_MS = 2000;
const float PREDELAY_MAX_MS = 500;

float At(const EffectSettings& e, size_t i) { return i < e.values.size() ? e.values[i] : 0.0f; }

float Clamp(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }

}  // namespace

FxChain::FxChain(const BassApi& bass, int rate, int chans) : b_(bass), rate_(rate), chans_(chans) {}

FxChain::~FxChain() {
    // The effects go with their streams.
    if (pre_.stream) b_.StreamFree(pre_.stream);
    if (post_.stream) b_.StreamFree(post_.stream);
    for (DenoiseState* st : denoise_) rnnoise_destroy(st);
}

DWORD FxChain::SetFx(Stage& stage, DWORD handle, bool on, DWORD type, int priority) {
    if (!on) {
        if (handle) {
            b_.ChannelRemoveFX(stage.stream, handle);
            --stage.count;
        }
        return 0;
    }
    if (handle) return handle;
    if (!stage.stream && b_.ChannelSetFX)
        stage.stream = b_.StreamCreate(rate_, chans_, BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT,
                                       STREAMPROC_PUSH, nullptr);
    if (!stage.stream) return 0;
    handle = b_.ChannelSetFX(stage.stream, type, priority);
    if (handle) ++stage.count;
    return handle;
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
    noise_amount_ = Clamp(fx.noise.amount, 0, 100) / 100.0f;
    stereo_ = fx.stereo;
    preamp_ = (float)std::pow(10.0, fx.eq.preamp / 20.0);

    // Equalizer: one peaking filter per band, an octave wide.
    eq_ = SetFx(pre_, eq_, !fx.eq.IsFlat(), BASS_FX_BFX_PEAKEQ, EQ_PRIORITY);
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

    const EffectSettings& comp = fx.effects[FX_COMPRESSOR];
    compressor_ = SetFx(pre_, compressor_, comp.on, BASS_FX_BFX_COMPRESSOR2, COMPRESSOR_PRIORITY);
    if (compressor_) {
        BASS_BFX_COMPRESSOR2 p = {};
        p.fThreshold = At(comp, 0);
        p.fRatio = std::max(1.0f, At(comp, 1));
        p.fAttack = Clamp(At(comp, 2), 0.01f, 1000);
        p.fRelease = Clamp(At(comp, 3), 0.01f, 5000);
        p.fGain = At(comp, 4);
        p.lChannel = BASS_BFX_CHANALL;
        b_.FXSetParameters(compressor_, &p);
    }

    // At 0 semitones the pitch shift is left out: it would still delay and
    // color the sound, and "Flat" must change nothing.
    const EffectSettings& pitch = fx.effects[FX_PITCH];
    float semitones = Clamp(At(pitch, 0), -12, 12);  // as far as BASS_FX goes
    pitch_ = SetFx(pre_, pitch_, pitch.on && std::fabs(semitones) >= 0.01f, BASS_FX_BFX_PITCHSHIFT,
                   PITCH_PRIORITY);
    if (pitch_) {
        BASS_BFX_PITCHSHIFT p = {};
        p.fPitchShift = (float)std::pow(2.0, semitones / 12.0);
        p.fSemitones = semitones;
        p.lFFTsize = 2048;
        p.lOsamp = 8;
        p.lChannel = BASS_BFX_CHANALL;
        b_.FXSetParameters(pitch_, &p);
    }

    // Chorus and flanger are left out without any of the effect's sound:
    // that is what "Flat" is, and it must change nothing.
    chorus_ = SetChorus(chorus_, fx.effects[FX_CHORUS], CHORUS_PRIORITY);
    flanger_ = SetChorus(flanger_, fx.effects[FX_FLANGER], FLANGER_PRIORITY);

    // Delay. BASS_FX's own one, in stereo, only crosses the left and right
    // echoes: a microphone has the same sound on both sides, so it sounded
    // like a plain delay. This one bounces for real.
    const EffectSettings& delay = fx.effects[FX_DELAY];
    if (delay.on && !delay_on_) {
        size_t len = (size_t)(DELAY_MAX_MS / 1000 * rate_) + 1;
        delay_line_.assign(len * chans_, 0.0f);
        delay_pos_ = 0;
    }
    delay_on_ = delay.on;
    if (delay_on_) {
        size_t len = delay_line_.size() / chans_;
        double frames = Clamp(At(delay, 0), 1, DELAY_MAX_MS) / 1000.0 * rate_;
        delay_frames_ = std::max<size_t>(1, std::min(len - 1, (size_t)std::lround(frames)));
        delay_feedback_ = Clamp(At(delay, 1) / 100, -0.95f, 0.95f);
        delay_wet_ = Clamp(At(delay, 2) / 100, 0, 1);
        delay_dry_ = Clamp(At(delay, 3) / 100, 0, 1);
        delay_pingpong_ = At(delay, 4) > 0.5f && chans_ >= 2;
    }

    const EffectSettings& reverb = fx.effects[FX_REVERB];
    if (reverb.on && !reverb_) {
        size_t len = (size_t)(PREDELAY_MAX_MS / 1000 * rate_) + 1;
        predelay_line_.assign(len * chans_, 0.0f);
        predelay_pos_ = 0;
    }
    reverb_ = SetFx(post_, reverb_, reverb.on, BASS_FX_BFX_FREEVERB, REVERB_PRIORITY);
    if (reverb_) {
        size_t len = predelay_line_.size() / chans_;
        double frames = Clamp(At(reverb, 5), 0, PREDELAY_MAX_MS) / 1000.0 * rate_;
        predelay_frames_ = std::min(len - 1, (size_t)std::lround(frames));
        reverb_dry_ = Clamp(At(reverb, 3) / 100, 0, 1);
        BASS_BFX_FREEVERB p = {};
        p.fRoomSize = Clamp(At(reverb, 0) / 100, 0, 1);
        p.fDamp = Clamp(At(reverb, 1) / 100, 0, 1);
        p.fWetMix = Clamp(At(reverb, 2) / 100, 0, 3);
        p.fDryMix = 0;  // the original sound is added in Reverb(), not held back
        p.fWidth = Clamp(At(reverb, 4) / 100, 0, 1);
        p.lChannel = BASS_BFX_CHANALL;
        b_.FXSetParameters(reverb_, &p);
    }

    // Convolution reverb, with a response that plays; without the effect's
    // sound it is left out, as the others.
    const EffectSettings& conv = fx.effects[FX_CONVOLUTION];
    conv_on_ = conv.on && At(conv, 1) >= 0.5f && !conv.file.empty();
    if (conv_on_) {
        std::wstring want = conv.file + L"|" + std::to_wstring((int)At(conv, 2));
        if (want != conv_loaded_) {
            std::vector<float> ir;
            if (LoadImpulse(b_, conv.file, rate_, chans_, &ir)) {
                size_t lead = (size_t)(Clamp(At(conv, 2), 0, 500) / 1000 * rate_) * chans_;
                ir.insert(ir.begin(), lead, 0.0f);
            }
            conv_.Setup(ir, chans_);
            conv_loaded_ = want;
        }
        conv_dry_ = Clamp(At(conv, 0) / 100, 0, 1);
        conv_wet_ = Clamp(At(conv, 1) / 100, 0, 2);
        conv_on_ = conv_.Ready();
    }
}

void FxChain::Convolve(float* data, DWORD frames) {
    size_t n = (size_t)frames * chans_;
    conv_out_.resize(n);
    conv_.Process(data, conv_out_.data(), frames);
    for (size_t i = 0; i < n; ++i) data[i] = conv_dry_ * data[i] + conv_wet_ * conv_out_[i];
}

DWORD FxChain::SetChorus(DWORD handle, const EffectSettings& e, int priority) {
    handle = SetFx(pre_, handle, e.on && std::fabs(At(e, 1)) >= 0.5f, BASS_FX_BFX_CHORUS, priority);
    if (handle) {
        // The delay sways between the two; the smaller one is the minimum.
        float a = Clamp(At(e, 3), 0.1f, 6000), b = Clamp(At(e, 4), 0.1f, 6000);
        BASS_BFX_CHORUS p = {};
        p.fDryMix = Clamp(At(e, 0) / 100, 0, 1);
        p.fWetMix = Clamp(At(e, 1) / 100, -1, 1);
        p.fFeedback = Clamp(At(e, 2) / 100, -0.95f, 0.95f);
        p.fMinSweep = std::min(a, b);
        p.fMaxSweep = std::max(a, b);
        p.fRate = Clamp(At(e, 5), 0.1f, 1000);
        p.lChannel = BASS_BFX_CHANALL;
        b_.FXSetParameters(handle, &p);
    }
    return handle;
}

void FxChain::RunStage(Stage& stage, float* data, DWORD frames) {
    if (!stage.stream || stage.count <= 0) return;
    DWORD bytes = frames * chans_ * sizeof(float);
    b_.StreamPutData(stage.stream, data, bytes);
    DWORD got = b_.ChannelGetData(stage.stream, data, bytes);
    if (got == (DWORD)-1) got = 0;
    if (got < bytes) std::fill((char*)data + got, (char*)data + bytes, 0);
}

void FxChain::Process(float* data, DWORD frames) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (noise_on_) Denoise(data, frames);
    if (preamp_ != 1.0f) {
        size_t n = (size_t)frames * chans_;
        for (size_t i = 0; i < n; ++i) data[i] *= preamp_;
    }
    RunStage(pre_, data, frames);
    if (delay_on_) Delay(data, frames);
    if (reverb_) Reverb(data, frames);
    if (conv_on_) Convolve(data, frames);
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

/* Each channel has a line that holds the last two seconds. The echo is read
   delay_frames_ back, and what is written goes back into the line, scaled by
   the feedback, for the next echoes.

   Ping-pong: the sound, both sides mixed, goes into the left line only; the
   left line's echo feeds the right one, and the right one's feeds the left,
   so the echoes take turns: left, right, left... */
void FxChain::Delay(float* data, DWORD frames) {
    const size_t len = delay_line_.size() / chans_;
    const float fb = delay_feedback_, wet = delay_wet_, dry = delay_dry_;
    // Echoes too faint to hear become 0: tiny floats are slow to compute.
    auto quiet = [](float v) { return std::fabs(v) < 1e-15f ? 0.0f : v; };
    for (DWORD f = 0; f < frames; ++f) {
        float* x = data + (size_t)f * chans_;
        float* write = &delay_line_[delay_pos_ * chans_];
        const float* read = &delay_line_[((delay_pos_ + len - delay_frames_) % len) * chans_];
        if (delay_pingpong_) {
            float in = 0;
            for (int c = 0; c < chans_; ++c) in += x[c];
            in /= chans_;
            float left = read[0], right = read[1];
            write[0] = quiet(in + fb * right);
            write[1] = quiet(fb * left);
            x[0] = x[0] * dry + left * wet;
            x[1] = x[1] * dry + right * wet;
            for (int c = 2; c < chans_; ++c) x[c] *= dry;
        } else {
            for (int c = 0; c < chans_; ++c) {
                float echo = read[c];
                write[c] = quiet(x[c] + fb * echo);
                x[c] = x[c] * dry + echo * wet;
            }
        }
        if (++delay_pos_ == len) delay_pos_ = 0;
    }
}

void FxChain::Reverb(float* data, DWORD frames) {
    const size_t n = (size_t)frames * chans_, len = predelay_line_.size() / chans_;
    reverb_in_.resize(n);
    for (DWORD f = 0; f < frames; ++f) {
        float* write = &predelay_line_[predelay_pos_ * chans_];
        const float* read = &predelay_line_[((predelay_pos_ + len - predelay_frames_) % len) * chans_];
        const float* x = data + (size_t)f * chans_;
        std::copy(x, x + chans_, write);
        std::copy(read, read + chans_, &reverb_in_[(size_t)f * chans_]);
        if (++predelay_pos_ == len) predelay_pos_ = 0;
    }
    RunStage(post_, reverb_in_.data(), frames);
    for (size_t i = 0; i < n; ++i) data[i] = data[i] * reverb_dry_ + reverb_in_[i];
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
