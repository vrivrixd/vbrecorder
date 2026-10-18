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

#include "fx_dialogs.h"

#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cwchar>

#include "i18n.h"
#include "util.h"

using ui::S;

namespace {

const DWORD BUTTON = WS_TABSTOP | BS_PUSHBUTTON;
const DWORD CHECK = WS_TABSTOP | BS_AUTOCHECKBOX;
const DWORD COMBO = WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST;

const int SLIDER_MAX = 100;
const int SLIDER_PAGE = 5;

enum {
    IDC_FX_PRESET = 501,
    IDC_FX_NOISE_ON,
    IDC_FX_TRACK = 600,  // + slider index
};

std::unique_ptr<ui::Box> HBox() { return std::unique_ptr<ui::Box>(new ui::Box(false)); }
std::unique_ptr<ui::Box> VBox() { return std::unique_ptr<ui::Box>(new ui::Box(true)); }

int ClampPos(int pos) { return std::max(0, std::min(SLIDER_MAX, pos)); }

/* Gain: 50 is 0 dB, each step 0.3 dB, from -15 dB (0) to +15 dB (100). */
const float GAIN_STEP = (EQ_MAX_GAIN - EQ_MIN_GAIN) / SLIDER_MAX;
float GainOf(int pos) { return EQ_MIN_GAIN + pos * GAIN_STEP; }
int GainPos(float db) { return ClampPos((int)std::lround((db - EQ_MIN_GAIN) / GAIN_STEP)); }
std::wstring GainText(int pos) {
    wchar_t buf[32];
    float db = GainOf(pos);
    swprintf(buf, 32, L"%+.1f dB", std::fabs(db) < 0.05f ? 0.0f : db);
    return buf;
}

/* A band's name: "31 Hz", "1 kHz", "16 kHz". */
std::wstring FreqName(float hz) {
    wchar_t buf[32];
    if (hz >= 1000) swprintf(buf, 32, L"%g kHz", hz / 1000);
    else swprintf(buf, 32, L"%d Hz", (int)hz);
    return buf;
}

/* The "Custom" entry of a preset list is the last one. */
void SelectCustom(HWND combo) {
    int n = (int)SendMessageW(combo, CB_GETCOUNT, 0, 0);
    if (n > 0 && SendMessageW(combo, CB_GETCURSEL, 0, 0) != n - 1)
        SendMessageW(combo, CB_SETCURSEL, n - 1, 0);
}

}  // namespace

/* ---- Sliders ---- */

int SliderDialog::AddSlider(ui::Box* row, const std::wstring& label, int pos, ValueText text) {
    int index = (int)sliders_.size();
    HWND lbl = Add(WC_STATICW, label, 0);
    Slider s;
    s.text = text;
    s.track = Add(TRACKBAR_CLASSW, L"", WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, IDC_FX_TRACK + index);
    SendMessageW(s.track, TBM_SETRANGE, TRUE, MAKELPARAM(0, SLIDER_MAX));
    SendMessageW(s.track, TBM_SETPAGESIZE, 0, SLIDER_PAGE);
    SetWindowSubclass(s.track, TrackProc, 2, (DWORD_PTR)this);
    s.value = Add(WC_STATICW, L"", SS_NOPREFIX);
    sliders_.push_back(s);
    row->Add(lbl, 0, ui::CENTER_V | ui::RIGHT, S(5), SIZE{S(170), 0});
    row->Add(s.track, 1, ui::EXPAND | ui::RIGHT, S(5), SIZE{S(200), 0});
    row->Add(s.value, 0, ui::CENTER_V, 0, SIZE{S(70), 0});
    SetSliderPos(index, pos);
    return index;
}

int SliderDialog::SliderPos(int i) const {
    return (int)SendMessageW(sliders_[i].track, TBM_GETPOS, 0, 0);
}

void SliderDialog::SetSliderPos(int i, int pos) {
    SendMessageW(sliders_[i].track, TBM_SETPOS, TRUE, ClampPos(pos));
    ShowValue(i);
}

void SliderDialog::ShowValue(int i) {
    ui::SetText(sliders_[i].value, sliders_[i].text(SliderPos(i)));
}

int SliderDialog::FindSlider(HWND track) const {
    for (size_t i = 0; i < sliders_.size(); ++i)
        if (sliders_[i].track == track) return (int)i;
    return -1;
}

/* The keys work as on the volume slider of the source dialog: up and right
   raise the value. */
LRESULT CALLBACK SliderDialog::TrackProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR,
                                         DWORD_PTR ref) {
    if (msg == WM_KEYDOWN) {
        SliderDialog* self = (SliderDialog*)ref;
        int i = self->FindSlider(hwnd);
        if (i >= 0) {
            int pos = (int)SendMessageW(hwnd, TBM_GETPOS, 0, 0), next = pos;
            bool moved = true;
            switch (wp) {
                case VK_HOME: next = 0; break;
                case VK_END: next = SLIDER_MAX; break;
                case VK_UP:
                case VK_RIGHT: next = pos + 1; break;
                case VK_DOWN:
                case VK_LEFT: next = pos - 1; break;
                // To the next multiple of the page, as the volume slider does.
                case VK_PRIOR: next = (pos / SLIDER_PAGE + 1) * SLIDER_PAGE; break;
                case VK_NEXT: next = (int)std::floor((pos - 1) / (double)SLIDER_PAGE) * SLIDER_PAGE; break;
                default: moved = false;
            }
            if (moved) {
                SendMessageW(hwnd, TBM_SETPOS, TRUE, ClampPos(next));
                self->ShowValue(i);
                self->OnSliderChanged(i);
                // Tell the screen reader about the new position, as the trackbar does on its own.
                NotifyWinEvent(EVENT_OBJECT_VALUECHANGE, hwnd, OBJID_CLIENT, CHILDID_SELF);
                return 0;
            }
        }
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

INT_PTR SliderDialog::OnMessage(UINT msg, WPARAM, LPARAM lp, bool* handled) {
    *handled = false;
    if (msg == WM_HSCROLL) {
        int i = FindSlider((HWND)lp);
        if (i >= 0) {
            ShowValue(i);
            OnSliderChanged(i);
            *handled = true;
        }
    }
    return 0;
}

HWND SliderDialog::AddPresetCombo(ui::Box* box, const std::vector<std::wstring>& names,
                                  int selected, int id) {
    auto row = HBox();
    HWND lbl = Add(WC_STATICW, tr("&Predefinição:"), 0);
    HWND combo = Add(WC_COMBOBOXW, L"", COMBO, id);
    std::vector<std::wstring> items = names;
    items.push_back(tr("Personalizado"));
    ui::ComboSetItems(combo, items);
    SendMessageW(combo, CB_SETCURSEL, selected < 0 ? items.size() - 1 : selected, 0);
    row->Add(lbl, 0, ui::CENTER_V | ui::RIGHT, S(5));
    row->Add(combo, 1, ui::EXPAND, 0);
    box->Add(std::move(row), 0, ui::ALL | ui::EXPAND, S(10));
    return combo;
}

void SliderDialog::AddOkCancel(ui::Box* box) {
    auto hb = HBox();
    HWND ok = Add(WC_BUTTONW, tr("&Ok"), BUTTON | BS_DEFPUSHBUTTON, IDOK);
    HWND cancel = Add(WC_BUTTONW, tr("&Cancelar"), BUTTON, IDCANCEL);
    hb->Add(ok, 0, ui::RIGHT, S(5));
    hb->Add(cancel, 0, 0, 0);
    box->Add(std::move(hb), 0, ui::CENTER | ui::ALL, S(15));
}

bool SliderDialog::OnCommand(int id, int, HWND) {
    if (id == IDOK) {
        OnOk();
        return true;
    }
    return false;
}

bool SliderDialog::OnCharHook(const MSG& msg) {
    // Enter confirms, except on a button, which does what it says, and on an
    // open list, which closes it.
    if (msg.message != WM_KEYDOWN || msg.wParam != VK_RETURN) return false;
    HWND focus = GetFocus();
    wchar_t cls[32] = L"";
    GetClassNameW(focus, cls, 32);
    if (_wcsicmp(cls, WC_BUTTONW) == 0) {
        LONG type = GetWindowLongW(focus, GWL_STYLE) & BS_TYPEMASK;
        if (type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON) return false;
    }
    if (_wcsicmp(cls, WC_COMBOBOXW) == 0 && SendMessageW(focus, CB_GETDROPPEDSTATE, 0, 0))
        return false;
    OnOk();
    return true;
}

/* ---- Equalizer ---- */

EqualizerDialog::EqualizerDialog(const FxSettings& fx, FxPreview preview)
    : SliderDialog(tr("Equalizador")), result(fx), fx_(fx), preview_(preview) {}

void EqualizerDialog::OnInit() {
    auto vbox = VBox();
    const auto& presets = EqPresets();
    std::vector<std::wstring> names;
    int selected = -1;
    for (size_t i = 0; i < presets.size(); ++i) {
        names.push_back(tr(presets[i].name));
        if (fx_.eq.preset == presets[i].key) selected = (int)i;
    }
    cmb_preset_ = AddPresetCombo(vbox.get(), names, selected, IDC_FX_PRESET);

    auto pre = HBox();
    AddSlider(pre.get(), tr("Pré-amplificação:"), GainPos(fx_.eq.preamp), GainText);
    vbox->Add(std::move(pre), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM | ui::EXPAND, S(10));

    for (int b = 0; b < EQ_BANDS; ++b) {
        auto row = HBox();
        AddSlider(row.get(), FreqName(EQ_FREQS[b]) + L":", GainPos(fx_.eq.gain[b]), GainText);
        vbox->Add(std::move(row), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM | ui::EXPAND, S(10));
    }
    AddOkCancel(vbox.get());
    root_ = std::move(vbox);
    Fit();
}

void EqualizerDialog::ApplyPreset(int index) {
    const auto& presets = EqPresets();
    if (index < 0 || index >= (int)presets.size()) return;  // "Custom": nothing to load
    const EqPreset& p = presets[index];
    fx_.eq.preset = p.key;
    fx_.eq.preamp = p.preamp;
    updating_ = true;
    SetSliderPos(0, GainPos(p.preamp));
    for (int b = 0; b < EQ_BANDS; ++b) {
        fx_.eq.gain[b] = p.gain[b];
        SetSliderPos(1 + b, GainPos(p.gain[b]));
    }
    updating_ = false;
    Changed(false);
}

void EqualizerDialog::Changed(bool custom) {
    if (custom) {
        fx_.eq.preset = FX_CUSTOM;
        SelectCustom(cmb_preset_);
    }
    if (preview_) preview_(fx_);
}

void EqualizerDialog::OnSliderChanged(int i) {
    if (updating_) return;
    int pos = SliderPos(i);
    if (i == 0) fx_.eq.preamp = GainOf(pos);
    else fx_.eq.gain[i - 1] = GainOf(pos);
    Changed(true);
}

bool EqualizerDialog::OnCommand(int id, int code, HWND control) {
    if (id == IDC_FX_PRESET && code == CBN_SELCHANGE) {
        ApplyPreset((int)SendMessageW(cmb_preset_, CB_GETCURSEL, 0, 0));
        return true;
    }
    return SliderDialog::OnCommand(id, code, control);
}

void EqualizerDialog::OnOk() {
    result = fx_;
    EndModal(IDOK);
}

/* ---- Noise remover ---- */

NoiseDialog::NoiseDialog(const FxSettings& fx, FxPreview preview)
    : SliderDialog(tr("Removedor de Ruído")), result(fx), fx_(fx), preview_(preview) {}

void NoiseDialog::OnInit() {
    auto vbox = VBox();
    chk_on_ = Add(WC_BUTTONW, tr("&Ativar Removedor de Ruído"), CHECK, IDC_FX_NOISE_ON);
    Button_SetCheck(chk_on_, fx_.noise.on ? BST_CHECKED : BST_UNCHECKED);
    vbox->Add(chk_on_, 0, ui::ALL, S(10));
    auto row = HBox();
    AddSlider(row.get(), tr("Intensidade:"), (int)std::lround(fx_.noise.amount),
              [](int pos) { return std::to_wstring(pos) + L"%"; });
    vbox->Add(std::move(row), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM | ui::EXPAND, S(10));
    AddOkCancel(vbox.get());
    root_ = std::move(vbox);
    Fit();
}

void NoiseDialog::OnSliderChanged(int i) {
    fx_.noise.amount = (float)SliderPos(i);
    if (preview_) preview_(fx_);
}

bool NoiseDialog::OnCommand(int id, int code, HWND control) {
    if (id == IDC_FX_NOISE_ON && code == BN_CLICKED) {
        fx_.noise.on = Button_GetCheck(chk_on_) == BST_CHECKED;
        if (preview_) preview_(fx_);
        return true;
    }
    return SliderDialog::OnCommand(id, code, control);
}

void NoiseDialog::OnOk() {
    result = fx_;
    EndModal(IDOK);
}
