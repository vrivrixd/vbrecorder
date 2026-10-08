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
 * Interface foundation: native Windows controls, which screen readers read
 * best, and a box layout (like the wxWidgets sizers of the Python version) so
 * the window looks the same as before.
 */
#pragma once

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

namespace ui {

void Init(HINSTANCE instance);
HINSTANCE Instance();
HFONT Font();
/* Converts 96 DPI pixels to the screen scale. */
int S(int px);

HWND Create(HWND parent, const wchar_t* cls, const std::wstring& text, DWORD style, int id,
            DWORD ex_style = 0);
std::wstring GetText(HWND hwnd);
void SetText(HWND hwnd, const std::wstring& text);
bool IsShownStyle(HWND hwnd);
void Show(HWND hwnd, bool show);

/* Read-only combo box, like wx.ComboBox with CB_READONLY. */
void ComboSetItems(HWND combo, const std::vector<std::wstring>& items);
std::vector<std::wstring> ComboItems(HWND combo);
/* Text of the selected item, or empty if nothing is selected. */
std::wstring ComboValue(HWND combo);
/* Selects the item with this text; if there is none, nothing changes. */
void ComboSetValue(HWND combo, const std::wstring& value);

/* Tooltip that the screen reader announces along with the control. */
void SetToolTip(HWND owner_window, HWND control, const std::wstring& text);

int MsgBox(HWND parent, const std::wstring& text, const std::wstring& title, UINT flags);

/* ---- Box layout ---- */

enum {
    LEFT = 1,
    RIGHT = 2,
    TOP = 4,
    BOTTOM = 8,
    ALL = LEFT | RIGHT | TOP | BOTTOM,
    EXPAND = 16,
    CENTER_V = 32,  // centers across a horizontal box
    CENTER = 64,    // centers across the box
    ALIGN_RIGHT = 128,
};

class Item {
public:
    virtual ~Item() {}
    virtual SIZE Min() = 0;
    virtual bool Shown() = 0;
    virtual void Place(int x, int y, int w, int h) = 0;
};

class Box : public Item {
public:
    explicit Box(bool vertical) : vertical_(vertical) {}
    /* min: forced minimum size; 0 on an axis lets the control decide. */
    Box* Add(HWND hwnd, int proportion, int flags, int border, SIZE min = {0, 0});
    Box* Add(std::unique_ptr<Item> item, int proportion, int flags, int border);

    SIZE Min() override;
    bool Shown() override;
    void Place(int x, int y, int w, int h) override;

private:
    struct Entry {
        std::unique_ptr<Item> item;
        int proportion, flags, border;
    };
    bool vertical_;
    std::vector<Entry> entries_;
};

/* Two-column grid with the second column stretching (FlexGridSizer). */
class Grid : public Item {
public:
    Grid(int vgap, int hgap) : vgap_(vgap), hgap_(hgap) {}
    Grid* Add(HWND hwnd, int flags, SIZE min = {0, 0});

    SIZE Min() override;
    bool Shown() override { return true; }
    void Place(int x, int y, int w, int h) override;

private:
    struct Entry {
        std::unique_ptr<Item> item;
        int flags;
    };
    int vgap_, hgap_;
    std::vector<Entry> entries_;
};

std::unique_ptr<Item> Control(HWND hwnd, SIZE min = {0, 0});
SIZE BestSize(HWND hwnd);

/* ---- Dialogs ---- */

class Dialog {
public:
    virtual ~Dialog() {}
    /* Shows the dialog and waits for it to close. Returns IDOK or IDCANCEL. */
    int ShowModal(HWND owner);
    void EndModal(int code);
    HWND hwnd() const { return hwnd_; }

protected:
    explicit Dialog(const std::wstring& title) : title_(title) {}
    virtual void OnInit() = 0;
    virtual bool OnCommand(int id, int code, HWND control) { return false; }
    /* Keys, before the default dialog handling; true if they were used. */
    virtual bool OnCharHook(const MSG& msg) { return false; }
    virtual INT_PTR OnMessage(UINT msg, WPARAM wp, LPARAM lp, bool* handled) {
        *handled = false;
        return 0;
    }
    /* Sizes the dialog to its content and centers it over the owner. */
    void Fit();
    void Relayout();

    HWND Add(const wchar_t* cls, const std::wstring& text, DWORD style, int id = 0,
             DWORD ex_style = 0);

    std::unique_ptr<Item> root_;
    HWND hwnd_ = nullptr;
    HWND owner_ = nullptr;
    DWORD style_ = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;

private:
    static INT_PTR CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    std::wstring title_;
    bool done_ = false;
    int result_ = IDCANCEL;
};

/* Creates an empty dialog window, without a resource template. */
HWND CreateDialogWindow(HWND owner, DWORD style, DWORD ex_style, const std::wstring& title,
                        DLGPROC proc, LPARAM param);

/* Tells whether a key message belongs to the window, for the main loop. */
bool IsKeyFor(HWND window, const MSG& msg);

}  // namespace ui
