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
 * What can be done to the sound of each source: the equalizer, the effects,
 * with their presets, the noise remover, and mono. Only the settings live
 * here; the processing is in fx_chain.cpp.
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

/* In the order they are applied, and listed in the Effects dialog. */
enum EffectKind {
    FX_COMPRESSOR,
    FX_PITCH,
    FX_CHORUS,
    FX_FLANGER,
    FX_DELAY,
    FX_REVERB,
    FX_COUNT
};

struct EffectSettings {
    bool on = false;
    std::wstring preset;        // preset key, or FX_CUSTOM
    std::vector<float> values;  // one per parameter, in the units shown
};

struct NoiseSettings {
    bool on = false;
    float amount = 100.0f;  // percent of the cleaned sound in the result
};

struct FxSettings {
    EqSettings eq;
    EffectSettings effects[FX_COUNT];
    NoiseSettings noise;
    bool stereo = true;  // false: the source's channels are mixed into one

    FxSettings();  // every effect off, with the values of its first preset

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

/* A control in an effect's dialog. The arrows move it by `step` (1 ms, 1%,
   1 dB...), Page Up and Page Down by `page`. Unlike the other sliders of the
   program, these say the value itself (semitones, milliseconds...), since
   that is what the effect is set in. */
struct FxParam {
    const char* label;  // translatable text
    float min, max;
    int decimals;
    const wchar_t* unit;  // shown after the value, never translated
    bool toggle;          // a check box (0 or 1) instead of a slider
    float step, page;
};

struct FxPreset {
    const wchar_t* key;  // in the .ini
    const char* name;    // translatable text
    std::vector<float> values;
};

struct EffectDef {
    const wchar_t* key;  // in the .ini
    const char* name;    // translatable text
    std::vector<FxParam> params;
    std::vector<FxPreset> presets;  // the first one is the starting point
};

const EffectDef& GetEffectDef(int kind);

/* Slider positions: 0 to ParamSteps, one step apart; to value and back. */
int ParamSteps(const FxParam& p);
int ParamPage(const FxParam& p);
float ParamValue(const FxParam& p, int pos);
int ParamPos(const FxParam& p, float value);
/* The value with its unit, as the dialog shows it. */
std::wstring FormatParam(const FxParam& p, float value);
