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

#include "net_source.h"

#include <algorithm>
#include <atomic>
#include <thread>

#include "util.h"

namespace {

/* How long to wait before trying again after a connection failed or
   dropped, in ms; and how often the connection is checked. */
const DWORD RETRY_MS = 3000;
const DWORD CHECK_MS = 500;

bool IsUrl(const std::wstring& s) {
    std::wstring low = ToLower(s);
    return low.compare(0, 7, L"http://") == 0 || low.compare(0, 8, L"https://") == 0 ||
           low.compare(0, 6, L"ftp://") == 0;
}

}  // namespace

/* Shared by the source and its thread: whichever ends last frees it. The
   source may end while BASS is still connecting, and the recording does not
   wait for that. */
struct NetSource::State {
    const BassApi* b = nullptr;
    std::wstring url;
    DWORD mixer = 0;
    std::atomic<DWORD> stream{0};
    HANDLE stop = nullptr;

    ~State() {
        DWORD s = stream.exchange(0);
        if (s) {
            b->MixerChannelRemove(s);
            b->StreamFree(s);
        }
        if (mixer) b->StreamFree(mixer);
        if (stop) CloseHandle(stop);
    }
};

NetSource::NetSource(const BassApi& bass, const std::wstring& url, int rate, int chans) : chans_(chans) {
    state_ = std::make_shared<State>();
    state_->b = &bass;
    state_->url = url;
    // The mixer gives silence while it has nothing, and converts the
    // stream to the recording's rate and channels.
    state_->mixer = bass.MixerStreamCreate(rate, chans, BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE | BASS_MIXER_NONSTOP);
    state_->stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    std::thread(Run, state_).detach();
}

NetSource::~NetSource() { SetEvent(state_->stop); }

DWORD NetSource::Open(const BassApi& b, const std::wstring& url, DWORD flags) {
    std::wstring u = Trim(url);
    if (u.size() >= 2 && u.front() == L'"' && u.back() == L'"') u = u.substr(1, u.size() - 2);
    if (IsUrl(u)) return b.StreamCreateURL(u.c_str(), 0, flags | BASS_UNICODE, nullptr, nullptr);
    // A playlist on the computer: BASS follows the addresses in it.
    return b.StreamCreateFile(FALSE, u.c_str(), 0, 0, flags | BASS_UNICODE);
}

void NetSource::Read(float* out, DWORD frames) {
    DWORD bytes = frames * chans_ * sizeof(float);
    DWORD got = state_->mixer ? state_->b->ChannelGetData(state_->mixer, out, bytes) : 0;
    if (got == (DWORD)-1) got = 0;
    if (got < bytes) std::fill((char*)out + got, (char*)out + bytes, 0);
}

void NetSource::Run(std::shared_ptr<State> st) {
    const BassApi& b = *st->b;
    DWORD wait = 0;
    while (WaitForSingleObject(st->stop, wait) == WAIT_TIMEOUT) {
        wait = CHECK_MS;
        DWORD s = st->stream.load();
        // The stream ended: the server closed it, or the connection dropped.
        if (s && b.ChannelIsActive(s) == BASS_ACTIVE_STOPPED) {
            st->stream = 0;
            b.MixerChannelRemove(s);
            b.StreamFree(s);
            s = 0;
        }
        if (s) continue;
        DWORD fresh = Open(b, st->url, BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);
        if (!fresh) {
            wait = RETRY_MS;
            continue;
        }
        if (WaitForSingleObject(st->stop, 0) == WAIT_OBJECT_0 ||
            !b.MixerStreamAddChannel(st->mixer, fresh, BASS_MIXER_CHAN_DOWNMIX | BASS_MIXER_CHAN_NORAMPIN)) {
            b.StreamFree(fresh);
            wait = RETRY_MS;
            continue;
        }
        st->stream = fresh;
    }
}
