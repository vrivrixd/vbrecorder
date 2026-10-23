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

#include "install.h"

#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>

#include "config.h"
#include "util.h"

namespace {

const wchar_t* const UNINSTALL_KEY =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" VB_APP_ID L"_is1";

std::wstring ReadValue(const wchar_t* name) {
    DWORD size = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, UNINSTALL_KEY, name, RRF_RT_REG_SZ, nullptr, nullptr,
                     &size) != ERROR_SUCCESS ||
        size < sizeof(wchar_t))
        return L"";
    std::wstring out(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, UNINSTALL_KEY, name, RRF_RT_REG_SZ, nullptr, &out[0],
                     &size) != ERROR_SUCCESS)
        return L"";
    out.resize(wcslen(out.c_str()));
    return out;
}

/* The same folder, however it is written: case, a trailing backslash. */
std::wstring FolderKey(std::wstring path) {
    while (!path.empty() && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
    return ToLower(path);
}

}  // namespace

bool IsInstalled() {
    static const bool installed = [] {
        std::wstring location = ReadValue(L"InstallLocation");
        return !location.empty() && FolderKey(location) == FolderKey(ExeDir());
    }();
    return installed;
}

void RefreshInstalledVersion() {
    if (!IsInstalled() || ReadValue(L"DisplayVersion") == VB_VERSION) return;
    std::wstring version = VB_VERSION;
    RegSetKeyValueW(HKEY_CURRENT_USER, UNINSTALL_KEY, L"DisplayVersion", REG_SZ, version.c_str(),
                    (DWORD)((version.size() + 1) * sizeof(wchar_t)));
}

std::wstring InstalledRecordingsDir() {
    std::wstring documents;
    wchar_t* path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &path))) documents = path;
    CoTaskMemFree(path);
    if (documents.empty()) documents = ExeDir();
    return JoinPath(documents, L"VB Recorder");
}
