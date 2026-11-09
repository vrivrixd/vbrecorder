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
#include <cmath>
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

/* "Faixa 2" before "Faixa 10", as in Explorer. */
void SortAsExplorer(std::vector<std::wstring>& names) {
    std::sort(names.begin(), names.end(), [](const std::wstring& a, const std::wstring& b) {
        return CompareStringEx(LOCALE_NAME_USER_DEFAULT, LINGUISTIC_IGNORECASE | SORT_DIGITSASNUMBERS,
                               a.c_str(), (int)a.size(), b.c_str(), (int)b.size(), nullptr,
                               nullptr, 0) == CSTR_LESS_THAN;
    });
}

void AddFolder(const std::wstring& folder, bool subfolders, std::vector<std::wstring>& files) {
    std::vector<std::wstring> names, folders;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(JoinPath(folder, L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (IsAudioFile(fd.cFileName)) names.push_back(fd.cFileName);
        } else if (subfolders && wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0 &&
                   // A link to another folder could lead back to this one,
                   // and hidden folders are not the user's.
                   !(fd.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_HIDDEN |
                                            FILE_ATTRIBUTE_SYSTEM))) {
            folders.push_back(fd.cFileName);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    SortAsExplorer(names);
    for (auto& n : names) files.push_back(JoinPath(folder, n));
    SortAsExplorer(folders);
    for (auto& f : folders) AddFolder(JoinPath(folder, f), true, files);
}

}  // namespace

std::vector<std::wstring> BackgroundFiles(const std::wstring& path, bool subfolders) {
    std::vector<std::wstring> files;
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return files;
    if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        files.push_back(path);
        return files;
    }
    AddFolder(path, subfolders, files);
    return files;
}

std::wstring BackgroundPattern() {
    std::wstring pattern;
    for (const wchar_t* t : AUDIO_TYPES) pattern += (pattern.empty() ? L"*." : L";*.") + std::wstring(t);
    return pattern;
}

BackgroundPlayer::BackgroundPlayer(const BassApi& bass, const std::wstring& path, bool loop,
                                   bool shuffle, bool subfolders, int rate, int chans)
    : b_(bass), path_(path), files_(BackgroundFiles(path, subfolders)), loop_(loop),
      shuffle_(shuffle), subfolders_(subfolders), rate_(rate), chans_(chans) {
    NewOrder();
}

void BackgroundPlayer::SetSubfolders(bool subfolders) {
    if (subfolders == subfolders_) return;
    subfolders_ = subfolders;
    const bool playing_any = cur_.stream || next_.stream;
    std::wstring playing = playing_any && index_ < order_.size() ? files_[order_[index_]] : std::wstring();
    files_ = BackgroundFiles(path_, subfolders_);
    order_.clear();
    NewOrder();
    // The file playing keeps its place in the new list: what comes after it
    // in the name order, or a shuffled round that starts with it.
    auto found = std::find(files_.begin(), files_.end(), playing);
    if (found == files_.end()) {
        // It is no longer in the list: when it ends, the list starts.
        index_ = (size_t)-1;
        return;
    }
    size_t at = std::find(order_.begin(), order_.end(), (size_t)(found - files_.begin())) - order_.begin();
    if (shuffle_) {
        std::swap(order_[0], order_[at]);
        at = 0;
    }
    index_ = at;
}

void BackgroundPlayer::NewOrder() {
    size_t last = index_ < order_.size() ? order_[index_] : files_.size();
    order_.resize(files_.size());
    std::iota(order_.begin(), order_.end(), 0);
    if (!shuffle_ || files_.size() < 2) return;
    static std::mt19937 random{std::random_device{}()};
    std::shuffle(order_.begin(), order_.end(), random);
    // The same song twice in a row, at the turn of a round, sounds like a mistake.
    if (order_[0] == last) std::swap(order_[0], order_[1 + random() % (order_.size() - 1)]);
}

BackgroundPlayer::~BackgroundPlayer() {
    CloseVoice(next_);
    CloseVoice(cur_);
}

void BackgroundPlayer::SetCrossfade(double ahead, double fade_out, double fade_in) {
    ahead_ = std::max(0.0, ahead);
    fade_out_ = std::max(0.0, fade_out);
    fade_in_ = std::max(0.0, fade_in);
}

bool BackgroundPlayer::Open(const Place* from, int* error) {
    *error = BASS_ERROR_FILEOPEN;
    if (files_.empty()) return false;
    if (from && from->index >= files_.size()) {
        // It had ended, without repeat: it stays silent.
        done_ = true;
        return true;
    }
    // The same order as before, unless the folder changed meanwhile.
    if (from && from->order.size() == files_.size()) order_ = from->order;
    if (from && OpenAt(cur_, from->index, from->pos, false)) return true;
    for (size_t i = 0; i < files_.size(); ++i) {
        if (OpenAt(cur_, i, 0, false)) return true;
        *error = b_.ErrorGetCode();
    }
    return false;
}

bool BackgroundPlayer::OpenAt(Voice& v, size_t index, QWORD pos, bool fade_in) {
    CloseVoice(v);
    // Without BASS_MIXER_NONSTOP, a read stops short where the file ends:
    // that is where the next one comes in.
    DWORD mixer = b_.MixerStreamCreate(rate_, chans_, BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE);
    if (!mixer) return false;
    DWORD s = b_.StreamCreateFile(FALSE, files_[order_[index]].c_str(), 0, 0,
                                  BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT | BASS_UNICODE);
    if (!s) {
        b_.StreamFree(mixer);
        return false;
    }
    if (pos) b_.ChannelSetPosition(s, pos, BASS_POS_BYTE);
    if (!b_.MixerStreamAddChannel(mixer, s, BASS_MIXER_CHAN_DOWNMIX | BASS_MIXER_CHAN_NORAMPIN)) {
        b_.StreamFree(s);
        b_.StreamFree(mixer);
        return false;
    }
    v.mixer = mixer;
    v.stream = s;
    v.index = index;
    v.fade_in = fade_in;
    // Where it is and how long it is, in frames of the rate wanted: the
    // crossfade counts from its end.
    v.length = -1;
    v.pos = 0;
    if (b_.ChannelGetLength && b_.ChannelBytes2Seconds) {
        QWORD bytes = b_.ChannelGetLength(s, BASS_POS_BYTE);
        double seconds = bytes == (QWORD)-1 ? -1 : b_.ChannelBytes2Seconds(s, bytes);
        if (seconds > 0) v.length = (long long)(seconds * rate_);
        if (pos) {
            double at = b_.ChannelBytes2Seconds(s, pos);
            if (at > 0) v.pos = (long long)(at * rate_);
        }
    }
    index_ = index;
    return true;
}

void BackgroundPlayer::CloseVoice(Voice& v) {
    if (v.stream) {
        b_.MixerChannelRemove(v.stream);
        b_.StreamFree(v.stream);
    }
    if (v.mixer) b_.StreamFree(v.mixer);
    v = Voice();
}

bool BackgroundPlayer::Next(Voice& v) {
    // Files that do not play are skipped; one round through the list at most.
    for (size_t tries = 0; tries < files_.size(); ++tries) {
        size_t next = index_ + 1;
        if (next >= files_.size()) {
            if (!loop_) return false;
            NewOrder();
            next = 0;
        }
        index_ = next;
        if (OpenAt(v, next, 0, true)) return true;
    }
    return false;
}

DWORD BackgroundPlayer::ReadVoice(Voice& v, float* out, DWORD frames) {
    if (!v.stream || !frames) return 0;
    const DWORD frame_bytes = chans_ * sizeof(float);
    if (scratch_.size() < (size_t)frames * chans_) scratch_.resize((size_t)frames * chans_);
    DWORD got = b_.ChannelGetData(v.mixer, scratch_.data(), frames * frame_bytes);
    got = got == (DWORD)-1 ? 0 : got / frame_bytes;
    const double half_pi = 1.5707963267948966;
    const long long fade_in = (long long)(fade_in_ * rate_), fade_out = (long long)(fade_out_ * rate_);
    for (DWORD f = 0; f < got; ++f) {
        const long long p = v.pos + f;
        // A quarter of a sine: what goes down and what comes up keep the
        // loudness steady where they overlap.
        double gain = 1.0;
        if (v.fade_in && fade_in > 0 && p < fade_in) gain *= std::sin(half_pi * (double)p / fade_in);
        if (fade_out > 0 && v.length > 0) {
            const long long left = v.length - p;
            if (left < fade_out) gain *= std::sin(half_pi * (double)std::max(0LL, left) / fade_out);
        }
        float* src = scratch_.data() + (size_t)f * chans_;
        float* dst = out + (size_t)f * chans_;
        for (int c = 0; c < chans_; ++c) dst[c] += (float)(src[c] * gain);
    }
    v.pos += got;
    return got;
}

void BackgroundPlayer::Read(float* out, DWORD frames) {
    std::memset(out, 0, (size_t)frames * chans_ * sizeof(float));
    DWORD filled = 0;
    // A file with nothing in it must not keep this loop going: as many empty
    // files in a row as there are files means the list has nothing to play.
    size_t empty = 0;
    const long long ahead = (long long)(ahead_ * rate_);
    while (!done_ && cur_.stream && filled < frames && empty <= files_.size()) {
        DWORD n = frames - filled;
        if (!next_.stream && !last_ && ahead > 0 && cur_.length > 0) {
            // The next file starts this long before the end of this one; a
            // short file overlaps by half its length at most.
            const long long start = cur_.length - std::min(ahead, cur_.length / 2);
            if (cur_.pos >= start) {
                if (!Next(next_)) last_ = true;
            } else {
                n = (DWORD)std::min<long long>(n, start - cur_.pos);
            }
        }
        DWORD got = ReadVoice(cur_, out + (size_t)filled * chans_, n);
        if (next_.stream && ReadVoice(next_, out + (size_t)filled * chans_, got) < got) {
            // The next one was shorter than the overlap: it is over already.
            CloseVoice(next_);
        }
        filled += got;
        if (got == n) continue;
        // The file is over: the next one goes on, or starts now.
        empty = got ? 0 : empty + 1;
        CloseVoice(cur_);
        last_ = false;
        if (next_.stream) {
            cur_ = next_;
            next_ = Voice();
        } else if (!Next(cur_)) {
            done_ = true;
        }
    }
}

void BackgroundPlayer::SetLoop(bool loop) {
    loop_ = loop;
    last_ = false;  // with repeat on, there is a next file again
    if (loop_ && done_ && !files_.empty()) {
        done_ = false;
        index_ = files_.size() - 1;  // Next() goes round to the first
        if (!Next(cur_)) done_ = true;
    }
}

BackgroundPlayer::Place BackgroundPlayer::Where() const {
    Place p;
    p.index = index_ < order_.size() ? index_ : 0;
    p.order = order_;
    // The newest file playing: in a crossfade, the one coming in.
    const Voice& v = next_.stream ? next_ : cur_;
    if (v.stream && index_ < order_.size()) {
        QWORD pos = b_.ChannelGetPosition(v.stream, BASS_POS_BYTE);
        if (pos != (QWORD)-1) p.pos = pos;
    } else if (done_) {
        // It had ended: it stays ended.
        p.index = files_.size();
    }
    return p;
}
