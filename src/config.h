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
#include "schedule.h"
#include "tags.h"

#define CONFIG_FILE L"VBRecorder.ini"

/* Program version, in a single place. It stays out of the translatable texts on
   purpose: bumping the version does not invalidate any translation. */
#define VB_VERSION L"1.5.2"

/* Recording buffer, in milliseconds: how long a capture may be held up without
   losing audio. The default is the one BASS itself uses for recording. */
const int BUFFER_DEFAULT = 2000;
const int BUFFER_MIN = 100;
const int BUFFER_MAX = 5000;

/* Pre-recording, in seconds: 0 is off. */
const int PREROLL_MAX = 60;

/* The silence detection: off unless turned on. Sound is audio at or above
   threshold_db (dBFS); after silence_ms without it, the recording pauses,
   or stops; and it starts, or goes on, preroll_ms before the sound that
   came. tones: a tone on each of those, which happen by themselves. */
const int SILENCE_DB_MIN = -60;
const int SILENCE_PREROLL_MAX = 300;
const int SILENCE_MIN_MS = 500, SILENCE_MAX_MS = 60000;
struct SilenceSettings {
    bool on = false;
    int threshold_db = -20;
    int preroll_ms = 150;
    int silence_ms = 2000;
    bool stop = false;  // false: pauses
    bool tones = false;
};

/* Bit depth value for 32-bit floating point, in wav and aiff. */
const int BITS_FLOAT = 0;

struct SourceEntry {
    std::wstring type;  // "device", "process", "exe" or "file"
    int id = 0;         // device index or process id
    std::wstring exe;   // executable name, for "exe"
    double volume = 1.0;
    FxSettings fx;
    std::wstring path;  // file or folder, for "file"
    bool loop = true;
    bool shuffle = false;
    bool subfolders = false;
    double pan = 0.0;  // -1 left, 0 center, +1 right
    bool listen = false;  // background audio or stream URL: also played while recording
};

/* A recording source. An "exe" source is every process with that executable
   name, whenever it runs; its name is the executable name (jogo.exe). A
   "file" source is background audio: its name is the file or folder played,
   loop says whether it starts over when it ends, shuffle whether a folder
   plays in a random order, and subfolders whether the audio of the folders
   inside it plays too. pan is the balance between left and right. listen:
   background audio or a stream URL is also played, to be heard, while it
   is recorded. */
struct Source {
    std::wstring type;
    int id = 0;
    std::wstring name;
    double volume = 1.0;
    FxSettings fx;  // equalizer, effects, noise remover and stereo
    bool loop = true;
    bool shuffle = false;
    bool subfolders = false;
    double pan = 0.0;  // -1 left, 0 center, +1 right
    bool listen = false;
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
    /* The tags, in [TAGS]: a key that is not there has its default (title
       $name, artist VB Recorder); one that is there but empty writes no tag. */
    TagValues GetTags() const;
    void SetTags(const TagValues& tags);
    /* How often a recording is split into files, in seconds; 0 is never. */
    int GetSplitSeconds() const;
    /* The pre-recording, in seconds; 0 is off. */
    int GetPreroll() const;
    /* The silence detection, in [DETECCAO]. */
    SilenceSettings GetSilence() const;
    void SetSilence(const SilenceSettings& settings);
    void SetSources(const std::vector<Source>& sources);

    /* The scheduled recordings, in their own section:
       [AGENDAMENTOS] 1 = 2026-12-31|14:30|1|1:30:00, with no date for every
       day, 1 when it leaves the list once it starts, and its duration, if it
       has one. */
    std::vector<Schedule> GetSchedules() const;
    void SetSchedules(const std::vector<Schedule>& schedules);

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
