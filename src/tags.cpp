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

#include "tags.h"

#include <lmcons.h>

#include <algorithm>
#include <cwctype>

#include "util.h"

bool TagValues::Empty() const {
    for (auto& v : value)
        if (!v.empty()) return false;
    return true;
}

const std::vector<TagVariable>& TagVariables() {
    static const std::vector<TagVariable> vars = {
        {L"$name", "Nome do Arquivo"},
        {L"$format", "Formato"},
        {L"$part", "Número da Parte"},
        {L"$date", "Data"},
        {L"$year", "Ano"},
        {L"$month", "Mês"},
        {L"$day", "Dia"},
        {L"$time", "Hora"},
        {L"$user", "Nome do Usuário"},
        {L"$computer", "Nome do Computador"},
    };
    return vars;
}

const wchar_t* TagKey(int field) {
    static const wchar_t* keys[TAG_COUNT] = {L"titulo", L"artista", L"artista_album", L"album",
                                             L"ano",    L"genero",  L"faixa",         L"comentario"};
    return keys[field];
}

const wchar_t* TagDefault(int field) {
    if (field == TAG_TITLE) return L"$name";
    if (field == TAG_ARTIST || field == TAG_ALBUM_ARTIST) return L"VB Recorder";
    return L"";
}

namespace {

std::wstring Two(int n) {
    wchar_t buf[8];
    swprintf(buf, 8, L"%02d", n);
    return buf;
}

std::wstring UserName() {
    wchar_t buf[UNLEN + 1];
    DWORD n = UNLEN + 1;
    return GetUserNameW(buf, &n) ? std::wstring(buf) : std::wstring();
}

std::wstring ComputerName() {
    wchar_t buf[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD n = MAX_COMPUTERNAME_LENGTH + 1;
    return GetComputerNameW(buf, &n) ? std::wstring(buf) : std::wstring();
}

std::wstring Value(const std::wstring& code, const TagContext& c) {
    const SYSTEMTIME& t = c.start;
    if (code == L"$name") return c.name;
    if (code == L"$format") {
        std::wstring f = c.format;
        for (auto& ch : f) ch = (wchar_t)towupper(ch);
        return f;
    }
    if (code == L"$part") return std::to_wstring(c.part);
    if (code == L"$date") return std::to_wstring(t.wYear) + L"-" + Two(t.wMonth) + L"-" + Two(t.wDay);
    if (code == L"$year") return std::to_wstring(t.wYear);
    if (code == L"$month") return Two(t.wMonth);
    if (code == L"$day") return Two(t.wDay);
    if (code == L"$time") return Two(t.wHour) + L":" + Two(t.wMinute);
    if (code == L"$user") return UserName();
    if (code == L"$computer") return ComputerName();
    return code;
}

/* For the encoders' command lines: in quotes, and a quote inside would end
   them, so it becomes an apostrophe. */
std::wstring Quoted(const std::wstring& text) {
    std::wstring s = text;
    std::replace(s.begin(), s.end(), L'"', L'\'');
    // A backslash before the closing quote would escape it.
    while (!s.empty() && s.back() == L'\\') s.pop_back();
    return L"\"" + s + L"\"";
}

bool Digits(const std::wstring& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; });
}

/* "3" or "3/12": a track number the players understand. */
bool TrackNumber(const std::wstring& s) {
    size_t slash = s.find(L'/');
    if (slash == std::wstring::npos) return Digits(s);
    return Digits(s.substr(0, slash)) && Digits(s.substr(slash + 1));
}

void Le32(std::string& out, DWORD v) {
    for (int i = 0; i < 4; ++i) out += (char)((v >> (8 * i)) & 0xFF);
}

void Be32(std::string& out, DWORD v) {
    for (int i = 3; i >= 0; --i) out += (char)((v >> (8 * i)) & 0xFF);
}

}  // namespace

std::wstring ExpandTags(const std::wstring& text, const TagContext& ctx) {
    std::wstring out;
    for (size_t i = 0; i < text.size();) {
        if (text[i] == L'$') {
            // The longest variable that is there, in any case.
            const TagVariable* best = nullptr;
            for (auto& v : TagVariables()) {
                size_t n = wcslen(v.code);
                if (i + n <= text.size() && ToLower(text.substr(i, n)) == v.code &&
                    (!best || n > wcslen(best->code)))
                    best = &v;
            }
            if (best) {
                out += Value(best->code, ctx);
                i += wcslen(best->code);
                continue;
            }
        }
        out += text[i++];
    }
    return out;
}

TagValues ExpandTags(const TagValues& raw, const TagContext& ctx) {
    TagValues out;
    for (int f = 0; f < TAG_COUNT; ++f) out.value[f] = Trim(ExpandTags(raw.value[f], ctx));
    return out;
}

std::wstring TagOptions(const std::wstring& format, const TagValues& t) {
    std::wstring o;
    auto add = [&](const wchar_t* opt, const std::wstring& v) {
        if (!v.empty()) o += L" " + std::wstring(opt) + L" " + Quoted(v);
    };
    const std::wstring* v = t.value;
    if (format == L"mp3") {
        // ID3v2 in UTF-16, so any language's letters come through.
        if (t.Empty()) return o;
        o += L" --id3v2-only --id3v2-utf16";
        add(L"--tt", v[TAG_TITLE]);
        add(L"--ta", v[TAG_ARTIST]);
        if (!v[TAG_ALBUM_ARTIST].empty()) add(L"--tv", L"TPE2=" + v[TAG_ALBUM_ARTIST]);
        add(L"--tl", v[TAG_ALBUM]);
        if (!v[TAG_YEAR].empty()) add(L"--tv", L"TYER=" + v[TAG_YEAR]);
        add(L"--tg", v[TAG_GENRE]);
        if (TrackNumber(v[TAG_TRACK])) add(L"--tn", v[TAG_TRACK]);
        add(L"--tc", v[TAG_COMMENT]);
    } else if (format == L"ogg") {
        add(L"-t", v[TAG_TITLE]);
        add(L"-a", v[TAG_ARTIST]);
        if (!v[TAG_ALBUM_ARTIST].empty()) add(L"-c", L"ALBUMARTIST=" + v[TAG_ALBUM_ARTIST]);
        add(L"-l", v[TAG_ALBUM]);
        add(L"-d", v[TAG_YEAR]);
        add(L"-G", v[TAG_GENRE]);
        add(L"-N", v[TAG_TRACK]);
        if (!v[TAG_COMMENT].empty()) add(L"-c", L"COMMENT=" + v[TAG_COMMENT]);
    } else if (format == L"opus") {
        add(L"--title", v[TAG_TITLE]);
        add(L"--artist", v[TAG_ARTIST]);
        if (!v[TAG_ALBUM_ARTIST].empty()) add(L"--comment", L"ALBUMARTIST=" + v[TAG_ALBUM_ARTIST]);
        add(L"--album", v[TAG_ALBUM]);
        add(L"--date", v[TAG_YEAR]);
        add(L"--genre", v[TAG_GENRE]);
        if (!v[TAG_TRACK].empty()) add(L"--comment", L"TRACKNUMBER=" + v[TAG_TRACK]);
        if (!v[TAG_COMMENT].empty()) add(L"--comment", L"COMMENT=" + v[TAG_COMMENT]);
    } else if (format == L"flac") {
        static const wchar_t* names[TAG_COUNT] = {L"TITLE", L"ARTIST", L"ALBUMARTIST", L"ALBUM",
                                                  L"DATE",  L"GENRE",  L"TRACKNUMBER", L"COMMENT"};
        for (int f = 0; f < TAG_COUNT; ++f)
            if (!v[f].empty()) add(L"-T", std::wstring(names[f]) + L"=" + v[f]);
    }
    return o;
}

std::string WavInfoChunk(const TagValues& t) {
    // INFO has no album artist.
    static const char* ids[TAG_COUNT] = {"INAM", "IART", nullptr, "IPRD", "ICRD", "IGNR", "ITRK", "ICMT"};
    std::string body = "INFO";
    for (int f = 0; f < TAG_COUNT; ++f) {
        if (t.value[f].empty() || !ids[f]) continue;
        // UTF-8, as the players that read these tags today expect.
        std::string text = Narrow(t.value[f], CP_UTF8);
        text += '\0';
        body += ids[f];
        Le32(body, (DWORD)text.size());
        body += text;
        if (text.size() % 2) body += '\0';
    }
    return body == "INFO" ? std::string() : body;
}

std::vector<std::pair<std::string, std::string>> AiffChunks(const TagValues& t) {
    std::vector<std::pair<std::string, std::string>> chunks;
    if (t.Empty()) return chunks;
    // The ones AIFF itself has, for the players that read only those.
    auto text = [&](const char* id, const std::wstring& v) {
        if (!v.empty()) chunks.emplace_back(id, Narrow(v, CP_UTF8));
    };
    text("NAME", t.value[TAG_TITLE]);
    text("AUTH", t.value[TAG_ARTIST]);
    text("ANNO", t.value[TAG_COMMENT]);
    // And all of them in an ID3v2.3 chunk, as iTunes and most players read.
    chunks.emplace_back("ID3 ", Id3Tag(t));
    return chunks;
}

std::string Id3Tag(const TagValues& t) {
    static const char* frames[TAG_COUNT] = {"TIT2", "TPE1", "TPE2", "TALB", "TYER", "TCON", "TRCK", "COMM"};
    auto utf16 = [](const std::wstring& s) {
        std::string b = "\xFF\xFE";  // byte order mark, little endian
        for (wchar_t c : s) {
            b += (char)(c & 0xFF);
            b += (char)(c >> 8);
        }
        return b;
    };
    std::string body;
    for (int f = 0; f < TAG_COUNT; ++f) {
        if (t.value[f].empty()) continue;
        std::string data = "\x01";  // UTF-16
        if (f == TAG_COMMENT) data += std::string("XXX", 3) + utf16(L"") + std::string(2, '\0');
        data += utf16(t.value[f]);
        body += frames[f];
        Be32(body, (DWORD)data.size());
        body += std::string(2, '\0');  // flags
        body += data;
    }
    std::string tag = "ID3";
    tag += '\x03';
    tag += '\0';
    tag += '\0';  // version 2.3, no flags
    DWORD n = (DWORD)body.size();
    tag += (char)((n >> 21) & 0x7F);  // size, 7 bits a byte
    tag += (char)((n >> 14) & 0x7F);
    tag += (char)((n >> 7) & 0x7F);
    tag += (char)(n & 0x7F);
    return tag + body;
}

std::vector<std::pair<const wchar_t*, std::wstring>> WmaTags(const TagValues& t) {
    // In UTF-16, the names as much as the values.
    static const wchar_t* names[TAG_COUNT] = {L"Title",   L"Author",   L"WM/AlbumArtist", L"WM/AlbumTitle",
                                              L"WM/Year", L"WM/Genre", L"WM/TrackNumber", L"Description"};
    std::vector<std::pair<const wchar_t*, std::wstring>> tags;
    for (int f = 0; f < TAG_COUNT; ++f)
        if (!t.value[f].empty()) tags.emplace_back(names[f], t.value[f]);
    return tags;
}
