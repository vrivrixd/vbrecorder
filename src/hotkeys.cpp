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

#include "hotkeys.h"

#include <commctrl.h>

#include <cwctype>
#include <vector>

#include "util.h"

namespace {

enum { ANY = -1, NORMAL = 0, EXTENDED = 1 };

struct KeyName {
    const wchar_t* name;
    UINT vk;
    int ext;  // the same virtual key comes from the main block (extended) or the numpad
};

const KeyName KEYS[] = {
    {L"BACK", VK_BACK, ANY},
    {L"TAB", VK_TAB, ANY},
    {L"RETURN", VK_RETURN, NORMAL},
    {L"NUMPAD_ENTER", VK_RETURN, EXTENDED},
    {L"ESCAPE", VK_ESCAPE, ANY},
    {L"SPACE", VK_SPACE, ANY},
    {L"DELETE", VK_DELETE, EXTENDED},
    {L"NUMPAD_DELETE", VK_DELETE, NORMAL},
    {L"INSERT", VK_INSERT, EXTENDED},
    {L"NUMPAD_INSERT", VK_INSERT, NORMAL},
    {L"HOME", VK_HOME, EXTENDED},
    {L"NUMPAD_HOME", VK_HOME, NORMAL},
    {L"END", VK_END, EXTENDED},
    {L"NUMPAD_END", VK_END, NORMAL},
    {L"PAGEUP", VK_PRIOR, EXTENDED},
    {L"NUMPAD_PAGEUP", VK_PRIOR, NORMAL},
    {L"PAGEDOWN", VK_NEXT, EXTENDED},
    {L"NUMPAD_PAGEDOWN", VK_NEXT, NORMAL},
    {L"LEFT", VK_LEFT, EXTENDED},
    {L"NUMPAD_LEFT", VK_LEFT, NORMAL},
    {L"UP", VK_UP, EXTENDED},
    {L"NUMPAD_UP", VK_UP, NORMAL},
    {L"RIGHT", VK_RIGHT, EXTENDED},
    {L"NUMPAD_RIGHT", VK_RIGHT, NORMAL},
    {L"DOWN", VK_DOWN, EXTENDED},
    {L"NUMPAD_DOWN", VK_DOWN, NORMAL},
    {L"CLEAR", VK_CLEAR, EXTENDED},
    {L"NUMPAD_BEGIN", VK_CLEAR, NORMAL},
    {L"NUMPAD0", VK_NUMPAD0, ANY},
    {L"NUMPAD1", VK_NUMPAD1, ANY},
    {L"NUMPAD2", VK_NUMPAD2, ANY},
    {L"NUMPAD3", VK_NUMPAD3, ANY},
    {L"NUMPAD4", VK_NUMPAD4, ANY},
    {L"NUMPAD5", VK_NUMPAD5, ANY},
    {L"NUMPAD6", VK_NUMPAD6, ANY},
    {L"NUMPAD7", VK_NUMPAD7, ANY},
    {L"NUMPAD8", VK_NUMPAD8, ANY},
    {L"NUMPAD9", VK_NUMPAD9, ANY},
    {L"NUMPAD_MULTIPLY", VK_MULTIPLY, ANY},
    {L"NUMPAD_ADD", VK_ADD, ANY},
    {L"NUMPAD_SEPARATOR", VK_SEPARATOR, ANY},
    {L"NUMPAD_SUBTRACT", VK_SUBTRACT, ANY},
    {L"NUMPAD_DECIMAL", VK_DECIMAL, ANY},
    {L"NUMPAD_DIVIDE", VK_DIVIDE, ANY},
    {L"F1", VK_F1, ANY},
    {L"F2", VK_F2, ANY},
    {L"F3", VK_F3, ANY},
    {L"F4", VK_F4, ANY},
    {L"F5", VK_F5, ANY},
    {L"F6", VK_F6, ANY},
    {L"F7", VK_F7, ANY},
    {L"F8", VK_F8, ANY},
    {L"F9", VK_F9, ANY},
    {L"F10", VK_F10, ANY},
    {L"F11", VK_F11, ANY},
    {L"F12", VK_F12, ANY},
    {L"F13", VK_F13, ANY},
    {L"F14", VK_F14, ANY},
    {L"F15", VK_F15, ANY},
    {L"F16", VK_F16, ANY},
    {L"F17", VK_F17, ANY},
    {L"F18", VK_F18, ANY},
    {L"F19", VK_F19, ANY},
    {L"F20", VK_F20, ANY},
    {L"F21", VK_F21, ANY},
    {L"F22", VK_F22, ANY},
    {L"F23", VK_F23, ANY},
    {L"F24", VK_F24, ANY},
    {L"NUMLOCK", VK_NUMLOCK, ANY},
    {L"SCROLL", VK_SCROLL, ANY},
    {L"PAUSE", VK_PAUSE, ANY},
    {L"CAPITAL", VK_CAPITAL, ANY},
    {L"SNAPSHOT", VK_SNAPSHOT, ANY},
    {L"SELECT", VK_SELECT, ANY},
    {L"PRINT", VK_PRINT, ANY},
    {L"EXECUTE", VK_EXECUTE, ANY},
    {L"HELP", VK_HELP, ANY},
    {L"CANCEL", VK_CANCEL, ANY},
    {L"WINDOWS_MENU", VK_APPS, ANY},
    {L"BROWSER_BACK", VK_BROWSER_BACK, ANY},
    {L"BROWSER_FORWARD", VK_BROWSER_FORWARD, ANY},
    {L"BROWSER_REFRESH", VK_BROWSER_REFRESH, ANY},
    {L"BROWSER_STOP", VK_BROWSER_STOP, ANY},
    {L"BROWSER_SEARCH", VK_BROWSER_SEARCH, ANY},
    {L"BROWSER_FAVORITES", VK_BROWSER_FAVORITES, ANY},
    {L"BROWSER_HOME", VK_BROWSER_HOME, ANY},
    {L"VOLUME_MUTE", VK_VOLUME_MUTE, ANY},
    {L"VOLUME_DOWN", VK_VOLUME_DOWN, ANY},
    {L"VOLUME_UP", VK_VOLUME_UP, ANY},
    {L"MEDIA_NEXT_TRACK", VK_MEDIA_NEXT_TRACK, ANY},
    {L"MEDIA_PREV_TRACK", VK_MEDIA_PREV_TRACK, ANY},
    {L"MEDIA_STOP", VK_MEDIA_STOP, ANY},
    {L"MEDIA_PLAY_PAUSE", VK_MEDIA_PLAY_PAUSE, ANY},
    {L"LAUNCH_MAIL", VK_LAUNCH_MAIL, ANY},
    {L"LAUNCH_APP1", VK_LAUNCH_APP1, ANY},
    {L"LAUNCH_APP2", VK_LAUNCH_APP2, ANY},
    // Only for reading the .ini: the capture field never writes these.
    {L"SHIFT", VK_SHIFT, ANY},
    {L"ALT", VK_MENU, ANY},
    {L"CONTROL", VK_CONTROL, ANY},
};

bool Extended(LPARAM lparam) { return (lparam & (1 << 24)) != 0; }

/* Name of the key, or its character if it is a printable ASCII character. */
std::wstring KeyLabel(UINT vk, bool ext) {
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return std::wstring(1, (wchar_t)vk);
    for (auto& k : KEYS) {
        if (k.vk != vk || k.vk == VK_SHIFT || k.vk == VK_MENU || k.vk == VK_CONTROL) continue;
        if (k.ext == ANY || (k.ext == EXTENDED) == ext) {
            if (vk == VK_SPACE) return L" ";
            return k.name;
        }
    }
    UINT ch = MapVirtualKeyW(vk, MAPVK_VK_TO_CHAR) & 0x7FFF;
    if (ch >= 32 && ch <= 126) return std::wstring(1, (wchar_t)towupper((wint_t)ch));
    return std::wstring();
}

bool Down(int vk) { return (GetKeyState(vk) & 0x8000) != 0; }

LRESULT CALLBACK HotkeyEditProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    switch (msg) {
        case WM_GETDLGCODE: {
            MSG* m = (MSG*)lp;
            if (m && (m->message == WM_KEYDOWN || m->message == WM_SYSKEYDOWN)) {
                if (m->wParam == VK_TAB || m->wParam == VK_ESCAPE || m->wParam == VK_RETURN)
                    return DefSubclassProc(hwnd, msg, wp, lp);
                return DLGC_WANTALLKEYS | DLGC_WANTMESSAGE | DefSubclassProc(hwnd, msg, wp, lp);
            }
            if (m && (m->message == WM_CHAR || m->message == WM_SYSCHAR))
                return DLGC_WANTMESSAGE | DLGC_WANTCHARS;
            return DefSubclassProc(hwnd, msg, wp, lp);
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            UINT vk = (UINT)wp;
            bool ext = Extended(lp);
            if (vk == VK_TAB || vk == VK_ESCAPE || vk == VK_RETURN ||
                ((vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT) && ext))
                return DefSubclassProc(hwnd, msg, wp, lp);
            if (vk == VK_BACK || (vk == VK_DELETE && ext)) {
                SetWindowTextW(hwnd, L"");
                return 0;
            }
            if (vk == VK_CONTROL || vk == VK_MENU || vk == VK_SHIFT || vk == VK_LWIN ||
                vk == VK_RWIN)
                return DefSubclassProc(hwnd, msg, wp, lp);
            std::wstring text = HotkeyFromKeyDown(wp, lp);
            if (!text.empty()) SetWindowTextW(hwnd, text.c_str());
            return 0;
        }
        case WM_CHAR:
        case WM_SYSCHAR:
            return 0;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

}  // namespace

std::wstring HotkeyFromKeyDown(WPARAM vk, LPARAM lparam) {
    std::wstring key = KeyLabel((UINT)vk, Extended(lparam));
    if (key.empty()) return std::wstring();
    std::vector<std::wstring> mods;
    if (Down(VK_CONTROL)) mods.push_back(L"Ctrl");
    if (Down(VK_MENU)) mods.push_back(L"Alt");
    if (Down(VK_SHIFT)) mods.push_back(L"Shift");
    if (Down(VK_LWIN) || Down(VK_RWIN)) mods.push_back(L"Win");
    std::wstring text;
    for (auto& m : mods) text += m + L"+";
    return text + key;
}

void ParseHotkey(const std::wstring& text, UINT* mods, UINT* vk) {
    *mods = 0;
    *vk = 0;
    if (text.empty()) return;
    std::wstring upper = text;
    CharUpperBuffW(&upper[0], (DWORD)upper.size());
    std::vector<std::wstring> parts;
    size_t pos = 0;
    while (pos <= upper.size()) {
        size_t end = upper.find(L'+', pos);
        if (end == std::wstring::npos) end = upper.size();
        parts.push_back(upper.substr(pos, end - pos));
        pos = end + 1;
    }
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        if (parts[i] == L"CTRL") *mods |= MOD_CONTROL;
        else if (parts[i] == L"ALT") *mods |= MOD_ALT;
        else if (parts[i] == L"SHIFT") *mods |= MOD_SHIFT;
        else if (parts[i] == L"WIN") *mods |= MOD_WIN;
    }
    std::wstring key = Trim(parts.back());
    if (key.size() == 1) {
        wchar_t c = key[0];
        if ((c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')) {
            *vk = c;
        } else {
            SHORT scan = VkKeyScanW(c);
            *vk = (scan != -1 && LOBYTE(scan) != 0xFF) ? LOBYTE(scan) : c;
        }
        return;
    }
    for (auto& k : KEYS) {
        if (key == k.name) {
            *vk = k.vk;
            return;
        }
    }
}

void MakeHotkeyInput(HWND edit) { SetWindowSubclass(edit, HotkeyEditProc, 1, 0); }
