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

/* Small text and path helpers used throughout the program. */
#pragma once

#include <windows.h>

#include <string>

std::wstring Widen(const std::string& text, UINT codepage = CP_UTF8);
std::string Narrow(const std::wstring& text, UINT codepage = CP_UTF8);

std::wstring Trim(const std::wstring& text);
std::wstring ToLower(const std::wstring& text);

/* Full path of the executable. */
std::wstring ExePath();
/* Folder of the executable, without the trailing slash. */
std::wstring ExeDir();
/* Current working folder, where the .ini and the recordings folder live. */
std::wstring CurrentDir();

std::wstring JoinPath(const std::wstring& a, const std::wstring& b);
std::wstring BaseName(const std::wstring& path);
/* Splits the extension like os.path.splitext: "a/b.mp3" -> "a/b", ".mp3". */
void SplitExt(const std::wstring& path, std::wstring* base, std::wstring* ext);

bool FileExists(const std::wstring& path);
bool DirExists(const std::wstring& path);
/* Creates the folder and any missing parents. Returns the Windows error code. */
DWORD MakeDirs(const std::wstring& path);

/* Windows error text, as Python shows it: "[WinError 2] ...". */
std::wstring WinErrorText(DWORD code);

/* Puts the current thread in the Windows audio scheduler (MMCSS), which keeps
   it from starving for CPU when the computer is busy or idle. */
HANDLE EnterAudioThread();
void LeaveAudioThread(HANDLE task);
