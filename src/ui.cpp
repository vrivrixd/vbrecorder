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

#include "ui.h"

#include <commctrl.h>

#include <algorithm>

namespace ui {

namespace {

HINSTANCE g_instance = nullptr;
HFONT g_font = nullptr;
int g_dpi = 96;

SIZE TextExtent(HWND hwnd, const std::wstring& text, UINT flags = 0, int width = 0) {
    HDC dc = GetDC(hwnd);
    HGDIOBJ old = SelectObject(dc, g_font);
    RECT r = {0, 0, width, 0};
    if (text.empty()) {
        TEXTMETRICW tm;
        GetTextMetricsW(dc, &tm);
        r.bottom = tm.tmHeight;
    } else {
        DrawTextW(dc, text.c_str(), (int)text.size(), &r, DT_CALCRECT | flags);
    }
    SelectObject(dc, old);
    ReleaseDC(hwnd, dc);
    return SIZE{r.right - r.left, r.bottom - r.top};
}

std::wstring ClassOf(HWND hwnd) {
    wchar_t cls[64] = {};
    GetClassNameW(hwnd, cls, 64);
    return cls;
}

class ControlItem : public Item {
public:
    ControlItem(HWND hwnd, SIZE min) : hwnd_(hwnd), min_(min) {}
    SIZE Min() override {
        SIZE best = (min_.cx > 0 && min_.cy > 0) ? min_ : BestSize(hwnd_);
        if (min_.cx > 0) best.cx = min_.cx;
        if (min_.cy > 0) best.cy = min_.cy;
        return best;
    }
    bool Shown() override { return IsShownStyle(hwnd_); }
    void Place(int x, int y, int w, int h) override {
        // The height of a combo box includes its drop-down list.
        if (ClassOf(hwnd_) == WC_COMBOBOXW) h += S(300);
        SetWindowPos(hwnd_, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    }

private:
    HWND hwnd_;
    SIZE min_;
};

}  // namespace

void Init(HINSTANCE instance) {
    g_instance = instance;
    HDC screen = GetDC(nullptr);
    g_dpi = GetDeviceCaps(screen, LOGPIXELSY);
    ReleaseDC(nullptr, screen);
    NONCLIENTMETRICSW ncm = {sizeof(ncm)};
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    g_font = CreateFontIndirectW(&ncm.lfMessageFont);
}

HINSTANCE Instance() { return g_instance; }
HFONT Font() { return g_font; }
int S(int px) { return MulDiv(px, g_dpi, 96); }

HWND Create(HWND parent, const wchar_t* cls, const std::wstring& text, DWORD style, int id,
            DWORD ex_style) {
    HWND hwnd = CreateWindowExW(ex_style, cls, text.c_str(), WS_CHILD | style, 0, 0, 10, 10,
                                parent, (HMENU)(INT_PTR)id, g_instance, nullptr);
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)g_font, FALSE);
    return hwnd;
}

std::wstring GetText(HWND hwnd) {
    int n = GetWindowTextLengthW(hwnd);
    std::wstring text(n + 1, L'\0');
    n = GetWindowTextW(hwnd, &text[0], n + 1);
    text.resize(n);
    return text;
}

void SetText(HWND hwnd, const std::wstring& text) {
    if (GetText(hwnd) != text) SetWindowTextW(hwnd, text.c_str());
}

bool IsShownStyle(HWND hwnd) { return (GetWindowLongW(hwnd, GWL_STYLE) & WS_VISIBLE) != 0; }

void Show(HWND hwnd, bool show) { ShowWindow(hwnd, show ? SW_SHOWNA : SW_HIDE); }

void ComboSetItems(HWND combo, const std::vector<std::wstring>& items) {
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    for (auto& item : items) SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)item.c_str());
}

std::vector<std::wstring> ComboItems(HWND combo) {
    std::vector<std::wstring> items;
    int n = (int)SendMessageW(combo, CB_GETCOUNT, 0, 0);
    for (int i = 0; i < n; ++i) {
        int len = (int)SendMessageW(combo, CB_GETLBTEXTLEN, i, 0);
        std::wstring text(len + 1, L'\0');
        SendMessageW(combo, CB_GETLBTEXT, i, (LPARAM)&text[0]);
        text.resize(len);
        items.push_back(text);
    }
    return items;
}

std::wstring ComboValue(HWND combo) {
    int sel = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (sel == CB_ERR) return std::wstring();
    return ComboItems(combo)[sel];
}

void ComboSetValue(HWND combo, const std::wstring& value) {
    std::vector<std::wstring> items = ComboItems(combo);
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i] == value) {
            SendMessageW(combo, CB_SETCURSEL, i, 0);
            return;
        }
    }
}

void SetToolTip(HWND owner_window, HWND control, const std::wstring& text) {
    HWND tip = (HWND)GetPropW(owner_window, L"vb.tooltip");
    if (!tip) {
        tip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                              WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT,
                              CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, owner_window, nullptr,
                              g_instance, nullptr);
        SendMessageW(tip, TTM_SETMAXTIPWIDTH, 0, S(400));
        SetPropW(owner_window, L"vb.tooltip", tip);
    }
    TOOLINFOW ti = {sizeof(ti)};
    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    ti.hwnd = owner_window;
    ti.uId = (UINT_PTR)control;
    ti.lpszText = const_cast<wchar_t*>(text.c_str());
    SendMessageW(tip, TTM_DELTOOLW, 0, (LPARAM)&ti);
    SendMessageW(tip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
}

int MsgBox(HWND parent, const std::wstring& text, const std::wstring& title, UINT flags) {
    // Without an owner, the box applies to the whole program and comes to the front.
    if (!parent) flags |= MB_TASKMODAL | MB_SETFOREGROUND;
    return ::MessageBoxW(parent, text.c_str(), title.c_str(), flags);
}

SIZE BestSize(HWND hwnd) {
    std::wstring cls = ClassOf(hwnd);
    std::wstring text = GetText(hwnd);
    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    if (cls == WC_BUTTONW) {
        LONG kind = style & BS_TYPEMASK;
        SIZE t = TextExtent(hwnd, text);
        if (kind == BS_CHECKBOX || kind == BS_AUTOCHECKBOX || kind == BS_RADIOBUTTON ||
            kind == BS_AUTORADIOBUTTON) {
            return SIZE{t.cx + S(20), std::max<LONG>(t.cy, S(13)) + S(2)};
        }
        return SIZE{std::max<LONG>(S(75), t.cx + S(16)), std::max<LONG>(S(23), t.cy + S(8))};
    }
    if (cls == WC_STATICW) return TextExtent(hwnd, text);
    if (cls == WC_COMBOBOXW) {
        RECT r;
        GetWindowRect(hwnd, &r);
        LONG widest = 0;
        for (auto& item : ComboItems(hwnd))
            widest = std::max(widest, TextExtent(hwnd, item, DT_NOPREFIX).cx);
        return SIZE{std::max<LONG>(S(80), widest + S(8) + GetSystemMetrics(SM_CXVSCROLL)),
                    r.bottom - r.top};
    }
    if (cls == WC_EDITW) {
        SIZE t = TextExtent(hwnd, L"");
        return SIZE{S(100), t.cy + S(8)};
    }
    if (cls == TRACKBAR_CLASSW) return SIZE{S(100), S(26)};
    if (cls == WC_LISTVIEWW) return SIZE{S(100), S(80)};
    if (cls == WC_LISTBOXW) return SIZE{S(200), S(150)};
    return SIZE{S(80), S(23)};
}

std::unique_ptr<Item> Control(HWND hwnd, SIZE min) {
    return std::unique_ptr<Item>(new ControlItem(hwnd, min));
}

/* ---- Box ---- */

Box* Box::Add(HWND hwnd, int proportion, int flags, int border, SIZE min) {
    return Add(Control(hwnd, min), proportion, flags, border);
}

Box* Box::Add(std::unique_ptr<Item> item, int proportion, int flags, int border) {
    entries_.push_back(Entry{std::move(item), proportion, flags, border});
    return this;
}

bool Box::Shown() {
    for (auto& e : entries_)
        if (e.item->Shown()) return true;
    return false;
}

SIZE Box::Min() {
    SIZE total = {0, 0};
    for (auto& e : entries_) {
        if (!e.item->Shown()) continue;
        SIZE m = e.item->Min();
        LONG bw = ((e.flags & LEFT) ? e.border : 0) + ((e.flags & RIGHT) ? e.border : 0);
        LONG bh = ((e.flags & TOP) ? e.border : 0) + ((e.flags & BOTTOM) ? e.border : 0);
        if (vertical_) {
            total.cx = std::max(total.cx, m.cx + bw);
            total.cy += m.cy + bh;
        } else {
            total.cx += m.cx + bw;
            total.cy = std::max(total.cy, m.cy + bh);
        }
    }
    return total;
}

void Box::Place(int x, int y, int w, int h) {
    SIZE min = Min();
    int extra = (vertical_ ? h - min.cy : w - min.cx);
    int props = 0;
    for (auto& e : entries_)
        if (e.item->Shown()) props += e.proportion;
    if (extra < 0) extra = 0;

    int pos = vertical_ ? y : x;
    for (auto& e : entries_) {
        if (!e.item->Shown()) continue;
        SIZE m = e.item->Min();
        int bl = (e.flags & LEFT) ? e.border : 0, br = (e.flags & RIGHT) ? e.border : 0;
        int bt = (e.flags & TOP) ? e.border : 0, bb = (e.flags & BOTTOM) ? e.border : 0;
        int grow = (props && e.proportion) ? extra * e.proportion / props : 0;
        if (vertical_) {
            int ih = m.cy + grow;
            int avail = w - bl - br;
            int iw = (e.flags & EXPAND) ? avail : std::min<int>(m.cx, avail);
            int ix = x + bl;
            if (!(e.flags & EXPAND)) {
                if (e.flags & CENTER) ix = x + bl + (avail - iw) / 2;
                else if (e.flags & ALIGN_RIGHT) ix = x + bl + avail - iw;
            }
            e.item->Place(ix, pos + bt, iw, ih);
            pos += bt + ih + bb;
        } else {
            int iw = m.cx + grow;
            int avail = h - bt - bb;
            int ih = (e.flags & EXPAND) ? avail : std::min<int>(m.cy, avail);
            int iy = y + bt;
            if (!(e.flags & EXPAND) && (e.flags & (CENTER | CENTER_V)))
                iy = y + bt + (avail - ih) / 2;
            e.item->Place(pos + bl, iy, iw, ih);
            pos += bl + iw + br;
        }
    }
}

/* ---- Grid ---- */

Grid* Grid::Add(HWND hwnd, int flags, SIZE min) {
    entries_.push_back(Entry{Control(hwnd, min), flags});
    return this;
}

SIZE Grid::Min() {
    LONG col[2] = {0, 0};
    LONG height = 0;
    for (size_t r = 0; r * 2 < entries_.size(); ++r) {
        LONG row = 0;
        for (int c = 0; c < 2 && r * 2 + c < entries_.size(); ++c) {
            SIZE m = entries_[r * 2 + c].item->Min();
            col[c] = std::max(col[c], m.cx);
            row = std::max(row, m.cy);
        }
        height += row + (r ? vgap_ : 0);
    }
    return SIZE{col[0] + hgap_ + col[1], height};
}

void Grid::Place(int x, int y, int w, int h) {
    LONG col0 = 0;
    for (size_t i = 0; i < entries_.size(); i += 2)
        col0 = std::max(col0, entries_[i].item->Min().cx);
    int widths[2] = {(int)col0, std::max(0, w - (int)col0 - hgap_)};
    int pos = y;
    for (size_t r = 0; r * 2 < entries_.size(); ++r) {
        LONG row = 0;
        for (int c = 0; c < 2 && r * 2 + c < entries_.size(); ++c)
            row = std::max(row, entries_[r * 2 + c].item->Min().cy);
        int cx = x;
        for (int c = 0; c < 2 && r * 2 + c < entries_.size(); ++c) {
            Entry& e = entries_[r * 2 + c];
            SIZE m = e.item->Min();
            if (e.flags & EXPAND) {
                e.item->Place(cx, pos, widths[c], row);
            } else {
                int iy = (e.flags & CENTER_V) ? pos + (row - m.cy) / 2 : pos;
                e.item->Place(cx, iy, std::min<int>(m.cx, widths[c]), m.cy);
            }
            cx += widths[c] + hgap_;
        }
        pos += row + vgap_;
    }
}

/* ---- Dialogs ---- */

HWND CreateDialogWindow(HWND owner, DWORD style, DWORD ex_style, const std::wstring& title,
                        DLGPROC proc, LPARAM param) {
    // Minimal template: no controls, no font; the controls come later.
    std::vector<WORD> tmpl(sizeof(DLGTEMPLATE) / sizeof(WORD) + 3 + title.size() + 1, 0);
    DLGTEMPLATE* t = (DLGTEMPLATE*)tmpl.data();
    t->style = style;
    t->dwExtendedStyle = ex_style;
    WORD* p = tmpl.data() + sizeof(DLGTEMPLATE) / sizeof(WORD);
    *p++ = 0;  // no menu
    *p++ = 0;  // default dialog class
    for (wchar_t c : title) *p++ = c;
    *p = 0;
    return CreateDialogIndirectParamW(g_instance, t, owner, proc, param);
}

bool IsKeyFor(HWND window, const MSG& msg) {
    return (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) &&
           (msg.hwnd == window || IsChild(window, msg.hwnd));
}

HWND Dialog::Add(const wchar_t* cls, const std::wstring& text, DWORD style, int id,
                 DWORD ex_style) {
    return Create(hwnd_, cls, text, WS_VISIBLE | style, id, ex_style);
}

INT_PTR CALLBACK Dialog::Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Dialog* self = (Dialog*)GetWindowLongPtrW(hwnd, DWLP_USER);
    if (msg == WM_INITDIALOG) {
        self = (Dialog*)lp;
        SetWindowLongPtrW(hwnd, DWLP_USER, (LONG_PTR)self);
        self->hwnd_ = hwnd;
        SendMessageW(hwnd, WM_SETFONT, (WPARAM)g_font, FALSE);
        self->OnInit();
        return TRUE;
    }
    if (!self) return FALSE;
    bool handled = false;
    INT_PTR r = self->OnMessage(msg, wp, lp, &handled);
    if (handled) return r;
    switch (msg) {
        case WM_COMMAND:
            if (self->OnCommand(LOWORD(wp), HIWORD(wp), (HWND)lp)) return TRUE;
            if (LOWORD(wp) == IDCANCEL) self->EndModal(IDCANCEL);
            return TRUE;
        case WM_CLOSE:
            self->EndModal(IDCANCEL);
            return TRUE;
        case WM_SIZE:
            self->Relayout();
            return TRUE;
        case WM_DESTROY: {
            HWND tip = (HWND)GetPropW(hwnd, L"vb.tooltip");
            if (tip) DestroyWindow(tip);
            RemovePropW(hwnd, L"vb.tooltip");
            return TRUE;
        }
    }
    return FALSE;
}

int Dialog::ShowModal(HWND owner) {
    owner_ = owner;
    done_ = false;
    result_ = IDCANCEL;
    HWND hwnd = CreateDialogWindow(owner, style_, 0, title_, Proc, (LPARAM)this);
    if (!hwnd) return IDCANCEL;
    bool owner_enabled = owner && IsWindowEnabled(owner);
    if (owner_enabled) EnableWindow(owner, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);

    MSG msg;
    bool quit = false;
    while (!done_) {
        BOOL got = GetMessageW(&msg, nullptr, 0, 0);
        if (got == 0 || got == -1) {
            quit = true;
            break;
        }
        if (IsKeyFor(hwnd, msg) && OnCharHook(msg)) continue;
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (owner_enabled) EnableWindow(owner, TRUE);
    if (owner) SetActiveWindow(owner);
    DestroyWindow(hwnd);
    hwnd_ = nullptr;
    if (quit) PostQuitMessage((int)msg.wParam);
    return result_;
}

void Dialog::EndModal(int code) {
    result_ = code;
    done_ = true;
}

void Dialog::Fit() {
    SIZE min = root_->Min();
    RECT r = {0, 0, min.cx, min.cy};
    AdjustWindowRectEx(&r, (DWORD)GetWindowLongW(hwnd_, GWL_STYLE), FALSE,
                       (DWORD)GetWindowLongW(hwnd_, GWL_EXSTYLE));
    int w = r.right - r.left, h = r.bottom - r.top;
    RECT area;
    if (owner_ && IsWindowVisible(owner_) && !IsIconic(owner_)) {
        GetWindowRect(owner_, &area);
    } else {
        HMONITOR mon = MonitorFromWindow(owner_ ? owner_ : hwnd_, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = {sizeof(mi)};
        GetMonitorInfoW(mon, &mi);
        area = mi.rcWork;
    }
    int x = area.left + ((area.right - area.left) - w) / 2;
    int y = area.top + ((area.bottom - area.top) - h) / 2;
    SetWindowPos(hwnd_, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    Relayout();
}

void Dialog::Relayout() {
    if (!root_ || !hwnd_) return;
    RECT rc;
    GetClientRect(hwnd_, &rc);
    root_->Place(0, 0, rc.right, rc.bottom);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

}  // namespace ui
