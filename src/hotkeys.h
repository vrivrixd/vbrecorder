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

/*
 * Hotkeys written as text ("Ctrl+Alt+R", "Ctrl+F5", "Shift+NUMPAD1"). Special
 * key names are the wxWidgets ones used by the Python version, so the .ini
 * keeps working from one version to the other.
 */
#pragma once

#include <windows.h>

#include <string>

/* Text of the pressed combination, or empty if the key has no name. */
std::wstring HotkeyFromKeyDown(WPARAM vk, LPARAM lparam);

/* Converts the text into modifiers (MOD_*) and a virtual key. Key 0: none. */
void ParseHotkey(const std::wstring& text, UINT* mods, UINT* vk);

/* Edit field that shows the pressed combination instead of typed text. */
void MakeHotkeyInput(HWND edit);
