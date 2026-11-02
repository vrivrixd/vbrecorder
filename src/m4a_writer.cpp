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

// The Media Foundation GUIDs are defined here, so no library of them is linked.
#include <initguid.h>

#include "m4a_writer.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <objbase.h>

#include <algorithm>
#include <cstring>

#include "util.h"

namespace {

/* Media Foundation, loaded when first used. */
struct Mf {
    HRESULT(WINAPI* Startup)(ULONG, DWORD) = nullptr;
    HRESULT(WINAPI* CreateMediaType)(IMFMediaType**) = nullptr;
    HRESULT(WINAPI* CreateSample)(IMFSample**) = nullptr;
    HRESULT(WINAPI* CreateMemoryBuffer)(DWORD, IMFMediaBuffer**) = nullptr;
    HRESULT(WINAPI* CreateAttributes)(IMFAttributes**, UINT32) = nullptr;
    HRESULT(WINAPI* TEnumEx)(GUID, UINT32, const MFT_REGISTER_TYPE_INFO*, const MFT_REGISTER_TYPE_INFO*,
                             IMFActivate***, UINT32*) = nullptr;
    HRESULT(WINAPI* CreateSinkWriterFromURL)(LPCWSTR, IMFByteStream*, IMFAttributes*, IMFSinkWriter**) = nullptr;
    bool ok = false;
};

const Mf& Api() {
    static Mf mf = [] {
        Mf m;
        HMODULE plat = LoadLibraryExW(L"mfplat.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        HMODULE rw = LoadLibraryExW(L"mfreadwrite.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!plat || !rw) return m;
        auto get = [](HMODULE h, const char* name, auto& fn) {
            fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>((void*)GetProcAddress(h, name));
        };
        get(plat, "MFStartup", m.Startup);
        get(plat, "MFCreateMediaType", m.CreateMediaType);
        get(plat, "MFCreateSample", m.CreateSample);
        get(plat, "MFCreateMemoryBuffer", m.CreateMemoryBuffer);
        get(plat, "MFCreateAttributes", m.CreateAttributes);
        get(plat, "MFTEnumEx", m.TEnumEx);
        get(rw, "MFCreateSinkWriterFromURL", m.CreateSinkWriterFromURL);
        m.ok = m.Startup && m.CreateMediaType && m.CreateSample && m.CreateMemoryBuffer && m.CreateAttributes &&
               m.TEnumEx && m.CreateSinkWriterFromURL && SUCCEEDED(m.Startup(MF_VERSION, MFSTARTUP_LITE));
        return m;
    }();
    return mf;
}

template <class T>
void Release(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

/* Media Foundation is called from the thread of the encoder too, which has
   no COM of its own. */
void EnterCom() {
    thread_local bool done = false;
    if (!done) {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        done = true;
    }
}

const LONGLONG HNS = 10000000;  // 100-nanosecond units in a second

}  // namespace

bool M4aWriter::Available() {
    static int available = -1;
    if (available < 0) {
        available = 0;
        const Mf& mf = Api();
        if (mf.ok) {
            MFT_REGISTER_TYPE_INFO out = {MFMediaType_Audio, MFAudioFormat_AAC};
            IMFActivate** found = nullptr;
            UINT32 count = 0;
            if (SUCCEEDED(mf.TEnumEx(MFT_CATEGORY_AUDIO_ENCODER, MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER,
                                     nullptr, &out, &found, &count))) {
                for (UINT32 i = 0; i < count; ++i) found[i]->Release();
                CoTaskMemFree(found);
                available = count > 0;
            }
        }
    }
    return available == 1;
}

const std::vector<int>& M4aWriter::Bitrates() {
    static const std::vector<int> kbps = {96, 128, 160, 192};
    return kbps;
}

const std::vector<int>& M4aWriter::Rates() {
    static const std::vector<int> rates = {44100, 48000};
    return rates;
}

M4aWriter::~M4aWriter() {
    Release(writer_);
    if (lock_made_) DeleteCriticalSection(&lock_);
}

bool M4aWriter::Open(const std::wstring& path, int rate, int chans, int kbps) {
    const Mf& mf = Api();
    if (!mf.ok) return false;
    EnterCom();
    InitializeCriticalSection(&lock_);
    lock_made_ = true;
    rate_ = rate;
    chans_ = chans;
    path_ = path;

    IMFAttributes* attrs = nullptr;
    IMFMediaType *out = nullptr, *in = nullptr;
    bool ok = SUCCEEDED(mf.CreateAttributes(&attrs, 1)) &&
              SUCCEEDED(attrs->SetGUID(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4)) &&
              SUCCEEDED(mf.CreateSinkWriterFromURL(path.c_str(), nullptr, attrs, &writer_));
    // AAC at the bitrate chosen, in bytes per second.
    ok = ok && SUCCEEDED(mf.CreateMediaType(&out)) && SUCCEEDED(out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) &&
         SUCCEEDED(out->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC)) &&
         SUCCEEDED(out->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16)) &&
         SUCCEEDED(out->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, rate)) &&
         SUCCEEDED(out->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, chans)) &&
         SUCCEEDED(out->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, kbps * 1000 / 8)) &&
         SUCCEEDED(writer_->AddStream(out, &stream_));
    ok = ok && SUCCEEDED(mf.CreateMediaType(&in)) && SUCCEEDED(in->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) &&
         SUCCEEDED(in->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM)) &&
         SUCCEEDED(in->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16)) &&
         SUCCEEDED(in->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, rate)) &&
         SUCCEEDED(in->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, chans)) &&
         SUCCEEDED(in->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, chans * 2)) &&
         SUCCEEDED(in->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, rate * chans * 2)) &&
         SUCCEEDED(writer_->SetInputMediaType(stream_, in, nullptr)) && SUCCEEDED(writer_->BeginWriting());
    Release(attrs);
    Release(out);
    Release(in);
    if (!ok) {
        Release(writer_);
        DeleteFileW(path.c_str());
    }
    return ok;
}

void M4aWriter::Write(const void* data, DWORD bytes) {
    if (!writer_ || !data || !bytes) return;
    EnterCom();
    EnterCriticalSection(&lock_);
    const BYTE* p = static_cast<const BYTE*>(data);
    pending_.insert(pending_.end(), p, p + bytes);
    Flush(false);
    LeaveCriticalSection(&lock_);
}

/* In pieces of about a tenth of a second, each with its time. */
void M4aWriter::Flush(bool all) {
    const Mf& mf = Api();
    const size_t frame = (size_t)chans_ * 2;
    const size_t piece = (size_t)(rate_ / 10) * frame;
    size_t at = 0;
    while (pending_.size() - at >= (all ? frame : piece)) {
        size_t n = std::min(piece, (pending_.size() - at) / frame * frame);
        IMFSample* sample = nullptr;
        IMFMediaBuffer* buffer = nullptr;
        BYTE* dst = nullptr;
        if (SUCCEEDED(mf.CreateMemoryBuffer((DWORD)n, &buffer)) && SUCCEEDED(buffer->Lock(&dst, nullptr, nullptr))) {
            std::memcpy(dst, &pending_[at], n);
            buffer->Unlock();
            buffer->SetCurrentLength((DWORD)n);
            long long frames = (long long)(n / frame);
            if (SUCCEEDED(mf.CreateSample(&sample)) && SUCCEEDED(sample->AddBuffer(buffer))) {
                sample->SetSampleTime(frames_ * HNS / rate_);
                sample->SetSampleDuration(frames * HNS / rate_);
                writer_->WriteSample(stream_, sample);
            }
            frames_ += frames;
        }
        Release(sample);
        Release(buffer);
        at += n;
    }
    pending_.erase(pending_.begin(), pending_.begin() + at);
}

bool M4aWriter::Close(const TagValues& tags) {
    if (!writer_) return false;
    EnterCom();
    EnterCriticalSection(&lock_);
    Flush(true);
    bool ok = SUCCEEDED(writer_->Finalize());
    Release(writer_);
    LeaveCriticalSection(&lock_);
    if (ok && !tags.Empty()) WriteM4aTags(path_, tags);
    return ok;
}

namespace {

void Put32(std::string& s, DWORD v) {
    for (int i = 3; i >= 0; --i) s += (char)((v >> (8 * i)) & 0xFF);
}

DWORD Get32(const BYTE* p) { return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16) | ((DWORD)p[2] << 8) | p[3]; }

std::string Box(const char* type, const std::string& body) {
    std::string b;
    Put32(b, (DWORD)(8 + body.size()));
    b.append(type, 4);
    return b + body;
}

/* The boxes in a body: (type, content). */
std::vector<std::pair<std::string, std::string>> Boxes(const std::string& body) {
    std::vector<std::pair<std::string, std::string>> out;
    for (size_t i = 0; i + 8 <= body.size();) {
        DWORD n = Get32((const BYTE*)&body[i]);
        if (n < 8 || i + n > body.size()) break;
        out.emplace_back(body.substr(i + 4, 4), body.substr(i + 8, n - 8));
        i += n;
    }
    return out;
}

std::string Join(const std::vector<std::pair<std::string, std::string>>& boxes) {
    std::string s;
    for (auto& b : boxes) s += Box(b.first.c_str(), b.second);
    return s;
}

std::string NewMeta(const std::string& items) {
    std::string hdlr;
    Put32(hdlr, 0);  // version and flags
    Put32(hdlr, 0);  // pre-defined
    hdlr += "mdirappl";
    hdlr += std::string(9, '\0');  // reserved, and an empty name
    std::string meta;
    Put32(meta, 0);
    return meta + Box("hdlr", hdlr) + Box("ilst", items);
}

/* The index's body with the tag items in its user data: added to the tag
   list there is, or in a new one. */
std::string WithTags(const std::string& moov, const std::string& items) {
    auto top = Boxes(moov);
    for (auto& t : top) {
        if (t.first != "udta") continue;
        auto udta = Boxes(t.second);
        for (auto& u : udta) {
            if (u.first != "meta" || u.second.size() < 4) continue;
            // A meta box starts with its version and flags.
            auto meta = Boxes(u.second.substr(4));
            for (auto& m : meta) {
                if (m.first != "ilst") continue;
                m.second += items;
                u.second = u.second.substr(0, 4) + Join(meta);
                t.second = Join(udta);
                return Join(top);
            }
            meta.emplace_back("ilst", items);
            u.second = u.second.substr(0, 4) + Join(meta);
            t.second = Join(udta);
            return Join(top);
        }
        udta.insert(udta.begin(), std::make_pair(std::string("meta"), NewMeta(items)));
        t.second = Join(udta);
        return Join(top);
    }
    return moov + Box("udta", Box("meta", NewMeta(items)));
}

}  // namespace

bool WriteM4aTags(const std::wstring& path, const TagValues& tags) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size = {};
    GetFileSizeEx(f, &size);
    // The top-level boxes, up to the index; it must be the last one, so the
    // tags go into it without moving anything the index points at.
    long long pos = 0, moov = -1, moov_size = 0;
    while (pos + 8 <= size.QuadPart) {
        BYTE head[16];
        DWORD got = 0;
        LARGE_INTEGER at;
        at.QuadPart = pos;
        SetFilePointerEx(f, at, nullptr, FILE_BEGIN);
        if (!ReadFile(f, head, 16, &got, nullptr) || got < 8) break;
        long long box = Get32(head);
        if (box == 1 && got >= 16) box = ((long long)Get32(head + 8) << 32) | Get32(head + 12);
        else if (box == 0) box = size.QuadPart - pos;
        if (box < 8) break;
        if (std::memcmp(head + 4, "moov", 4) == 0) {
            moov = pos;
            moov_size = box;
        }
        pos += box;
    }
    bool ok = moov >= 0 && moov + moov_size == size.QuadPart && moov_size < 64 * 1024 * 1024 && pos == size.QuadPart;
    if (ok) {
        std::string body((size_t)moov_size - 8, '\0');
        LARGE_INTEGER at;
        at.QuadPart = moov + 8;
        DWORD got = 0;
        SetFilePointerEx(f, at, nullptr, FILE_BEGIN);
        ok = ReadFile(f, &body[0], (DWORD)body.size(), &got, nullptr) && got == body.size();
        if (ok) {
            static const char* types[TAG_COUNT] = {"\xA9nam", "\xA9" "ART", "aART",     "\xA9" "alb",
                                                   "\xA9" "day", "\xA9gen",    "trkn", "\xA9" "cmt"};
            std::string items;
            for (int t = 0; t < TAG_COUNT; ++t) {
                const std::wstring& v = tags.value[t];
                if (v.empty()) continue;
                std::string data;
                if (t == TAG_TRACK) {
                    // Binary: the number and the total, two bytes each.
                    int n = _wtoi(v.c_str()), total = 0;
                    size_t slash = v.find(L'/');
                    if (slash != std::wstring::npos) total = _wtoi(v.c_str() + slash + 1);
                    if (n <= 0 || n > 65535) continue;
                    Put32(data, 0);  // type: binary
                    Put32(data, 0);  // locale
                    data += std::string(2, '\0');
                    data += (char)(n >> 8);
                    data += (char)(n & 0xFF);
                    data += (char)((total >> 8) & 0xFF);
                    data += (char)(total & 0xFF);
                    data += std::string(2, '\0');
                } else {
                    Put32(data, 1);  // type: UTF-8 text
                    Put32(data, 0);
                    data += Narrow(v, CP_UTF8);
                }
                items += Box(types[t], Box("data", data));
            }
            // Media Foundation writes user data with an empty tag list of its
            // own: the tags go into that list. Otherwise the user data is new.
            std::string new_body = WithTags(body, items);
            std::string head;
            Put32(head, (DWORD)(8 + new_body.size()));
            head += "moov";
            std::string whole = head + new_body;
            LARGE_INTEGER w;
            w.QuadPart = moov;
            DWORD wrote = 0;
            SetFilePointerEx(f, w, nullptr, FILE_BEGIN);
            ok = WriteFile(f, whole.data(), (DWORD)whole.size(), &wrote, nullptr) && wrote == whole.size();
        }
    }
    CloseHandle(f);
    return ok;
}
