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

#include "startup.h"

#include <windows.h>

#include <string>

#include "util.h"

namespace {

const wchar_t* const RUN_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* const APPROVED_KEY =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
const wchar_t* const VALUE_NAME = L"VB Recorder";

std::wstring Command() { return L"\"" + ExePath() + L"\" " + STARTUP_FLAG; }

std::wstring ReadValue(const wchar_t* key, const wchar_t* name) {
    DWORD size = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, key, name, RRF_RT_REG_SZ, nullptr, nullptr, &size) !=
            ERROR_SUCCESS ||
        size < sizeof(wchar_t))
        return L"";
    std::wstring out(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, key, name, RRF_RT_REG_SZ, nullptr, &out[0], &size) !=
        ERROR_SUCCESS)
        return L"";
    out.resize(wcslen(out.c_str()));
    return out;
}

void WriteValue(const std::wstring& command) {
    RegSetKeyValueW(HKEY_CURRENT_USER, RUN_KEY, VALUE_NAME, REG_SZ, command.c_str(),
                    (DWORD)((command.size() + 1) * sizeof(wchar_t)));
}

}  // namespace

void SetStartWithWindows(bool on) {
    if (on) {
        WriteValue(Command());
        RegDeleteKeyValueW(HKEY_CURRENT_USER, APPROVED_KEY, VALUE_NAME);
    } else {
        RegDeleteKeyValueW(HKEY_CURRENT_USER, RUN_KEY, VALUE_NAME);
    }
}

void RefreshStartWithWindows() {
    std::wstring command = Command();
    if (ReadValue(RUN_KEY, VALUE_NAME) != command) WriteValue(command);
}
