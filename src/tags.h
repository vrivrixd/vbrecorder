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
 * The tags of the recordings: title, artist and the rest, typed by the user
 * with variables ($name, $date...) that become their values when each file
 * starts. An empty field writes no tag. Every format gets them, each in its
 * own way: options of the encoder (mp3, ogg, opus, flac), chunks of the file
 * (wav, aiff), the WMA writer, or the index of an .m4a.
 */
#pragma once

#include <windows.h>

#include <string>
#include <vector>

/* TAG_ARTIST is the contributing artists, as Windows calls them; TAG_ALBUM_ARTIST
   the artist of the album. */
enum TagField {
    TAG_TITLE,
    TAG_ARTIST,
    TAG_ALBUM_ARTIST,
    TAG_ALBUM,
    TAG_YEAR,
    TAG_GENRE,
    TAG_TRACK,
    TAG_COMMENT,
    TAG_COUNT
};

struct TagValues {
    std::wstring value[TAG_COUNT];
    bool Empty() const;
};

/* What the variables of one file become. */
struct TagContext {
    std::wstring name;    // the file name, without its extension
    std::wstring format;  // the extension: mp3, wav...
    int part = 1;         // its number, when the recording is split
    SYSTEMTIME start = {};  // when the recording started
};

/* A variable and what it is, in the order of the Variables menu. */
struct TagVariable {
    const wchar_t* code;  // $name
    const char* label;    // translatable text
};
const std::vector<TagVariable>& TagVariables();

/* The key of each field in the [TAGS] section of the .ini, and the value it
   has when the key is not there at all. */
const wchar_t* TagKey(int field);
const wchar_t* TagDefault(int field);

std::wstring ExpandTags(const std::wstring& text, const TagContext& ctx);
TagValues ExpandTags(const TagValues& raw, const TagContext& ctx);

/* The encoder options for these tags: LAME (mp3), oggenc (ogg), opusenc
   (opus) or flac. Empty for the other formats. */
std::wstring TagOptions(const std::wstring& format, const TagValues& tags);
/* A RIFF INFO list for a wav file, as the body of its LIST chunk. */
std::string WavInfoChunk(const TagValues& tags);
/* An ID3v2.3 tag with all of them, for the id3 chunk of wav and aiff, which
   INFO and the AIFF text chunks cannot hold (the album artist, for one). */
std::string Id3Tag(const TagValues& tags);
/* The AIFF text chunks: (id, body). */
std::vector<std::pair<std::string, std::string>> AiffChunks(const TagValues& tags);
/* The WMA attribute names: (name, value). */
std::vector<std::pair<const wchar_t*, std::wstring>> WmaTags(const TagValues& tags);
