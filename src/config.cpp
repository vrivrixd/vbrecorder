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

#include "config.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <initializer_list>

#include "util.h"

namespace {

/* The Python version wrote the .ini in the Windows code page (ANSI). This
   version writes UTF-8; reading accepts both. */
std::wstring DecodeFile(const std::string& bytes) {
    std::string data = bytes;
    if (data.size() >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB &&
        (unsigned char)data[2] == 0xBF) {
        data.erase(0, 3);
    }
    if (data.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data.data(), (int)data.size(),
                                nullptr, 0);
    if (n > 0) return Widen(data, CP_UTF8);
    return Widen(data, CP_ACP);
}

bool ReadFileBytes(const std::wstring& path, std::string* out) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size;
    bool ok = GetFileSizeEx(f, &size) && size.QuadPart < (1 << 24);
    if (ok) {
        out->resize((size_t)size.QuadPart);
        DWORD read = 0;
        ok = size.QuadPart == 0 ||
             (ReadFile(f, &(*out)[0], (DWORD)size.QuadPart, &read, nullptr) &&
              read == size.QuadPart);
    }
    CloseHandle(f);
    return ok;
}

}  // namespace

ConfigManager::ConfigManager() { Load(); }

ConfigManager::Section* ConfigManager::Find(const std::wstring& name) {
    for (auto& s : sections_)
        if (s.name == name) return &s;
    return nullptr;
}

const ConfigManager::Section* ConfigManager::Find(const std::wstring& name) const {
    for (auto& s : sections_)
        if (s.name == name) return &s;
    return nullptr;
}

ConfigManager::Section& ConfigManager::Ensure(const std::wstring& name) {
    Section* s = Find(name);
    if (s) return *s;
    sections_.push_back(Section{name, {}});
    return sections_.back();
}

const std::wstring* ConfigManager::Lookup(const Items& items, const std::wstring& key) {
    for (auto& kv : items)
        if (kv.first == key) return &kv.second;
    return nullptr;
}

void ConfigManager::Put(Items& items, const std::wstring& key, const std::wstring& value) {
    for (auto& kv : items) {
        if (kv.first == key) {
            kv.second = value;
            return;
        }
    }
    items.emplace_back(key, value);
}

void ConfigManager::Load() {
    sections_.clear();
    std::string bytes;
    if (ReadFileBytes(CONFIG_FILE, &bytes)) {
        std::wstring text = DecodeFile(bytes);
        Section* current = nullptr;
        size_t pos = 0;
        while (pos <= text.size()) {
            size_t end = text.find(L'\n', pos);
            if (end == std::wstring::npos) end = text.size();
            std::wstring raw = text.substr(pos, end - pos);
            pos = end + 1;
            std::wstring line = Trim(raw);
            if (line.empty() || line[0] == L'#' || line[0] == L';') continue;
            if (line[0] == L'[' && line.back() == L']') {
                current = &Ensure(line.substr(1, line.size() - 2));
                continue;
            }
            if (!current) continue;
            size_t sep = line.find_first_of(L"=:");
            if (sep == std::wstring::npos) continue;
            // Keys are case-insensitive, as in configparser.
            std::wstring key = ToLower(Trim(line.substr(0, sep)));
            Put(current->items, key, Trim(line.substr(sep + 1)));
        }
    }

    Section* cfg = Find(L"CONFIG");
    if (!cfg) {
        Section& s = Ensure(L"CONFIG");
        s.items = {{L"formato", L"mp3"},
                   {L"bitrate", L"128 KBPS"},
                   {L"flac_nivel", L"4"},
                   {L"taxa", L"44100"},
                   {L"canais", L"stereo"},
                   {L"fontes", L"device:0:1.0"},
                   {L"reproduzir_tons", L"False"},
                   {L"arquivos_separados", L"False"},
                   {L"duracao", L""},
                   {L"arquivo", L""},
                   {L"diretorio", L""},
                   {L"idioma", L""},
                   {L"iniciar_minimizado", L"False"},
                   {L"minimizar_bandeja", L"False"},
                   {L"sempre_no_topo", L"True"},
                   {L"buffer", std::to_wstring(BUFFER_DEFAULT)},
                   {L"verificar_atualizacoes", L"True"},
                   {L"bits", L"16"},
                   {L"iniciar_com_windows", L"False"},
                   {L"contagem_regressiva", L"False"}};
    } else {
        if (!Lookup(cfg->items, L"fontes")) {
            const std::wstring* d1 = Lookup(cfg->items, L"dispositivo1");
            Put(cfg->items, L"fontes", L"device:" + (d1 ? *d1 : std::wstring(L"0")) + L":1.0");
        }
        // Keys added in newer versions: an old .ini gets the default value
        // instead of nothing.
        const std::pair<const wchar_t*, std::wstring> defaults[] = {
            {L"idioma", L""},
            {L"iniciar_minimizado", L"False"},
            {L"minimizar_bandeja", L"False"},
            {L"sempre_no_topo", L"True"},
            {L"buffer", std::to_wstring(BUFFER_DEFAULT)},
            {L"verificar_atualizacoes", L"True"},
            {L"bits", L"16"},
            {L"iniciar_com_windows", L"False"},
            {L"contagem_regressiva", L"False"}};
        for (auto& p : defaults)
            if (!Lookup(cfg->items, p.first)) Put(cfg->items, p.first, p.second);
    }

    if (!Find(L"KEYS")) {
        Section& s = Ensure(L"KEYS");
        s.items = {{L"gravar", L""},     {L"pausar", L""},  {L"parar", L""},
                   {L"reproduzir", L""}, {L"excluir", L""}, {L"mostrar", L""},
                   {L"fechar", L""}};
    }
}

void ConfigManager::Save() {
    // Known keys in a fixed order, so keys added by newer versions do not
    // end up at the bottom; unknown ones are kept after them.
    auto sort_items = [](Items& items, std::initializer_list<const wchar_t*> ordered) {
        Items sorted;
        for (const wchar_t* k : ordered)
            if (const std::wstring* v = Lookup(items, k)) sorted.emplace_back(k, *v);
        for (auto& kv : items)
            if (!Lookup(sorted, kv.first)) sorted.push_back(kv);
        items = sorted;
    };
    sort_items(Ensure(L"CONFIG").items,
               {L"formato", L"bitrate", L"flac_nivel", L"taxa", L"bits", L"canais", L"fontes",
                L"reproduzir_tons", L"arquivos_separados", L"duracao", L"arquivo", L"diretorio",
                L"idioma", L"iniciar_minimizado", L"minimizar_bandeja", L"sempre_no_topo", L"buffer",
                L"verificar_atualizacoes", L"iniciar_com_windows", L"contagem_regressiva"});
    sort_items(Ensure(L"KEYS").items, {L"gravar", L"pausar", L"parar", L"reproduzir", L"excluir",
                                       L"mostrar", L"fechar"});

    std::wstring text;
    for (auto& s : sections_) {
        text += L"[" + s.name + L"]\n";
        for (auto& kv : s.items) text += kv.first + L" = " + kv.second + L"\n";
        text += L"\n";
    }
    std::string bytes = Narrow(text, CP_UTF8);
    HANDLE f = CreateFileW(CONFIG_FILE, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(f, bytes.data(), (DWORD)bytes.size(), &written, nullptr);
    CloseHandle(f);
}

void ConfigManager::SetConfig(const std::wstring& key, const std::wstring& value) {
    Put(Ensure(L"CONFIG").items, key, value);
}

void ConfigManager::SetConfig(const std::wstring& key, bool value) {
    SetConfig(key, std::wstring(value ? L"True" : L"False"));
}

void ConfigManager::SetConfig(const std::wstring& key, int value) {
    SetConfig(key, std::to_wstring(value));
}

void ConfigManager::SetKey(const std::wstring& key, const std::wstring& value) {
    Put(Ensure(L"KEYS").items, key, value);
}

std::wstring ConfigManager::Get(const std::wstring& key) const {
    const Section* s = Find(L"CONFIG");
    const std::wstring* v = s ? Lookup(s->items, key) : nullptr;
    return v ? *v : std::wstring();
}

std::wstring ConfigManager::GetKey(const std::wstring& key) const {
    const Section* s = Find(L"KEYS");
    const std::wstring* v = s ? Lookup(s->items, key) : nullptr;
    return v ? *v : std::wstring();
}

bool ConfigManager::GetBoolean(const std::wstring& key, bool fallback) const {
    const Section* s = Find(L"CONFIG");
    const std::wstring* v = s ? Lookup(s->items, key) : nullptr;
    if (!v) return fallback;
    std::wstring low = ToLower(*v);
    if (low == L"1" || low == L"yes" || low == L"true" || low == L"on") return true;
    if (low == L"0" || low == L"no" || low == L"false" || low == L"off") return false;
    return fallback;
}

int ConfigManager::GetBuffer() const {
    std::wstring raw = Trim(Get(L"buffer"));
    wchar_t* end = nullptr;
    long value = raw.empty() ? 0 : wcstol(raw.c_str(), &end, 10);
    if (raw.empty() || !end || *end != L'\0') return BUFFER_DEFAULT;
    return (int)std::max<long>(BUFFER_MIN, std::min<long>(BUFFER_MAX, value));
}

std::vector<SourceEntry> ConfigManager::GetSources() const {
    std::wstring raw = Get(L"fontes");
    std::vector<SourceEntry> result;
    // The equalizer, noise remover and stereo of each source, in the same order, in
    // a section of their own: [EFEITOS] 1 = ..., 2 = ...
    const Section* fx_section = Find(L"EFEITOS");
    // And the path of each background audio, which may have any character:
    // [ARQUIVOS] 2 = C:\Música\fundo.mp3
    const Section* file_section = Find(L"ARQUIVOS");
    int index = 0;
    auto fx_of = [&](int i) {
        const std::wstring* v = fx_section ? Lookup(fx_section->items, std::to_wstring(i)) : nullptr;
        return v ? FxSettings::Parse(*v) : FxSettings();
    };
    size_t pos = 0;
    while (pos <= raw.size()) {
        size_t end = raw.find(L',', pos);
        if (end == std::wstring::npos) end = raw.size();
        std::wstring part = Trim(raw.substr(pos, end - pos));
        pos = end + 1;
        ++index;

        std::vector<std::wstring> tokens;
        size_t t = 0;
        while (t <= part.size()) {
            size_t c = part.find(L':', t);
            if (c == std::wstring::npos) c = part.size();
            tokens.push_back(part.substr(t, c - t));
            t = c + 1;
        }
        if (tokens.size() < 2) continue;
        if (tokens[0] == L"exe") {
            // A source by executable name: "exe:jogo.exe:1.0000".
            SourceEntry e;
            e.type = tokens[0];
            e.exe = Trim(tokens[1]);
            if (tokens.size() >= 3) {
                wchar_t* vend = nullptr;
                double v = wcstod(tokens[2].c_str(), &vend);
                if (vend && vend != tokens[2].c_str()) e.volume = v;
            }
            e.fx = fx_of(index);
            if (!e.exe.empty()) result.push_back(e);
            continue;
        }
        if (tokens[0] == L"file") {
            // Background audio: "file:1:1.0000:0", 1 when it repeats, then 1
            // when it shuffles.
            SourceEntry e;
            e.type = tokens[0];
            e.loop = Trim(tokens[1]) != L"0";
            e.shuffle = tokens.size() >= 4 && Trim(tokens[3]) == L"1";
            if (tokens.size() >= 3) {
                wchar_t* vend = nullptr;
                double v = wcstod(tokens[2].c_str(), &vend);
                if (vend && vend != tokens[2].c_str()) e.volume = v;
            }
            e.fx = fx_of(index);
            const std::wstring* path =
                file_section ? Lookup(file_section->items, std::to_wstring(index)) : nullptr;
            if (path && !Trim(*path).empty()) {
                e.path = Trim(*path);
                result.push_back(e);
            }
            continue;
        }
        const std::wstring& id_str = tokens[1];
        size_t digits = id_str.find_first_not_of(L'-');
        // A single sign only: "-1" is valid, "--1" is not a number.
        if (digits == std::wstring::npos || digits > 1 ||
            id_str.find_first_not_of(L"0123456789", digits) != std::wstring::npos)
            continue;
        SourceEntry e;
        e.type = tokens[0];
        e.id = (int)wcstol(id_str.c_str(), nullptr, 10);
        if (tokens.size() >= 3) {
            wchar_t* vend = nullptr;
            double v = wcstod(tokens[2].c_str(), &vend);
            if (vend && vend != tokens[2].c_str()) e.volume = v;
        }
        e.fx = fx_of(index);
        result.push_back(e);
    }
    if (result.empty()) result.push_back(SourceEntry{L"device", 0, L"", 1.0, FxSettings()});
    return result;
}

void ConfigManager::SetSources(const std::vector<Source>& sources) {
    std::wstring value;
    for (size_t i = 0; i < sources.size(); ++i) {
        wchar_t vol[64];
        swprintf(vol, 64, L"%.4f", sources[i].volume);
        if (i) value += L",";
        const Source& s = sources[i];
        std::wstring what = s.type == L"exe"    ? s.name
                            : s.type == L"file" ? std::wstring(s.loop ? L"1" : L"0")
                                                : std::to_wstring(s.id);
        value += s.type + L":" + what + L":" + vol;
        if (s.type == L"file") value += s.shuffle ? L":1" : L":0";
    }
    SetConfig(L"fontes", value);
    Section& file_section = Ensure(L"ARQUIVOS");
    file_section.items.clear();
    for (size_t i = 0; i < sources.size(); ++i)
        if (sources[i].type == L"file")
            file_section.items.emplace_back(std::to_wstring(i + 1), sources[i].name);
    Section& fx_section = Ensure(L"EFEITOS");
    fx_section.items.clear();
    for (size_t i = 0; i < sources.size(); ++i) {
        std::wstring fx = sources[i].fx.Serialize();
        if (!fx.empty()) fx_section.items.emplace_back(std::to_wstring(i + 1), fx);
    }
}

std::vector<Schedule> ConfigManager::GetSchedules() const {
    std::vector<Schedule> result;
    const Section* section = Find(L"AGENDAMENTOS");
    if (!section) return result;
    for (auto& kv : section->items) {
        std::wstring v = kv.second;
        size_t a = v.find(L'|');
        size_t b = a == std::wstring::npos ? a : v.find(L'|', a + 1);
        if (b == std::wstring::npos) continue;
        size_t c = v.find(L'|', b + 1);
        Schedule s;
        s.date = Trim(v.substr(0, a));
        s.remove_after = Trim(v.substr(b + 1, c == std::wstring::npos ? c : c - b - 1)) == L"1";
        if (c != std::wstring::npos) ParseDuration(v.substr(c + 1), &s.duration);
        if (!ParseScheduleTime(v.substr(a + 1, b - a - 1), &s.hour, &s.minute, &s.second)) continue;
        int y = 0, m = 0, d = 0;
        if (!s.date.empty() && swscanf(s.date.c_str(), L"%d-%d-%d", &y, &m, &d) != 3) continue;
        result.push_back(s);
    }
    return result;
}

void ConfigManager::SetSchedules(const std::vector<Schedule>& schedules) {
    Section& section = Ensure(L"AGENDAMENTOS");
    section.items.clear();
    for (size_t i = 0; i < schedules.size(); ++i) {
        const Schedule& s = schedules[i];
        section.items.emplace_back(std::to_wstring(i + 1),
                                   s.date + L"|" + ScheduleTimeForEdit(s) + L"|" +
                                       (s.remove_after ? L"1" : L"0") + L"|" +
                                       (s.duration > 0 ? FormatDuration(s.duration) : L""));
    }
}

bool SameSource(const Source& a, const Source& b) {
    if (a.type != b.type) return false;
    if (a.type == L"exe" || a.type == L"file") return ToLower(a.name) == ToLower(b.name);
    return a.id == b.id;
}
