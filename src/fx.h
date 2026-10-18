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
 * What can be done to the sound of each source: the equalizer, with its
 * presets, the noise remover, and mono. Only the settings live here; the
 * processing is in fx_chain.cpp.
 */
#pragma once

#include <string>
#include <vector>

/* Key of the preset that is not one: the values were changed by hand. */
#define FX_CUSTOM L"custom"

const int EQ_BANDS = 10;
const float EQ_MIN_GAIN = -15.0f;
const float EQ_MAX_GAIN = 15.0f;
/* The usual octave bands of 10-band equalizers, from 31 Hz to 16 kHz. */
extern const float EQ_FREQS[EQ_BANDS];

struct EqSettings {
    std::wstring preset = L"flat";
    float preamp = 0.0f;  // dB, applied before the bands so boosts do not clip
    float gain[EQ_BANDS];  // dB, one per band of EQ_FREQS

    EqSettings();
    bool IsFlat() const;
};

struct NoiseSettings {
    bool on = false;
    float amount = 100.0f;  // percent of the cleaned sound in the result
};

struct FxSettings {
    EqSettings eq;
    NoiseSettings noise;
    bool stereo = true;  // false: the source's channels are mixed into one

    /* One line for the .ini; empty when everything is at its default. */
    std::wstring Serialize() const;
    static FxSettings Parse(const std::wstring& text);
};

struct EqPreset {
    const wchar_t* key;
    const char* name;  // translatable text
    float preamp;
    float gain[EQ_BANDS];
};

const std::vector<EqPreset>& EqPresets();
