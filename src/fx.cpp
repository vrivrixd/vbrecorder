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

#include "fx.h"

#include <algorithm>
#include <cmath>
#include <cwchar>

const float EQ_FREQS[EQ_BANDS] = {31.25f, 62.5f, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};

EqSettings::EqSettings() {
    for (int i = 0; i < EQ_BANDS; ++i) gain[i] = 0.0f;
}

bool EqSettings::IsFlat() const {
    if (preamp != 0.0f) return false;
    for (int i = 0; i < EQ_BANDS; ++i)
        if (gain[i] != 0.0f) return false;
    return true;
}

namespace {

std::wstring Num(float v) {
    wchar_t buf[32];
    swprintf(buf, 32, L"%g", v);
    return buf;
}

std::vector<std::wstring> Split(const std::wstring& text, wchar_t sep) {
    std::vector<std::wstring> out;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t end = text.find(sep, pos);
        if (end == std::wstring::npos) end = text.size();
        out.push_back(text.substr(pos, end - pos));
        pos = end + 1;
    }
    return out;
}

float ToFloat(const std::wstring& text, float fallback) {
    wchar_t* end = nullptr;
    double v = wcstod(text.c_str(), &end);
    return (end && end != text.c_str() && std::isfinite(v)) ? (float)v : fallback;
}

float Clamp(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }

}  // namespace

/* The classic Winamp presets (as VLC ships them), moved to these bands, plus
   curves for voice and everyday use. */
const std::vector<EqPreset>& EqPresets() {
    static const std::vector<EqPreset> presets = {
        {L"flat", "Plano", 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {L"bass_boost", "Reforço de Graves", -3, {6, 5, 4, 2, 0.5f, 0, 0, 0, 0, 0}},
        {L"bass_cut", "Redução de Graves", 0, {-6, -5, -4, -2, -0.5f, 0, 0, 0, 0, 0}},
        {L"treble_boost", "Reforço de Agudos", -3, {0, 0, 0, 0, 0, 0.5f, 2, 4, 5, 6}},
        {L"treble_cut", "Redução de Agudos", 0, {0, 0, 0, 0, 0, -0.5f, -2, -4, -5, -6}},
        {L"vocal", "Reforço de Voz", -2, {-3, -3, -2, 0, 2, 4, 4, 3, 1, -1}},
        {L"speech", "Locução e Podcast", -1.5f, {-8, -6, -2, 0, 1, 2, 3, 3, 1, 0}},
        {L"remove_rumble", "Remover Ronco", 0, {-15, -10, -3, 0, 0, 0, 0, 0, 0, 0}},
        {L"telephone", "Telefone", -2, {-15, -15, -12, -6, 2, 4, 3, -6, -15, -15}},
        {L"am_radio", "Rádio AM", -1.5f, {-12, -10, -4, 0, 2, 3, 2, -3, -12, -15}},
        {L"small_speakers", "Alto-falantes Pequenos", -3, {6, 5, 4, 2, 1, 0, -1, -2, -3, -4}},
        {L"headphones", "Fones de Ouvido", -7, {5, 5, 9.5f, 7.5f, -1, -2.5f, 0, 3, 7, 14.5f}},
        {L"acoustic", "Acústico", -2, {4, 4, 3, 1, 1, 1, 2, 3, 3, 2}},
        {L"classical", "Clássica", 0, {0, 0, 0, 0, 0, 0, 0, -3, -7, -9.5f}},
        {L"club", "Clube", -3, {0, 0, 0, 5, 6.5f, 5.5f, 5.5f, 4.5f, 2, 0}},
        {L"dance", "Dance", -5, {9.5f, 9.5f, 8, 4, 0.5f, 0, -3.5f, -6.5f, -7, 0}},
        {L"deep", "Grave Profundo", -2.5f, {5, 3.5f, 2, 1, 3, 2.5f, 1.5f, -2, -3.5f, -4.5f}},
        {L"electronic", "Eletrônica", -2.5f, {4, 4, 1, 0, -2, 2, 1, 1, 4, 5}},
        {L"fullbass", "Graves no Máximo", -5, {-8, -7.5f, 4.5f, 9.5f, 6.5f, 1.5f, -2, -5.5f, -9, -11}},
        {L"fullbasstreble", "Graves e Agudos no Máximo", -6, {7, 7, 6, 2, -5, -5, -1, 4.5f, 9.5f, 12}},
        {L"fulltreble", "Agudos no Máximo", -7.5f, {-9.5f, -9.5f, -9.5f, -9.5f, -5.5f, 2.5f, 8, 13, 15, 15}},
        {L"hiphop", "Hip-Hop", -2.5f, {5, 4, 1, 3, -1, -1, 1, -0.5f, 2, 3}},
        {L"jazz", "Jazz", -2, {4, 3, 1, 2, -1, -1, 0, 1, 3, 4}},
        {L"largehall", "Salão Grande", -5, {10.5f, 10.5f, 10.5f, 7.5f, 5.5f, 0, -3, -5, -5, 0}},
        {L"latin", "Latina", -2, {4.5f, 3, 0, 0, -1.5f, -1.5f, -1.5f, 0, 3, 4.5f}},
        {L"live", "Ao Vivo", -3, {-5, -4.5f, -1.5f, 2.5f, 5, 5.5f, 5.5f, 5, 3.5f, 2.5f}},
        {L"lounge", "Lounge", -2, {-3, -1.5f, -0.5f, 1.5f, 4, 2.5f, 0, -1.5f, 2, 1}},
        {L"party", "Festa", -3.5f, {7, 7, 7, 2.5f, 0, 0, 0, 0, 0, 7}},
        {L"piano", "Piano", -2, {3, 2, 0, 2.5f, 3, 1.5f, 3.5f, 4.5f, 3, 3.5f}},
        {L"pop", "Pop", -4, {-1.5f, -1.5f, 3, 6.5f, 8, 5.5f, 2, -1, -2.5f, -1.5f}},
        {L"rnb", "R&B", -3.5f, {3, 7, 5.5f, 1.5f, -2, -1.5f, 2.5f, 2.5f, 3, 4}},
        {L"reggae", "Reggae", -3, {0, 0, 0, 0, -4, 0, 4, 6.5f, 3.5f, 0}},
        {L"rock", "Rock", -5.5f, {8, 8, 5.5f, -2, -7.5f, -3, 1.5f, 6, 10, 11}},
        {L"ska", "Ska", -5, {-2.5f, -2.5f, -4, -4.5f, -1, 4, 5, 7, 9, 9.5f}},
        {L"soft", "Suave", -6, {5, 4.5f, 2.5f, 0.5f, -1.5f, 0, 2.5f, 5.5f, 8.5f, 12}},
        {L"softrock", "Soft Rock", -4.5f, {4, 4, 4, 3, 0.5f, -4, -5, -4.5f, -2, 9}},
        {L"techno", "Techno", -4.5f, {8, 8, 6.5f, 2, -4, -5, -2, 3.5f, 8.5f, 9}},
    };
    return presets;
}

/* eq=preset|preamp|gain/gain/...;ruido=1|amount;estereo=0
   Only what differs from the defaults is written. */
std::wstring FxSettings::Serialize() const {
    const FxSettings defaults;
    std::vector<std::wstring> parts;
    if (eq.preset != defaults.eq.preset || !eq.IsFlat()) {
        std::wstring s = L"eq=" + eq.preset + L"|" + Num(eq.preamp) + L"|";
        for (int i = 0; i < EQ_BANDS; ++i) s += (i ? L"/" : L"") + Num(eq.gain[i]);
        parts.push_back(s);
    }
    if (noise.on || noise.amount != defaults.noise.amount)
        parts.push_back(std::wstring(L"ruido=") + (noise.on ? L"1" : L"0") + L"|" + Num(noise.amount));
    if (!stereo) parts.push_back(L"estereo=0");
    std::wstring out;
    for (size_t i = 0; i < parts.size(); ++i) out += (i ? L";" : L"") + parts[i];
    return out;
}

FxSettings FxSettings::Parse(const std::wstring& text) {
    FxSettings fx;
    for (auto& part : Split(text, L';')) {
        size_t eqpos = part.find(L'=');
        if (eqpos == std::wstring::npos) continue;
        std::wstring name = part.substr(0, eqpos);
        std::vector<std::wstring> fields = Split(part.substr(eqpos + 1), L'|');
        if (name == L"eq" && fields.size() >= 3) {
            fx.eq.preset = fields[0];
            fx.eq.preamp = Clamp(ToFloat(fields[1], 0), EQ_MIN_GAIN, EQ_MAX_GAIN);
            std::vector<std::wstring> bands = Split(fields[2], L'/');
            for (int i = 0; i < EQ_BANDS && i < (int)bands.size(); ++i) {
                // The test versions wrote "frequency:gain"; only the gain counts.
                std::wstring g = bands[i].substr(bands[i].find(L':') + 1);
                fx.eq.gain[i] = Clamp(ToFloat(g, 0), EQ_MIN_GAIN, EQ_MAX_GAIN);
            }
        } else if (name == L"ruido" && fields.size() >= 2) {
            fx.noise.on = fields[0] == L"1";
            fx.noise.amount = Clamp(ToFloat(fields[1], 100), 0, 100);
        } else if (name == L"estereo") {
            fx.stereo = fields[0] != L"0";
        }
    }
    return fx;
}
