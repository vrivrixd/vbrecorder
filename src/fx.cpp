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

/* Every effect starts with "Flat", which changes nothing, as the equalizer
   does: checking an effect turns it on without changing the sound yet.
   Where the other presets come from:
   - compressor: usual starting points for recording voice and music;
   - chorus and flanger: the examples of the BASS_FX author, and a gentler
     flanger;
   - pitch shift: steps that keep a voice recognizable, and the extremes;
   - delay: the examples of the BASS_FX author and common pedal settings;
   - reverb: Audacity's factory presets, converted to Freeverb, with their
     pre-delays as they are. */
const std::vector<EffectDef>& Defs() {
    static const std::vector<EffectDef> defs = {
        {L"compressor", "Compressor",
         {{"Limiar:", -60, 0, 0, L" dB", false, 1, 5},
          {"Proporção:", 1, 20, 1, L":1", false, 0.5f, 2},
          {"Ataque:", 1, 500, 0, L" ms", false, 1, 10},
          {"Liberação:", 10, 5000, 0, L" ms", false, 1, 50},
          {"Ganho:", -20, 30, 0, L" dB", false, 1, 5}},
         {
             {L"flat", "Plano", {0, 1, 10, 200, 0}},
             {L"default", "Padrão", {-15, 3, 10, 200, 0}},
             {L"gentle_voice", "Voz Suave", {-18, 2, 10, 150, 3}},
             {L"podcast", "Podcast", {-20, 3, 5, 100, 5}},
             {L"radio", "Locução de Rádio", {-24, 4, 3, 80, 8}},
             {L"music", "Nivelar Música", {-15, 2.5f, 20, 250, 3}},
             {L"heavy", "Compressão Pesada", {-30, 8, 1, 100, 9}},
             {L"limiter", "Limitador", {-6, 20, 1, 50, 0}},
         }},
        // One octave each way, as far as BASS_FX goes, in half semitones.
        {L"pitch", "Pitch Shift",
         {{"Semitons:", -12, 12, 1, L"", false, 0.5f, 1}},
         {
             {L"flat", "Plano", {0}},
             {L"deeper", "Voz Grave", {-5}},
             {L"slightly_lower", "Um Pouco Mais Grave", {-2}},
             {L"monster", "Monstro", {-12}},
             {L"slightly_higher", "Um Pouco Mais Aguda", {2}},
             {L"higher", "Voz Aguda", {5}},
             {L"chipmunk", "Esquilo", {12}},
         }},
        // A delay that sways, from a few to hundreds of milliseconds.
        {L"chorus", "Chorus",
         {{"Som Original:", 0, 100, 0, L"%", false, 1, 5},
          {"Nível do Efeito:", -100, 100, 0, L"%", false, 1, 5},
          {"Realimentação:", -95, 95, 0, L"%", false, 1, 5},
          {"Atraso Mínimo:", 1, 500, 0, L" ms", false, 1, 10},
          {"Atraso Máximo:", 1, 500, 0, L" ms", false, 1, 10},
          {"Velocidade:", 1, 500, 0, L" ms/s", false, 1, 10}},
         {
             {L"flat", "Plano", {100, 0, 50, 1, 200, 50}},
             {L"exaggerated", "Chorus Exagerado", {70, 25, 50, 1, 200, 50}},
             {L"motorcycle", "Motocicleta", {90, 45, 50, 1, 100, 25}},
             {L"devil", "Diabo", {90, 35, 50, 1, 50, 200}},
             {L"many_voices", "Muitas Vozes", {90, 35, 50, 1, 400, 200}},
             {L"back_chipmunk", "Esquilo ao Fundo", {90, -20, 50, 1, 400, 400}},
         }},
        // The same effect with a short delay, in tenths of a millisecond.
        {L"flanger", "Flanger",
         {{"Som Original:", 0, 100, 0, L"%", false, 1, 5},
          {"Nível do Efeito:", -100, 100, 0, L"%", false, 1, 5},
          {"Realimentação:", -95, 95, 0, L"%", false, 1, 5},
          {"Atraso Mínimo:", 0.1f, 20, 1, L" ms", false, 0.1f, 1},
          {"Atraso Máximo:", 0.1f, 20, 1, L" ms", false, 0.1f, 1},
          {"Velocidade:", 0.1f, 50, 1, L" ms/s", false, 0.1f, 1}},
         {
             {L"flat", "Plano", {100, 0, 50, 1, 5, 1}},
             {L"default", "Padrão", {100, 35, 50, 1, 5, 1}},
             {L"gentle", "Flanger Suave", {100, 25, 20, 1, 3, 0.5f}},
             {L"water", "Água", {90, -40, 50, 1, 2, 1}},
             {L"airplane", "Avião", {30, 40, 50, 1, 10, 5}},
         }},
        {L"delay", "Delay",
         {{"Tempo:", 1, 2000, 0, L" ms", false, 1, 50},
          {"Realimentação:", -95, 95, 0, L"%", false, 1, 5},
          {"Nível do Efeito:", 0, 100, 0, L"%", false, 1, 5},
          {"Som Original:", 0, 100, 0, L"%", false, 1, 5},
          {"P&ingue-pongue", 0, 1, 0, L"", true, 1, 1}},
         {
             {L"flat", "Plano", {200, 0, 0, 100, 0}},
             {L"small_echo", "Eco Curto", {200, 0, 60, 100, 0}},
             {L"many_echoes", "Muitos Ecos", {500, 70, 50, 100, 0}},
             {L"reverse_echoes", "Ecos Invertidos", {800, -70, 60, 100, 0}},
             {L"robotic", "Voz Robótica", {10, 70, 80, 50, 0}},
             {L"slapback", "Slapback", {110, 0, 50, 100, 0}},
             {L"doubling", "Dobra de Voz", {30, 0, 60, 100, 0}},
             {L"pingpong", "Pingue-pongue", {350, 50, 60, 100, 1}},
             {L"stadium", "Estádio", {250, 35, 40, 100, 1}},
             {L"canyon", "Cânion", {700, 40, 60, 100, 0}},
         }},
        {L"reverb", "Reverb",
         {{"Tamanho da Sala:", 0, 100, 0, L"%", false, 1, 5},
          {"Amortecimento:", 0, 100, 0, L"%", false, 1, 5},
          {"Nível do Efeito:", 0, 300, 0, L"%", false, 1, 10},
          {"Som Original:", 0, 100, 0, L"%", false, 1, 5},
          {"Largura Estéreo:", 0, 100, 0, L"%", false, 1, 5},
          {"Pré-atraso:", 0, 500, 0, L" ms", false, 1, 10}},
         {
             {L"flat", "Plano", {50, 50, 0, 100, 100, 0}},
             {L"medium_room", "Sala Média", {56, 42, 89, 89, 70, 10}},
             {L"small_bright", "Sala Pequena Clara", {39, 30, 89, 89, 100, 10}},
             {L"small_dark", "Sala Pequena Escura", {39, 70, 89, 89, 100, 10}},
             {L"large_room", "Sala Grande", {61, 38, 100, 50, 90, 10}},
             {L"bathroom", "Banheiro", {47, 0, 50, 100, 100, 8}},
             {L"church_hall", "Salão de Igreja", {74, 50, 100, 25, 100, 32}},
             {L"cathedral", "Catedral", {88, 70, 100, 10, 100, 16}},
             {L"big_cave", "Caverna Grande", {98, 55, 178, 71, 100, 55}},
             {L"ambience", "Ambiente", {98, 55, 100, 32, 100, 55}},
             {L"acoustic", "Acústico", {61, 60, 20, 100, 80, 10}},
             {L"artificial", "Artificial", {51, 70, 63, 100, 100, 99}},
             {L"clean", "Limpo", {61, 60, 13, 100, 75, 10}},
             {L"modern", "Moderno", {61, 60, 18, 100, 75, 10}},
             {L"vocal1", "Voz I", {54, 79, 25, 100, 70, 20}},
             {L"vocal2", "Voz II", {49, 59, 89, 89, 70, 0}},
             {L"dance_vocal", "Voz Dance", {74, 66, 32, 100, 100, 2}},
             {L"modern_vocal", "Voz Moderna", {70, 24, 32, 100, 68, 27}},
             {L"voice_tail", "Cauda de Voz", {81, 24, 50, 100, 68, 27}},
         }},
        // The sound of a real place, from an impulse response of the user's.
        {L"convolution", "Reverb de Convolução",
         {{"Som Original:", 0, 100, 0, L"%", false, 1, 5},
          {"Nível do Efeito:", 0, 200, 0, L"%", false, 1, 10},
          {"Pré-atraso:", 0, 500, 0, L" ms", false, 1, 10}},
         {
             {L"flat", "Plano", {100, 0, 0}},
             {L"default", "Padrão", {100, 50, 0}},
             {L"effect_only", "Somente o Efeito", {0, 100, 0}},
         },
         true},
    };
    return defs;
}

}  // namespace

const EffectDef& GetEffectDef(int kind) { return Defs()[kind]; }

int ParamSteps(const FxParam& p) { return std::max(1, (int)std::lround((p.max - p.min) / p.step)); }

int ParamPage(const FxParam& p) { return std::max(1, (int)std::lround(p.page / p.step)); }

float ParamValue(const FxParam& p, int pos) {
    if (p.toggle) return pos > 0 ? 1.0f : 0.0f;
    pos = std::max(0, std::min(ParamSteps(p), pos));
    return std::min(p.max, p.min + pos * p.step);
}

int ParamPos(const FxParam& p, float value) {
    if (p.toggle) return value > 0.5f ? 1 : 0;
    return std::max(0, std::min(ParamSteps(p), (int)std::lround((value - p.min) / p.step)));
}

std::wstring FormatParam(const FxParam& p, float value) {
    wchar_t buf[48];
    // Values that can go both ways show their sign: +5.0, -5.0.
    float shown = std::fabs(value) < 0.5f * std::pow(10.0f, -(float)p.decimals) ? 0.0f : value;
    swprintf(buf, 48, p.min < 0 && shown > 0 ? L"%+.*f%ls" : L"%.*f%ls", p.decimals, shown, p.unit);
    return buf;
}

FxSettings::FxSettings() {
    for (int k = 0; k < FX_COUNT; ++k) {
        const EffectDef& def = GetEffectDef(k);
        effects[k].preset = def.presets[0].key;
        effects[k].values = def.presets[0].values;
    }
}

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

namespace {

/* A path in the effects line: ; and | separate things there, and % starts
   an escape. */
std::wstring Escape(const std::wstring& text) {
    std::wstring out;
    for (wchar_t c : text) {
        if (c == L'%') out += L"%25";
        else if (c == L';') out += L"%3B";
        else if (c == L'|') out += L"%7C";
        else out += c;
    }
    return out;
}

std::wstring Unescape(const std::wstring& text) {
    std::wstring out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'%' && i + 2 < text.size()) {
            std::wstring code = text.substr(i + 1, 2);
            if (code == L"25") { out += L'%'; i += 2; continue; }
            if (code == L"3B") { out += L';'; i += 2; continue; }
            if (code == L"7C") { out += L'|'; i += 2; continue; }
        }
        out += text[i];
    }
    return out;
}

}  // namespace

/* eq=preset|preamp|gain/gain/...;delay=1|preset|v/v/...;ruido=1|amount;estereo=0
   Only what differs from the defaults is written. */
std::wstring FxSettings::Serialize() const {
    const FxSettings defaults;
    std::vector<std::wstring> parts;
    if (eq.preset != defaults.eq.preset || !eq.IsFlat()) {
        std::wstring s = L"eq=" + eq.preset + L"|" + Num(eq.preamp) + L"|";
        for (int i = 0; i < EQ_BANDS; ++i) s += (i ? L"/" : L"") + Num(eq.gain[i]);
        parts.push_back(s);
    }
    for (int k = 0; k < FX_COUNT; ++k) {
        const EffectSettings& e = effects[k];
        if (!e.on && e.preset == defaults.effects[k].preset &&
            e.values == defaults.effects[k].values && e.file.empty())
            continue;
        std::wstring s = std::wstring(GetEffectDef(k).key) + L"=" + (e.on ? L"1" : L"0") + L"|" +
                         e.preset + L"|";
        for (size_t i = 0; i < e.values.size(); ++i) s += (i ? L"/" : L"") + Num(e.values[i]);
        if (!e.file.empty()) s += L"|" + Escape(e.file);
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
        } else {
            for (int k = 0; k < FX_COUNT; ++k) {
                const EffectDef& def = GetEffectDef(k);
                if (name != def.key || fields.size() < 3) continue;
                EffectSettings& e = fx.effects[k];
                e.on = fields[0] == L"1";
                e.preset = fields[1];
                std::vector<std::wstring> values = Split(fields[2], L'/');
                for (size_t i = 0; i < def.params.size() && i < values.size(); ++i)
                    e.values[i] = Clamp(ToFloat(values[i], e.values[i]), def.params[i].min,
                                        def.params[i].max);
                if (fields.size() >= 4) e.file = Unescape(fields[3]);
            }
        }
    }
    return fx;
}
