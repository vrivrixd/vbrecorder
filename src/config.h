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
 * Reading and writing VBRecorder.ini, in the same format as the Python version:
 * people who switch versions keep their settings.
 */
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "fx.h"

#define CONFIG_FILE L"VBRecorder.ini"

/* Program version, in a single place. It stays out of the translatable texts on
   purpose: bumping the version does not invalidate any translation. */
#define VB_VERSION L"1.4.6"

/* Recording buffer, in milliseconds. People with a busy computer can raise it
   so the audio does not crackle. */
const int BUFFER_DEFAULT = 500;
const int BUFFER_MIN = 100;
const int BUFFER_MAX = 5000;

struct SourceEntry {
    std::wstring type;  // "device", "process", "exe" or "file"
    int id = 0;         // device index or process id
    std::wstring exe;   // executable name, for "exe"
    double volume = 1.0;
    FxSettings fx;
    std::wstring path;  // file or folder, for "file"
    bool loop = true;
    bool shuffle = false;
};

/* A recording source. An "exe" source is every process with that executable
   name, whenever it runs; its name is the executable name (jogo.exe). A
   "file" source is background audio: its name is the file or folder played,
   loop says whether it starts over when it ends, and shuffle whether a
   folder plays in a random order. */
struct Source {
    std::wstring type;
    int id = 0;
    std::wstring name;
    double volume = 1.0;
    FxSettings fx;  // equalizer, effects, noise remover and stereo
    bool loop = true;
    bool shuffle = false;
};


/* Whether two sources are the same device, process or executable. */
bool SameSource(const Source& a, const Source& b);

class ConfigManager {
public:
    ConfigManager();

    void Load();
    void Save();

    void SetConfig(const std::wstring& key, const std::wstring& value);
    void SetConfig(const std::wstring& key, bool value);
    void SetConfig(const std::wstring& key, int value);
    void SetKey(const std::wstring& key, const std::wstring& value);

    /* Value from the CONFIG section; empty if the key does not exist. */
    std::wstring Get(const std::wstring& key) const;
    std::wstring GetKey(const std::wstring& key) const;
    bool GetBoolean(const std::wstring& key, bool fallback = false) const;

    /* Buffer in milliseconds, always within the accepted range. */
    int GetBuffer() const;

    std::vector<SourceEntry> GetSources() const;
    void SetSources(const std::vector<Source>& sources);

private:
    typedef std::vector<std::pair<std::wstring, std::wstring>> Items;
    struct Section {
        std::wstring name;
        Items items;
    };

    Section* Find(const std::wstring& name);
    const Section* Find(const std::wstring& name) const;
    Section& Ensure(const std::wstring& name);
    static const std::wstring* Lookup(const Items& items, const std::wstring& key);
    static void Put(Items& items, const std::wstring& key, const std::wstring& value);

    std::vector<Section> sections_;
};
