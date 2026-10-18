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

#include "process_audio.h"

#include <audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <tlhelp32.h>

#include <algorithm>
#include <map>
#include <set>

#include "util.h"

namespace {

typedef HRESULT(WINAPI* ActivateAsyncFn)(LPCWSTR, REFIID, PROPVARIANT*,
                                          IActivateAudioInterfaceCompletionHandler*,
                                          IActivateAudioInterfaceAsyncOperation**);

/* Structures from audioclientactivationparams.h, which not every SDK has. */
struct LoopbackParams {
    DWORD TargetProcessId;
    DWORD ProcessLoopbackMode;  // 0: the process and its children
};
struct ActivationParams {
    DWORD ActivationType;  // 1: process loopback
    LoopbackParams Params;
};

const IID IID_AgileObject = {0x94ea2b94, 0xe9cc, 0x49e0, {0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90}};

ActivateAsyncFn ActivateAsync() {
    static ActivateAsyncFn fn = [] {
        HMODULE dll = LoadLibraryW(L"mmdevapi.dll");
        return dll ? (ActivateAsyncFn)(void*)GetProcAddress(dll, "ActivateAudioInterfaceAsync")
                   : nullptr;
    }();
    return fn;
}

DWORD WindowsBuild() {
    typedef LONG(WINAPI * RtlGetVersionFn)(OSVERSIONINFOW*);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    RtlGetVersionFn get =
        ntdll ? (RtlGetVersionFn)(void*)GetProcAddress(ntdll, "RtlGetVersion") : nullptr;
    OSVERSIONINFOW info = {sizeof(info)};
    if (!get || get(&info) != 0) return 0;
    return info.dwMajorVersion > 10 ? 99999 : (info.dwMajorVersion == 10 ? info.dwBuildNumber : 0);
}

template <class T>
void SafeRelease(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

/* Receives the result of the asynchronous activation of the virtual loopback device. */
class ActivationHandler final : public IActivateAudioInterfaceCompletionHandler {
public:
    ActivationHandler() { done_ = CreateEventW(nullptr, TRUE, FALSE, nullptr); }
    ~ActivationHandler() {
        SafeRelease(client_);
        CloseHandle(done_);
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) ||
            IsEqualIID(riid, __uuidof(IActivateAudioInterfaceCompletionHandler)) ||
            IsEqualIID(riid, IID_AgileObject)) {
            *ppv = static_cast<IActivateAudioInterfaceCompletionHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs_); }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG n = InterlockedDecrement(&refs_);
        if (n == 0) delete this;
        return n;
    }

    STDMETHODIMP ActivateCompleted(IActivateAudioInterfaceAsyncOperation* op) override {
        HRESULT hr = E_FAIL;
        IUnknown* unk = nullptr;
        if (SUCCEEDED(op->GetActivateResult(&hr, &unk)) && SUCCEEDED(hr) && unk) {
            unk->QueryInterface(__uuidof(IAudioClient), (void**)&client_);
        }
        SafeRelease(unk);
        SetEvent(done_);
        return S_OK;
    }

    bool Wait(DWORD ms) { return WaitForSingleObject(done_, ms) == WAIT_OBJECT_0; }
    IAudioClient* TakeClient() {
        IAudioClient* c = client_;
        client_ = nullptr;
        return c;
    }

private:
    LONG refs_ = 1;
    HANDLE done_;
    IAudioClient* client_ = nullptr;
};

}  // namespace

bool ProcessLoopbackAvailable() {
    static bool available = ActivateAsync() != nullptr && WindowsBuild() >= 19041;
    return available;
}

std::wstring GetProcessName(DWORD pid) {
    // Also covers the rare case of Windows reusing a PID saved in the .ini that
    // happens to be ours: the source is dropped when the settings load.
    if (pid == GetCurrentProcessId()) return std::wstring();
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return std::wstring();
    wchar_t buf[MAX_PATH];
    DWORD size = MAX_PATH;
    std::wstring name;
    if (QueryFullProcessImageNameW(h, 0, buf, &size)) name = BaseName(std::wstring(buf, size));
    CloseHandle(h);
    return name;
}

std::vector<std::pair<DWORD, std::wstring>> GetAllProcesses() {
    std::vector<std::pair<DWORD, std::wstring>> result;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return result;
    DWORD self = GetCurrentProcessId(), session = 0;
    ProcessIdToSessionId(self, &session);
    PROCESSENTRY32W pe = {sizeof(pe)};
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        DWORD s = 0;
        if (pe.th32ProcessID == self || !ProcessIdToSessionId(pe.th32ProcessID, &s) ||
            s != session)
            continue;
        result.emplace_back(pe.th32ProcessID, pe.szExeFile);
    }
    CloseHandle(snap);
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        int c = _wcsicmp(a.second.c_str(), b.second.c_str());
        return c != 0 ? c < 0 : a.first < b.first;
    });
    return result;
}

std::vector<DWORD> FindMainProcesses(const std::wstring& exe_name) {
    std::vector<DWORD> result;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return result;
    std::wstring target = ToLower(exe_name);
    std::map<DWORD, std::pair<std::wstring, DWORD>> procs;  // pid -> (name, parent)
    PROCESSENTRY32W pe = {sizeof(pe)};
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
        procs[pe.th32ProcessID] = {ToLower(pe.szExeFile), pe.th32ParentProcessID};
    CloseHandle(snap);
    DWORD self = GetCurrentProcessId();
    for (auto& p : procs) {
        if (p.first == self || p.second.first != target) continue;
        auto parent = procs.find(p.second.second);
        if (parent != procs.end() && parent->second.first == target) continue;
        result.push_back(p.first);
    }
    return result;
}

std::vector<std::pair<DWORD, std::wstring>> GetAudioSessions() {
    std::vector<std::pair<DWORD, std::wstring>> result;
    HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioSessionManager2* manager = nullptr;
    IAudioSessionEnumerator* sessions = nullptr;
    int count = 0;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                   __uuidof(IMMDeviceEnumerator), (void**)&enumerator)) &&
        SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device)) &&
        SUCCEEDED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                                   (void**)&manager)) &&
        SUCCEEDED(manager->GetSessionEnumerator(&sessions)) &&
        SUCCEEDED(sessions->GetCount(&count))) {
        // The recorder itself must never show up as an audio source. The filter
        // uses the PID, not the executable name, so it works under any name.
        std::set<DWORD> seen = {0, GetCurrentProcessId()};
        for (int i = 0; i < count; ++i) {
            IAudioSessionControl* ctrl = nullptr;
            IAudioSessionControl2* ctrl2 = nullptr;
            if (SUCCEEDED(sessions->GetSession(i, &ctrl)) && ctrl &&
                SUCCEEDED(ctrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&ctrl2))) {
                DWORD pid = 0;
                if (ctrl2->GetProcessId(&pid) == S_OK && !seen.count(pid)) {
                    seen.insert(pid);
                    std::wstring name = GetProcessName(pid);
                    if (!name.empty()) result.emplace_back(pid, name);
                }
            }
            SafeRelease(ctrl2);
            SafeRelease(ctrl);
        }
    }
    SafeRelease(sessions);
    SafeRelease(manager);
    SafeRelease(device);
    SafeRelease(enumerator);
    if (SUCCEEDED(init)) CoUninitialize();
    return result;
}

ProcessLoopbackCapture::ProcessLoopbackCapture(DWORD pid, int buffer_ms)
    : pid_(pid), buffer_ms_(buffer_ms) {
    ready_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    stop_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
}

ProcessLoopbackCapture::~ProcessLoopbackCapture() {
    Stop();
    CloseHandle(ready_);
    CloseHandle(stop_);
}

bool ProcessLoopbackCapture::Start() {
    if (!ProcessLoopbackAvailable() || thread_) return false;
    thread_ = CreateThread(nullptr, 0, ThreadProc, this, 0, nullptr);
    return thread_ != nullptr;
}

bool ProcessLoopbackCapture::WaitReady(DWORD timeout_ms) {
    return WaitForSingleObject(ready_, timeout_ms) == WAIT_OBJECT_0;
}

void ProcessLoopbackCapture::Stop() {
    if (!thread_) return;
    SetEvent(stop_);
    // The thread exits within a few seconds, even in the middle of activation.
    WaitForSingleObject(thread_, INFINITE);
    CloseHandle(thread_);
    thread_ = nullptr;
}

DWORD WINAPI ProcessLoopbackCapture::ThreadProc(void* self) {
    static_cast<ProcessLoopbackCapture*>(self)->Run();
    return 0;
}

void ProcessLoopbackCapture::Run() {
    HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    HANDLE task = EnterAudioThread();
    Capture();
    SetEvent(ready_);
    LeaveAudioThread(task);
    if (SUCCEEDED(init)) CoUninitialize();
}

IAudioClient* ProcessLoopbackCapture::Activate() {
    ActivationParams params = {};
    params.ActivationType = 1;
    params.Params.TargetProcessId = pid_;
    params.Params.ProcessLoopbackMode = 0;

    PROPVARIANT prop = {};
    prop.vt = VT_BLOB;
    prop.blob.cbSize = sizeof(params);
    prop.blob.pBlobData = (BYTE*)&params;

    ActivationHandler* handler = new ActivationHandler();
    IActivateAudioInterfaceAsyncOperation* op = nullptr;
    IAudioClient* client = nullptr;
    HRESULT hr = ActivateAsync()(L"VAD\\Process_Loopback", __uuidof(IAudioClient), &prop,
                                 handler, &op);
    if (SUCCEEDED(hr) && handler->Wait(5000)) client = handler->TakeClient();
    SafeRelease(op);
    handler->Release();
    return client;
}

void ProcessLoopbackCapture::Capture() {
    // The virtual loopback device has no format of its own: it delivers audio
    // in the requested format. 32-bit float, 48 kHz, stereo.
    WAVEFORMATEX fmt = {};
    fmt.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    fmt.nChannels = CHANNELS;
    fmt.nSamplesPerSec = RATE;
    fmt.wBitsPerSample = 32;
    fmt.nBlockAlign = fmt.nChannels * 4;
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

    // The Windows buffer holds the audio until this thread fetches it. It
    // follows the buffer chosen in the settings and never goes below 200 ms:
    // losing a piece here means losing audio for good.
    int ms = buffer_ms_ < 200 ? 200 : buffer_ms_;
    REFERENCE_TIME duration = (REFERENCE_TIME)ms * 10000;

    // First with event notification; if Windows refuses, polling every
    // 10 ms.
    IAudioClient* client = nullptr;
    HANDLE event = nullptr;
    for (int attempt = 0; attempt < 2 && !client; ++attempt) {
        if (WaitForSingleObject(stop_, 0) == WAIT_OBJECT_0) return;
        client = Activate();
        if (!client) return;
        bool by_event = attempt == 0;
        DWORD flags = AUDCLNT_STREAMFLAGS_LOOPBACK |
                      (by_event ? AUDCLNT_STREAMFLAGS_EVENTCALLBACK : 0);
        HRESULT hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, duration, 0, &fmt,
                                        nullptr);
        if (SUCCEEDED(hr) && by_event) {
            event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            hr = client->SetEventHandle(event);
            if (FAILED(hr)) {
                CloseHandle(event);
                event = nullptr;
            }
        }
        if (FAILED(hr)) SafeRelease(client);
    }
    if (!client) return;

    IAudioCaptureClient* capture = nullptr;
    if (FAILED(client->GetService(__uuidof(IAudioCaptureClient), (void**)&capture)) ||
        FAILED(client->Start())) {
        SafeRelease(capture);
        SafeRelease(client);
        if (event) CloseHandle(event);
        return;
    }

    sample_rate_ = (int)fmt.nSamplesPerSec;
    channels_ = fmt.nChannels;
    SetEvent(ready_);

    HANDLE waits[2] = {stop_, event};
    bool running = true;
    while (running) {
        DWORD w = WaitForMultipleObjects(event ? 2 : 1, waits, FALSE, event ? 200 : 10);
        if (w == WAIT_OBJECT_0) break;
        UINT32 packet = 0;
        if (FAILED(capture->GetNextPacketSize(&packet))) break;
        while (packet > 0) {
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) {
                running = false;
                break;
            }
            CaptureSink* sink = sink_.load();
            if (sink && frames) {
                // With the silent flag, the buffer contents are meaningless and
                // must be treated as silence.
                sink->OnAudio((const float*)data, frames * fmt.nChannels,
                              (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0);
            }
            capture->ReleaseBuffer(frames);
            if (FAILED(capture->GetNextPacketSize(&packet))) {
                running = false;
                break;
            }
        }
    }

    client->Stop();
    SafeRelease(capture);
    SafeRelease(client);
    if (event) CloseHandle(event);
}
