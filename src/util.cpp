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

#include "util.h"

#include <vector>

std::wstring Widen(const std::string& text, UINT codepage) {
    if (text.empty()) return std::wstring();
    int n = MultiByteToWideChar(codepage, 0, text.data(), (int)text.size(), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(codepage, 0, text.data(), (int)text.size(), &out[0], n);
    return out;
}

std::string Narrow(const std::wstring& text, UINT codepage) {
    if (text.empty()) return std::string();
    int n = WideCharToMultiByte(codepage, 0, text.data(), (int)text.size(), nullptr, 0,
                                nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(codepage, 0, text.data(), (int)text.size(), &out[0], n, nullptr,
                        nullptr);
    return out;
}

std::wstring Trim(const std::wstring& text) {
    const wchar_t* spaces = L" \t\r\n\f\v";
    size_t start = text.find_first_not_of(spaces);
    if (start == std::wstring::npos) return std::wstring();
    size_t end = text.find_last_not_of(spaces);
    return text.substr(start, end - start + 1);
}

std::wstring ToLower(const std::wstring& text) {
    std::wstring out = text;
    if (!out.empty()) CharLowerBuffW(&out[0], (DWORD)out.size());
    return out;
}

std::wstring ExeDir() {
    std::vector<wchar_t> buf(MAX_PATH);
    for (;;) {
        DWORD n = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
        if (n < buf.size()) {
            std::wstring path(buf.data(), n);
            size_t slash = path.find_last_of(L"\\/");
            return slash == std::wstring::npos ? std::wstring(L".") : path.substr(0, slash);
        }
        buf.resize(buf.size() * 2);
    }
}

std::wstring CurrentDir() {
    DWORD n = GetCurrentDirectoryW(0, nullptr);
    std::wstring out(n, L'\0');
    n = GetCurrentDirectoryW(n, &out[0]);
    out.resize(n);
    return out;
}

std::wstring JoinPath(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    // An absolute path in b discards a, as in os.path.join.
    if (b.size() >= 2 && (b[1] == L':' || (b[0] == L'\\' && b[1] == L'\\'))) return b;
    wchar_t last = a.back();
    if (last == L'\\' || last == L'/' || last == L':') return a + b;
    return a + L"\\" + b;
}

std::wstring BaseName(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/:");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

void SplitExt(const std::wstring& path, std::wstring* base, std::wstring* ext) {
    size_t slash = path.find_last_of(L"\\/:");
    size_t name_start = slash == std::wstring::npos ? 0 : slash + 1;
    size_t dot = path.find_last_of(L'.');
    // Leading dots in the name (".bashrc") do not count as an extension.
    if (dot != std::wstring::npos && dot > name_start) {
        size_t first_real = path.find_first_not_of(L'.', name_start);
        if (first_real != std::wstring::npos && first_real < dot) {
            *base = path.substr(0, dot);
            *ext = path.substr(dot);
            return;
        }
    }
    *base = path;
    ext->clear();
}

bool FileExists(const std::wstring& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool DirExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

DWORD MakeDirs(const std::wstring& path) {
    if (path.empty() || DirExists(path)) return ERROR_SUCCESS;
    size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos && slash > 0) {
        std::wstring parent = path.substr(0, slash);
        if (!(parent.size() == 2 && parent[1] == L':')) {
            DWORD err = MakeDirs(parent);
            if (err != ERROR_SUCCESS) return err;
        }
    }
    if (!CreateDirectoryW(path.c_str(), nullptr)) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) return err;
    }
    return ERROR_SUCCESS;
}

std::wstring WinErrorText(DWORD code) {
    wchar_t* msg = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, (LPWSTR)&msg, 0, nullptr);
    std::wstring text = msg ? Trim(msg) : std::wstring();
    if (msg) LocalFree(msg);
    return L"[WinError " + std::to_wstring(code) + L"] " + text;
}

namespace {
typedef HANDLE(WINAPI* AvSetFn)(LPCWSTR, LPDWORD);
typedef BOOL(WINAPI* AvRevertFn)(HANDLE);
}  // namespace

HANDLE EnterAudioThread() {
    static HMODULE avrt = LoadLibraryW(L"avrt.dll");
    AvSetFn set = avrt ? (AvSetFn)(void*)GetProcAddress(avrt, "AvSetMmThreadCharacteristicsW")
                       : nullptr;
    DWORD index = 0;
    HANDLE task = set ? set(L"Pro Audio", &index) : nullptr;
    if (!task) SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    return task;
}

void LeaveAudioThread(HANDLE task) {
    if (!task) return;
    HMODULE avrt = GetModuleHandleW(L"avrt.dll");
    AvRevertFn revert =
        avrt ? (AvRevertFn)(void*)GetProcAddress(avrt, "AvRevertMmThreadCharacteristics")
             : nullptr;
    if (revert) revert(task);
}
