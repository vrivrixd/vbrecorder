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

#include "convolver.h"

#include <algorithm>
#include <cmath>

namespace {

const size_t N = 2 * Convolver::BLOCK;  // the FFT size: two blocks

}  // namespace

bool LoadImpulse(const BassApi& b, const std::wstring& path, int rate, int chans,
                 std::vector<float>* out) {
    out->clear();
    DWORD s = b.StreamCreateFile(FALSE, path.c_str(), 0, 0,
                                 BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT | BASS_UNICODE);
    if (!s) return false;
    // A mixer converts it to the rate and channels of the source.
    DWORD m = b.MixerStreamCreate(rate, chans, BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE);
    if (!m || !b.MixerStreamAddChannel(m, s, BASS_MIXER_CHAN_DOWNMIX | BASS_MIXER_CHAN_NORAMPIN)) {
        if (m) b.StreamFree(m);
        b.StreamFree(s);
        return false;
    }
    const size_t max = (size_t)(IMPULSE_MAX_SECONDS * rate) * chans;
    std::vector<float> buf(16384);
    while (out->size() < max) {
        DWORD got = b.ChannelGetData(m, buf.data(), (DWORD)(buf.size() * sizeof(float)));
        if (got == 0 || got == (DWORD)-1) break;
        out->insert(out->end(), buf.begin(), buf.begin() + got / sizeof(float));
    }
    b.MixerChannelRemove(s);
    b.StreamFree(s);
    b.StreamFree(m);
    if (out->size() > max) out->resize(max);
    out->resize(out->size() / chans * chans);
    // Its energy brought to 1, on the loudest channel: a response recorded
    // quietly and one recorded loudly come out alike.
    double energy = 0;
    for (int c = 0; c < chans; ++c) {
        double e = 0;
        for (size_t i = c; i < out->size(); i += chans) e += (double)(*out)[i] * (*out)[i];
        energy = std::max(energy, e);
    }
    if (energy <= 1e-12) {
        out->clear();
        return false;
    }
    float scale = (float)(1 / std::sqrt(energy));
    for (float& v : *out) v *= scale;
    return true;
}

void Convolver::Setup(const std::vector<float>& ir, int chans) {
    chans_ = chans;
    size_t frames = chans > 0 ? ir.size() / chans : 0;
    parts_ = (frames + BLOCK - 1) / BLOCK;
    prev_.assign((size_t)chans * BLOCK, 0.0f);
    cur_.assign((size_t)chans * BLOCK, 0.0f);
    out_.assign((size_t)chans * BLOCK, 0.0f);
    fill_ = 0;
    newest_ = 0;
    if (!parts_) return;

    twiddle_.resize(N / 2);
    for (size_t k = 0; k < N / 2; ++k)
        twiddle_[k] = std::polar(1.0f, (float)(-2 * 3.14159265358979323846 * k / N));
    reverse_.resize(N);
    int bits = 0;
    while (((size_t)1 << bits) < N) ++bits;
    for (size_t i = 0; i < N; ++i) {
        size_t r = 0;
        for (int b = 0; b < bits; ++b)
            if (i & ((size_t)1 << b)) r |= (size_t)1 << (bits - 1 - b);
        reverse_[i] = r;
    }
    acc_.resize(N);

    // Each block of the response, padded to N, in the frequency domain.
    ir_.assign((size_t)chans * parts_ * N, Complex());
    spectra_.assign((size_t)chans * parts_ * N, Complex());
    for (int c = 0; c < chans; ++c) {
        for (size_t p = 0; p < parts_; ++p) {
            Complex* h = &ir_[((size_t)c * parts_ + p) * N];
            for (size_t i = 0; i < (size_t)BLOCK; ++i) {
                size_t f = p * BLOCK + i;
                if (f < frames) h[i] = ir[f * chans + c];
            }
            Fft(h, false);
        }
    }
}

void Convolver::Fft(Complex* data, bool inverse) const {
    for (size_t i = 0; i < N; ++i)
        if (i < reverse_[i]) std::swap(data[i], data[reverse_[i]]);
    for (size_t len = 2; len <= N; len <<= 1) {
        size_t step = N / len;
        for (size_t start = 0; start < N; start += len) {
            for (size_t k = 0; k < len / 2; ++k) {
                Complex w = twiddle_[k * step];
                if (inverse) w = std::conj(w);
                Complex a = data[start + k], x = data[start + k + len / 2];
                Complex b(x.real() * w.real() - x.imag() * w.imag(), x.real() * w.imag() + x.imag() * w.real());
                data[start + k] = a + b;
                data[start + k + len / 2] = a - b;
            }
        }
    }
}

void Convolver::Process(const float* in, float* wet, DWORD frames) {
    if (!parts_) {
        std::fill(wet, wet + (size_t)frames * chans_, 0.0f);
        return;
    }
    for (DWORD f = 0; f < frames; ++f) {
        for (int c = 0; c < chans_; ++c) {
            cur_[(size_t)c * BLOCK + fill_] = in[(size_t)f * chans_ + c];
            wet[(size_t)f * chans_ + c] = out_[(size_t)c * BLOCK + fill_];
        }
        if (++fill_ == (size_t)BLOCK) {
            RunBlock();
            fill_ = 0;
        }
    }
}

void Convolver::RunBlock() {
    newest_ = (newest_ + 1) % parts_;
    for (int c = 0; c < chans_; ++c) {
        // The last two blocks of sound, in the frequency domain, kept with
        // the ones before them.
        Complex* x = &spectra_[((size_t)c * parts_ + newest_) * N];
        for (size_t i = 0; i < (size_t)BLOCK; ++i) {
            x[i] = prev_[(size_t)c * BLOCK + i];
            x[BLOCK + i] = cur_[(size_t)c * BLOCK + i];
        }
        Fft(x, false);
        // Each block of the response with the block of sound as old as it is.
        // The signals are real, so the upper half of the spectrum mirrors the
        // lower one: only the lower half is multiplied, by hand (the complex
        // product of the library checks for infinities, and is far slower).
        const size_t half = N / 2 + 1;
        std::fill(acc_.begin(), acc_.end(), Complex());
        float* acc = reinterpret_cast<float*>(acc_.data());
        for (size_t p = 0; p < parts_; ++p) {
            const float* h = reinterpret_cast<const float*>(&ir_[((size_t)c * parts_ + p) * N]);
            const float* s = reinterpret_cast<const float*>(
                &spectra_[((size_t)c * parts_ + (newest_ + parts_ - p) % parts_) * N]);
            for (size_t i = 0; i < 2 * half; i += 2) {
                acc[i] += h[i] * s[i] - h[i + 1] * s[i + 1];
                acc[i + 1] += h[i] * s[i + 1] + h[i + 1] * s[i];
            }
        }
        for (size_t i = half; i < N; ++i) acc_[i] = std::conj(acc_[N - i]);
        Fft(acc_.data(), true);
        // Overlap-save: the second half is the result for this block.
        for (size_t i = 0; i < (size_t)BLOCK; ++i)
            out_[(size_t)c * BLOCK + i] = acc_[BLOCK + i].real() / N;
        std::copy(cur_.begin() + (size_t)c * BLOCK, cur_.begin() + (size_t)(c + 1) * BLOCK,
                  prev_.begin() + (size_t)c * BLOCK);
    }
}
