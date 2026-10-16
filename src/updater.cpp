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

#include "updater.h"

#include <bcrypt.h>
#include <winhttp.h>

#include <cwchar>
#include <vector>

#include "config.h"
#include "util.h"

namespace {

const wchar_t* const LATEST_URL =
    L"https://api.github.com/repos/vrivrixd/vbrecorder/releases/latest";

#ifdef _WIN64
const char* const ASSET_NAME = "VBRecorder-x64.zip";
#else
const char* const ASSET_NAME = "VBRecorder-x86.zip";
#endif

const wchar_t* const UPDATED_FLAG = L"--updated";

/* The files set aside by the last installation, to be removed once this
   program has closed. */
std::vector<std::wstring> g_set_aside;

/* Reads a URL piece by piece. on_size gets the total (0 if unknown); on_data
   returns false to stop. */
bool HttpGet(const std::wstring& url, const wchar_t* extra_headers,
             const std::function<void(unsigned long long)>& on_size,
             const std::function<bool(const char*, DWORD)>& on_data) {
    URL_COMPONENTS parts = {sizeof(parts)};
    wchar_t host[256] = {}, path[2048] = {};
    parts.lpszHostName = host;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) return false;

    std::wstring agent = std::wstring(L"VB Recorder/") + VB_VERSION;
    HINTERNET session = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return false;
    WinHttpSetTimeouts(session, 10000, 10000, 15000, 15000);
    bool ok = false;
    HINTERNET connect = WinHttpConnect(session, host, parts.nPort, 0);
    HINTERNET request =
        connect ? WinHttpOpenRequest(connect, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES,
                                     parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                : nullptr;
    if (request &&
        WinHttpSendRequest(request, extra_headers ? extra_headers : WINHTTP_NO_ADDITIONAL_HEADERS,
                           extra_headers ? (DWORD)-1L : 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr)) {
        DWORD status = 0, size = sizeof(status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
        if (status == 200) {
            wchar_t length[32] = {};
            DWORD length_size = sizeof(length);
            unsigned long long total = 0;
            if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH,
                                    WINHTTP_HEADER_NAME_BY_INDEX, length, &length_size,
                                    WINHTTP_NO_HEADER_INDEX))
                total = wcstoull(length, nullptr, 10);
            if (on_size) on_size(total);
            std::vector<char> buf(16384);
            ok = true;
            for (;;) {
                DWORD read = 0;
                if (!WinHttpReadData(request, buf.data(), (DWORD)buf.size(), &read)) {
                    ok = false;
                    break;
                }
                if (read == 0) break;
                if (!on_data(buf.data(), read)) {
                    ok = false;
                    break;
                }
            }
        }
    }
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return ok;
}

/* The string value of a JSON key, from the given position on. */
bool JsonString(const std::string& json, const std::string& key, size_t from, std::string* value,
                size_t* at = nullptr) {
    size_t k = json.find("\"" + key + "\"", from);
    if (k == std::string::npos) return false;
    size_t colon = json.find(':', k);
    if (colon == std::string::npos) return false;
    size_t q = json.find_first_not_of(" \t\r\n", colon + 1);
    if (q == std::string::npos || json[q] != '"') return false;
    value->clear();
    for (size_t i = q + 1; i < json.size(); ++i) {
        if (json[i] == '\\' && i + 1 < json.size()) {
            value->push_back(json[++i] == '/' ? '/' : json[i]);
        } else if (json[i] == '"') {
            if (at) *at = i;
            return true;
        } else {
            value->push_back(json[i]);
        }
    }
    return false;
}

std::vector<int> VersionParts(const std::wstring& text) {
    std::vector<int> parts;
    size_t i = (!text.empty() && (text[0] == L'v' || text[0] == L'V')) ? 1 : 0;
    while (i < text.size()) {
        parts.push_back(_wtoi(text.c_str() + i));
        size_t dot = text.find(L'.', i);
        if (dot == std::wstring::npos) break;
        i = dot + 1;
    }
    while (!parts.empty() && parts.back() == 0) parts.pop_back();
    return parts;
}

bool IsNewer(const std::wstring& latest, const std::wstring& current) {
    return VersionParts(latest) > VersionParts(current);
}

std::string Sha256OfFile(const std::wstring& path) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0,
                           nullptr);
    if (f == INVALID_HANDLE_VALUE) return std::string();
    std::string hex;
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0 &&
        BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0) {
        std::vector<unsigned char> buf(65536);
        DWORD read = 0;
        bool ok = true;
        while (ReadFile(f, buf.data(), (DWORD)buf.size(), &read, nullptr) && read > 0)
            ok = ok && BCryptHashData(hash, buf.data(), read, 0) == 0;
        unsigned char digest[32];
        if (ok && BCryptFinishHash(hash, digest, 32, 0) == 0) {
            const char* digits = "0123456789abcdef";
            for (unsigned char c : digest) {
                hex.push_back(digits[c >> 4]);
                hex.push_back(digits[c & 15]);
            }
        }
    }
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(f);
    return hex;
}

void DeleteTree(const std::wstring& dir) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(JoinPath(dir, L"*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            std::wstring name = fd.cFileName;
            if (name == L"." || name == L"..") continue;
            std::wstring full = JoinPath(dir, name);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) DeleteTree(full);
            else DeleteFileW(full.c_str());
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir.c_str());
}

void ListFiles(const std::wstring& dir, const std::wstring& prefix, std::vector<std::wstring>* out) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(JoinPath(dir, L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::wstring name = fd.cFileName;
        if (name == L"." || name == L"..") continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            ListFiles(JoinPath(dir, name), prefix + name + L"\\", out);
        else
            out->push_back(prefix + name);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

void RemoveLeftovers(const std::wstring& dir, const wchar_t* pattern) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(JoinPath(dir, pattern).c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            DeleteFileW(JoinPath(dir, fd.cFileName).c_str());
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

}  // namespace

UpdateCheck CheckForUpdate(UpdateInfo* info) {
#ifdef VB_UPDATE_TEST
    // Test build: a version 1.5 whose "download" is the zip built next to it
    // (dist\VBRecorder.zip), so the whole update can be tried before
    // publishing.
    info->version = L"1.5";
    info->url = JoinPath(ExeDir(), L"..\\VBRecorder.zip");
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExW(info->url.c_str(), GetFileExInfoStandard, &fa))
        return UpdateCheck::Failed;
    info->size = ((unsigned long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
    return UpdateCheck::Available;
#endif
    std::string json;
    if (!HttpGet(LATEST_URL, L"Accept: application/vnd.github+json\r\n", nullptr,
                 [&](const char* data, DWORD n) {
                     json.append(data, n);
                     return json.size() < (4u << 20);
                 }))
        return UpdateCheck::Failed;
    std::string tag;
    if (!JsonString(json, "tag_name", 0, &tag)) return UpdateCheck::Failed;
    std::wstring version = Widen(tag);
    if (!version.empty() && (version[0] == L'v' || version[0] == L'V')) version.erase(0, 1);
    if (!IsNewer(version, VB_VERSION)) return UpdateCheck::UpToDate;

    // The asset of this architecture: its fields come after its name.
    size_t at = json.find(std::string("\"name\":\"") + ASSET_NAME + "\"");
    if (at == std::string::npos) at = json.find(std::string("\"name\": \"") + ASSET_NAME + "\"");
    if (at == std::string::npos) return UpdateCheck::Failed;
    std::string url, digest;
    if (!JsonString(json, "browser_download_url", at, &url)) return UpdateCheck::Failed;
    if (JsonString(json, "digest", at, &digest) && digest.compare(0, 7, "sha256:") == 0)
        info->sha256 = digest.substr(7);
    size_t s = json.find("\"size\"", at);
    if (s != std::string::npos) info->size = strtoull(json.c_str() + json.find(':', s) + 1, nullptr, 10);
    info->version = version;
    info->url = Widen(url);
    return UpdateCheck::Available;
}

bool DownloadUpdate(const UpdateInfo& info, const std::wstring& file,
                    const std::atomic<bool>& cancel,
                    const std::function<void(unsigned long long, unsigned long long)>& progress) {
    HANDLE f = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    unsigned long long total = info.size, done = 0;
#ifdef VB_UPDATE_TEST
    // Test build: the local zip, slowly, to see the download dialog.
    if (info.url.compare(0, 8, L"https://") != 0) {
        HANDLE src = CreateFileW(info.url.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                 OPEN_EXISTING, 0, nullptr);
        bool copied = src != INVALID_HANDLE_VALUE;
        char buf[4096];
        DWORD read = 0, written = 0;
        while (copied && !cancel && ReadFile(src, buf, sizeof(buf), &read, nullptr) && read) {
            Sleep(60);
            copied = WriteFile(f, buf, read, &written, nullptr) && written == read;
            done += read;
            if (progress) progress(done, total);
        }
        if (src != INVALID_HANDLE_VALUE) CloseHandle(src);
        CloseHandle(f);
        if (!copied || cancel) DeleteFileW(file.c_str());
        return copied && !cancel;
    }
#endif
    bool ok = HttpGet(
        info.url, nullptr, [&](unsigned long long size) { if (size) total = size; },
        [&](const char* data, DWORD n) {
            if (cancel) return false;
#ifdef VB_UPDATE_TEST
            // Test build: a slow connection, to see the download dialog.
            for (DWORD i = 0; i < n && !cancel; i += 2048) Sleep(30);
#endif
            DWORD written = 0;
            if (!WriteFile(f, data, n, &written, nullptr) || written != n) return false;
            done += n;
            if (progress) progress(done, total);
            return true;
        });
    CloseHandle(f);
    if (!ok || cancel || (total && done != total)) {
        DeleteFileW(file.c_str());
        return false;
    }
    return true;
}

bool InstallUpdate(const std::wstring& zip, const UpdateInfo& info, std::wstring* error) {
    // The file must be exactly the one GitHub published.
    if (!info.sha256.empty() && Sha256OfFile(zip) != info.sha256) {
        *error = L"SHA-256";
        return false;
    }
    const std::wstring dir = ExeDir();
    const std::wstring tmp = JoinPath(dir, L"update.tmp");
    DeleteTree(tmp);
    DWORD err = MakeDirs(tmp);
    if (err != ERROR_SUCCESS) {
        *error = WinErrorText(err);
        return false;
    }

    // tar, which comes with Windows 10, also extracts zip files.
    wchar_t system[MAX_PATH];
    GetSystemDirectoryW(system, MAX_PATH);
    std::wstring cmd = L"\"" + JoinPath(system, L"tar.exe") + L"\" -xf \"" + zip + L"\" -C \"" +
                       tmp + L"\"";
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    DWORD code = 1;
    if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                       nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 60000);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    std::vector<std::wstring> files;
    ListFiles(tmp, L"", &files);
    bool has_exe = false;
    for (auto& f : files) has_exe = has_exe || ToLower(f) == L"vbrecorder.exe";
    if (code != 0 || !has_exe) {
        DeleteTree(tmp);
        *error = L"zip";
        return false;
    }

    // The current files are set aside, and the new ones take their place. The
    // program keeps its own file name, even if it was renamed.
    std::vector<std::pair<std::wstring, bool>> placed;  // (target, had an old one)
    for (auto& rel : files) {
        std::wstring target = ToLower(rel) == L"vbrecorder.exe" ? ExePath() : JoinPath(dir, rel);
        size_t slash = target.find_last_of(L'\\');
        if (slash != std::wstring::npos) MakeDirs(target.substr(0, slash));
        bool had = FileExists(target);
        bool ok = !had || MoveFileExW(target.c_str(), (target + L".old").c_str(),
                                      MOVEFILE_REPLACE_EXISTING);
        ok = ok && MoveFileExW(JoinPath(tmp, rel).c_str(), target.c_str(),
                               MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
        if (!ok) {
            *error = WinErrorText(GetLastError());
            // Everything goes back as it was.
            if (had && !FileExists(target))
                MoveFileExW((target + L".old").c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
            for (auto& p : placed) {
                DeleteFileW(p.first.c_str());
                if (p.second)
                    MoveFileExW((p.first + L".old").c_str(), p.first.c_str(),
                                MOVEFILE_REPLACE_EXISTING);
            }
            DeleteTree(tmp);
            return false;
        }
        placed.emplace_back(target, had);
    }
    g_set_aside.clear();
    for (auto& p : placed)
        if (p.second) g_set_aside.push_back(p.first + L".old");
    DeleteTree(tmp);
    DeleteFileW(zip.c_str());
    return true;
}

bool StartUpdatedProgram() {
    // The files set aside are removed as soon as this program closes, whatever
    // the version that starts now: a hidden command tries for a few seconds.
    if (!g_set_aside.empty()) {
        wchar_t system[MAX_PATH];
        GetSystemDirectoryW(system, MAX_PATH);
        std::wstring del = L"del /f /q";
        for (auto& f : g_set_aside) del += L" \"" + f + L"\"";
        std::wstring cleanup = L"\"" + JoinPath(system, L"cmd.exe") +
                               L"\" /d /c for /l %i in (1,1,20) do (" + del +
                               L" >nul 2>&1 & ping -n 2 127.0.0.1 >nul)";
        STARTUPINFOW csi = {sizeof(csi)};
        PROCESS_INFORMATION cpi = {};
        if (CreateProcessW(nullptr, &cleanup[0], nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr, &csi, &cpi)) {
            CloseHandle(cpi.hThread);
            CloseHandle(cpi.hProcess);
        }
    }
    std::wstring cmd = L"\"" + ExePath() + L"\" " + UPDATED_FLAG + L" " +
                       std::to_wstring(GetCurrentProcessId());
    std::wstring cwd = CurrentDir();
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, cwd.c_str(), &si,
                        &pi))
        return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

void FinishUpdate(const wchar_t* command_line) {
    const wchar_t* flag = command_line ? wcsstr(command_line, UPDATED_FLAG) : nullptr;
    if (flag) {
        DWORD pid = (DWORD)wcstoul(flag + wcslen(UPDATED_FLAG), nullptr, 10);
        HANDLE old = pid ? OpenProcess(SYNCHRONIZE, FALSE, pid) : nullptr;
        if (old) {
            WaitForSingleObject(old, 20000);
            CloseHandle(old);
        }
    }
    // Only the program's own files, as the update set them aside.
    const std::wstring dir = ExeDir();
    RemoveLeftovers(dir, L"*.exe.old");
    RemoveLeftovers(dir, L"LICENSE.old");
    RemoveLeftovers(JoinPath(dir, L"lib"), L"*.dll.old");
}
