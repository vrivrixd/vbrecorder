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

#include "background.h"

#include <algorithm>
#include <cstring>
#include <numeric>
#include <random>

#include "util.h"

namespace {

/* What BASS plays by itself or with the plugins that come with the program;
   the last ones through Windows (Media Foundation). */
const wchar_t* const AUDIO_TYPES[] = {L"mp3", L"mp2", L"mp1", L"ogg", L"wav", L"aif", L"aiff",
                                      L"flac", L"opus", L"m4a", L"aac", L"wma"};

bool IsAudioFile(const std::wstring& name) {
    std::wstring base, ext;
    SplitExt(name, &base, &ext);
    if (ext.size() < 2) return false;
    std::wstring type = ToLower(ext.substr(1));
    for (const wchar_t* t : AUDIO_TYPES)
        if (type == t) return true;
    return false;
}

}  // namespace

std::vector<std::wstring> BackgroundFiles(const std::wstring& path) {
    std::vector<std::wstring> files;
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return files;
    if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        files.push_back(path);
        return files;
    }
    std::vector<std::wstring> names;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(JoinPath(path, L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return files;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && IsAudioFile(fd.cFileName))
            names.push_back(fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    // "Faixa 2" before "Faixa 10", as in Explorer.
    std::sort(names.begin(), names.end(), [](const std::wstring& a, const std::wstring& b) {
        return CompareStringEx(LOCALE_NAME_USER_DEFAULT, LINGUISTIC_IGNORECASE | SORT_DIGITSASNUMBERS,
                               a.c_str(), (int)a.size(), b.c_str(), (int)b.size(), nullptr,
                               nullptr, 0) == CSTR_LESS_THAN;
    });
    for (auto& n : names) files.push_back(JoinPath(path, n));
    return files;
}

std::wstring BackgroundPattern() {
    std::wstring pattern;
    for (const wchar_t* t : AUDIO_TYPES) pattern += (pattern.empty() ? L"*." : L";*.") + std::wstring(t);
    return pattern;
}

BackgroundPlayer::BackgroundPlayer(const BassApi& bass, const std::wstring& path, bool loop,
                                   bool shuffle, int rate, int chans)
    : b_(bass), files_(BackgroundFiles(path)), loop_(loop), shuffle_(shuffle), rate_(rate),
      chans_(chans) {
    NewOrder();
}

void BackgroundPlayer::NewOrder() {
    size_t last = order_.empty() ? files_.size() : order_[index_];
    order_.resize(files_.size());
    std::iota(order_.begin(), order_.end(), 0);
    if (!shuffle_ || files_.size() < 2) return;
    static std::mt19937 random{std::random_device{}()};
    std::shuffle(order_.begin(), order_.end(), random);
    // The same song twice in a row, at the turn of a round, sounds like a mistake.
    if (order_[0] == last) std::swap(order_[0], order_[1 + random() % (order_.size() - 1)]);
}

BackgroundPlayer::~BackgroundPlayer() {
    Close();
    if (mixer_) b_.StreamFree(mixer_);
}

bool BackgroundPlayer::Open(const Place* from, int* error) {
    *error = BASS_ERROR_FILEOPEN;
    if (files_.empty()) return false;
    // Without BASS_MIXER_NONSTOP, a read stops short where the file ends:
    // that is where the next one comes in.
    mixer_ = b_.MixerStreamCreate(rate_, chans_, BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE);
    if (!mixer_) {
        *error = b_.ErrorGetCode();
        return false;
    }
    if (from && from->index >= files_.size()) {
        // It had ended, without repeat: it stays silent.
        done_ = true;
        return true;
    }
    // The same order as before, unless the folder changed meanwhile.
    if (from && from->order.size() == files_.size()) order_ = from->order;
    if (from && OpenAt(from->index, from->pos)) return true;
    for (size_t i = 0; i < files_.size(); ++i) {
        if (OpenAt(i, 0)) return true;
        *error = b_.ErrorGetCode();
    }
    return false;
}

bool BackgroundPlayer::OpenAt(size_t index, QWORD pos) {
    Close();
    DWORD s = b_.StreamCreateFile(FALSE, files_[order_[index]].c_str(), 0, 0,
                                  BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT | BASS_UNICODE);
    if (!s) return false;
    if (pos) b_.ChannelSetPosition(s, pos, BASS_POS_BYTE);
    if (!b_.MixerStreamAddChannel(mixer_, s, BASS_MIXER_CHAN_DOWNMIX | BASS_MIXER_CHAN_NORAMPIN)) {
        b_.StreamFree(s);
        return false;
    }
    stream_ = s;
    index_ = index;
    return true;
}

void BackgroundPlayer::Close() {
    if (!stream_) return;
    b_.MixerChannelRemove(stream_);
    b_.StreamFree(stream_);
    stream_ = 0;
}

bool BackgroundPlayer::Next() {
    // Files that do not play are skipped; one round through the list at most.
    for (size_t tries = 0; tries < files_.size(); ++tries) {
        size_t next = index_ + 1;
        if (next >= files_.size()) {
            if (!loop_) return false;
            NewOrder();
            next = 0;
        }
        index_ = next;
        if (OpenAt(next, 0)) return true;
    }
    return false;
}

void BackgroundPlayer::Read(float* out, DWORD frames) {
    const DWORD frame_bytes = chans_ * sizeof(float);
    DWORD filled = 0;
    // A file with nothing in it must not keep this loop going: as many empty
    // files in a row as there are files means the list has nothing to play.
    size_t empty = 0;
    while (!done_ && stream_ && filled < frames && empty <= files_.size()) {
        DWORD got = b_.ChannelGetData(mixer_, out + (size_t)filled * chans_, (frames - filled) * frame_bytes);
        if (got == (DWORD)-1) got = 0;
        filled += got / frame_bytes;
        if (filled >= frames) break;
        // The file is over.
        empty = got ? 0 : empty + 1;
        if (!Next()) {
            Close();
            done_ = true;
        }
    }
    if (filled < frames) std::memset(out + (size_t)filled * chans_, 0, (size_t)(frames - filled) * frame_bytes);
}

void BackgroundPlayer::SetLoop(bool loop) {
    loop_ = loop;
    if (loop_ && done_ && mixer_ && !files_.empty()) {
        done_ = false;
        index_ = files_.size() - 1;  // Next() goes round to the first
        if (!Next()) done_ = true;
    }
}

BackgroundPlayer::Place BackgroundPlayer::Where() const {
    Place p;
    p.index = index_;
    p.order = order_;
    if (stream_) {
        QWORD pos = b_.ChannelGetPosition(stream_, BASS_POS_BYTE);
        if (pos != (QWORD)-1) p.pos = pos;
    } else if (done_) {
        // It had ended: it stays ended.
        p.index = files_.size();
    }
    return p;
}
