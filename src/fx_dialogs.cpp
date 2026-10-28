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

#include "background.h"
#include "dialogs.h"
#include "i18n.h"
#include "util.h"

using ui::S;

namespace {

/* The roles in the side chain's list: taking no part, the default, first. */
const int ROLE_ORDER[] = {SC_NONE, SC_REDUCED, SC_REDUCES};

const DWORD BUTTON = WS_TABSTOP | BS_PUSHBUTTON;
const DWORD CHECK = WS_TABSTOP | BS_AUTOCHECKBOX;
const DWORD COMBO = WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST;

enum {
    IDC_FX_PRESET = 501,
    IDC_FX_NOISE_ON,
    IDC_FX_LIST,
    IDC_FX_CONFIG,
    IDC_FX_FILE,
    IDC_FX_BROWSE,
    IDC_FX_ROLE,
    IDC_FX_TRACK = 600,  // + slider index
    IDC_FX_CHECK = 700,  // + parameter index
};

std::unique_ptr<ui::Box> HBox() { return std::unique_ptr<ui::Box>(new ui::Box(false)); }
std::unique_ptr<ui::Box> VBox() { return std::unique_ptr<ui::Box>(new ui::Box(true)); }

/* Gain: from -15 dB (position 0) to +15 dB, 0.5 dB a step, which every
   preset is made of; Page Up and Page Down go 3 dB. The screen reader says
   the dB. */
const float GAIN_STEP = 0.5f;
const int GAIN_MAX = (int)std::lround((EQ_MAX_GAIN - EQ_MIN_GAIN) / GAIN_STEP);
const int GAIN_PAGE = 6;
const int GAIN_ORIGIN = (int)std::lround(EQ_MIN_GAIN / GAIN_STEP);
float GainOf(int pos) { return EQ_MIN_GAIN + pos * GAIN_STEP; }
int GainPos(float db) {
    return std::max(0, std::min(GAIN_MAX, (int)std::lround((db - EQ_MIN_GAIN) / GAIN_STEP)));
}
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

int SliderDialog::AddSlider(ui::Box* row, const std::wstring& label, int pos, ValueText text,
                            int max, int page, bool says_value, int origin) {
    int index = (int)sliders_.size();
    HWND lbl = Add(WC_STATICW, label, 0);
    Slider s;
    s.label = lbl;
    s.text = text;
    s.max = max;
    s.page = page;
    s.origin = origin;
    s.says_value = says_value;
    s.track = Add(TRACKBAR_CLASSW, L"", WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, IDC_FX_TRACK + index);
    SendMessageW(s.track, TBM_SETRANGEMIN, FALSE, 0);
    SendMessageW(s.track, TBM_SETRANGEMAX, TRUE, max);
    SendMessageW(s.track, TBM_SETPAGESIZE, 0, page);
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
    SendMessageW(sliders_[i].track, TBM_SETPOS, TRUE, std::max(0, std::min(sliders_[i].max, pos)));
    ShowValue(i);
}

void SliderDialog::ShowValue(int i) {
    std::wstring text = sliders_[i].text(SliderPos(i));
    ui::SetText(sliders_[i].value, text);
    if (sliders_[i].says_value) ui::SetAccessibleValue(sliders_[i].track, text);
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
            const int max = self->sliders_[i].max, page = self->sliders_[i].page;
            const int origin = self->sliders_[i].origin;
            int pos = (int)SendMessageW(hwnd, TBM_GETPOS, 0, 0), next = pos;
            bool moved = true;
            switch (wp) {
                case VK_HOME: next = 0; break;
                case VK_END: next = max; break;
                case VK_UP:
                case VK_RIGHT: next = pos + 1; break;
                case VK_DOWN:
                case VK_LEFT: next = pos - 1; break;
                // To the next multiple of the page, as the volume slider does,
                // counted on the value: 50, 100, 150 ms, not 60, 110, 160.
                case VK_PRIOR:
                    next = (int)std::floor((pos + origin) / (double)page + 1) * page - origin;
                    break;
                case VK_NEXT:
                    next = (int)std::floor((pos + origin - 1) / (double)page) * page - origin;
                    break;
                default: moved = false;
            }
            if (moved) {
                SendMessageW(hwnd, TBM_SETPOS, TRUE, std::max(0, std::min(max, next)));
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
    AddSlider(pre.get(), tr("Pré-amplificação:"), GainPos(fx_.eq.preamp), GainText, GAIN_MAX,
              GAIN_PAGE, true, GAIN_ORIGIN);
    vbox->Add(std::move(pre), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM | ui::EXPAND, S(10));

    for (int b = 0; b < EQ_BANDS; ++b) {
        auto row = HBox();
        AddSlider(row.get(), FreqName(EQ_FREQS[b]) + L":", GainPos(fx_.eq.gain[b]), GainText,
                  GAIN_MAX, GAIN_PAGE, true, GAIN_ORIGIN);
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

/* ---- Effects ---- */

EffectsDialog::EffectsDialog(const FxSettings& fx, FxPreview preview)
    : SliderDialog(tr("Efeitos")), result(fx), fx_(fx), preview_(preview) {}

void EffectsDialog::OnInit() {
    auto vbox = VBox();
    HWND lbl = Add(WC_STATICW, tr("&Efeitos:"), 0);
    list_ = Add(WC_LISTVIEWW, L"",
                WS_TABSTOP | LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                IDC_FX_LIST, WS_EX_CLIENTEDGE);
    ListView_SetExtendedListViewStyle(list_, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
    LVCOLUMNW col = {};
    col.mask = LVCF_WIDTH;
    col.cx = S(260);
    ListView_InsertColumn(list_, 0, &col);
    for (int k = 0; k < FX_COUNT; ++k) {
        std::wstring name = tr(GetEffectDef(k).name);
        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = k;
        item.pszText = &name[0];
        ListView_InsertItem(list_, &item);
        ListView_SetCheckState(list_, k, fx_.effects[k].on);
    }
    ListView_SetItemState(list_, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    vbox->Add(lbl, 0, ui::LEFT | ui::RIGHT | ui::TOP, S(10));
    vbox->Add(list_, 1, ui::ALL | ui::EXPAND, S(10), SIZE{S(280), S(120)});
    HWND config = Add(WC_BUTTONW, tr("Con&figurar..."), BUTTON, IDC_FX_CONFIG);
    vbox->Add(config, 0, ui::LEFT | ui::RIGHT, S(10));
    AddOkCancel(vbox.get());
    root_ = std::move(vbox);
    Fit();
    ready_ = true;
}

INT_PTR EffectsDialog::OnMessage(UINT msg, WPARAM wp, LPARAM lp, bool* handled) {
    if (msg == WM_NOTIFY && ((NMHDR*)lp)->hwndFrom == list_) {
        NMHDR* hdr = (NMHDR*)lp;
        if (hdr->code == LVN_ITEMCHANGED && ready_) {
            NMLISTVIEW* nm = (NMLISTVIEW*)lp;
            if ((nm->uChanged & LVIF_STATE) &&
                ((nm->uNewState ^ nm->uOldState) & LVIS_STATEIMAGEMASK) && nm->iItem >= 0 &&
                nm->iItem < FX_COUNT) {
                // Checking an effect turns it on, and the test hears it.
                fx_.effects[nm->iItem].on = ListView_GetCheckState(list_, nm->iItem) != 0;
                if (preview_) preview_(fx_);
            }
        } else if (hdr->code == NM_DBLCLK) {
            Configure();
        }
        *handled = true;
        return 0;
    }
    return SliderDialog::OnMessage(msg, wp, lp, handled);
}

void EffectsDialog::Configure() {
    int k = ListView_GetNextItem(list_, -1, LVNI_SELECTED);
    if (k < 0) k = ListView_GetNextItem(list_, -1, LVNI_FOCUSED);
    if (k < 0 || k >= FX_COUNT) return;
    EffectConfigDialog dlg(k, fx_, preview_);
    if (dlg.ShowModal(hwnd_) == IDOK) {
        fx_.effects[k].preset = dlg.result.effects[k].preset;
        fx_.effects[k].values = dlg.result.effects[k].values;
    }
    if (preview_) preview_(fx_);
    SetFocus(list_);
}

bool EffectsDialog::OnCommand(int id, int code, HWND control) {
    if (id == IDC_FX_CONFIG && code == BN_CLICKED) {
        Configure();
        return true;
    }
    return SliderDialog::OnCommand(id, code, control);
}

void EffectsDialog::OnOk() {
    result = fx_;
    EndModal(IDOK);
}

/* ---- One effect ---- */

EffectConfigDialog::EffectConfigDialog(int kind, const FxSettings& fx, FxPreview preview)
    : SliderDialog(tr(GetEffectDef(kind).name)),
      result(fx),
      kind_(kind),
      fx_(fx),
      preview_(preview) {}

void EffectConfigDialog::OnInit() {
    const EffectDef& def = GetEffectDef(kind_);
    const EffectSettings& e = fx_.effects[kind_];
    auto vbox = VBox();
    std::vector<std::wstring> names;
    int selected = -1;
    for (size_t i = 0; i < def.presets.size(); ++i) {
        names.push_back(tr(def.presets[i].name));
        if (e.preset == def.presets[i].key) selected = (int)i;
    }
    cmb_preset_ = AddPresetCombo(vbox.get(), names, selected, IDC_FX_PRESET);

    if (def.takes_file) {
        // The impulse response: typed, or found with the button.
        HWND label = Add(WC_STATICW, tr("Arquivo de &Impulso:"), 0);
        txt_file_ = Add(WC_EDITW, e.file, WS_TABSTOP | ES_AUTOHSCROLL, IDC_FX_FILE, WS_EX_CLIENTEDGE);
        HWND browse = Add(WC_BUTTONW, tr("Proc&urar..."), BUTTON, IDC_FX_BROWSE);
        vbox->Add(label, 0, ui::LEFT | ui::RIGHT, S(10));
        auto row = HBox();
        row->Add(txt_file_, 1, ui::EXPAND | ui::RIGHT, S(5), SIZE{S(300), 0});
        row->Add(browse, 0, 0, 0);
        vbox->Add(std::move(row), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM | ui::EXPAND, S(10));
    }

    check_of_.assign(def.params.size(), nullptr);
    for (size_t i = 0; i < def.params.size(); ++i) {
        const FxParam& p = def.params[i];
        float v = i < e.values.size() ? e.values[i] : p.min;
        if (p.toggle) {
            check_of_[i] = Add(WC_BUTTONW, tr(p.label), CHECK, IDC_FX_CHECK + (int)i);
            Button_SetCheck(check_of_[i], v > 0.5f ? BST_CHECKED : BST_UNCHECKED);
            vbox->Add(check_of_[i], 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));
        } else {
            auto row = HBox();
            AddSlider(row.get(), tr(p.label), ParamPos(p, v),
                      [p](int pos) { return FormatParam(p, ParamValue(p, pos)); },
                      ParamSteps(p), ParamPage(p), true, (int)std::lround(p.min / p.step));
            param_of_slider_.push_back((int)i);
            vbox->Add(std::move(row), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM | ui::EXPAND, S(10));
        }
    }
    AddOkCancel(vbox.get());
    root_ = std::move(vbox);
    Fit();
}

void EffectConfigDialog::ApplyPreset(int index) {
    const EffectDef& def = GetEffectDef(kind_);
    if (index < 0 || index >= (int)def.presets.size()) return;  // "Custom": nothing to load
    EffectSettings& e = fx_.effects[kind_];
    e.preset = def.presets[index].key;
    e.values = def.presets[index].values;
    updating_ = true;
    for (size_t s = 0; s < param_of_slider_.size(); ++s) {
        int i = param_of_slider_[s];
        SetSliderPos((int)s, ParamPos(def.params[i], e.values[i]));
    }
    for (size_t i = 0; i < check_of_.size(); ++i)
        if (check_of_[i])
            Button_SetCheck(check_of_[i], e.values[i] > 0.5f ? BST_CHECKED : BST_UNCHECKED);
    updating_ = false;
    Changed(false);
}

void EffectConfigDialog::Changed(bool custom) {
    if (custom) {
        fx_.effects[kind_].preset = FX_CUSTOM;
        SelectCustom(cmb_preset_);
    }
    if (preview_) preview_(fx_);
}

void EffectConfigDialog::OnSliderChanged(int s) {
    if (updating_ || s < 0 || s >= (int)param_of_slider_.size()) return;
    int i = param_of_slider_[s];
    fx_.effects[kind_].values[i] = ParamValue(GetEffectDef(kind_).params[i], SliderPos(s));
    Changed(true);
}

bool EffectConfigDialog::OnCommand(int id, int code, HWND control) {
    if (id == IDC_FX_PRESET && code == CBN_SELCHANGE) {
        ApplyPreset((int)SendMessageW(cmb_preset_, CB_GETCURSEL, 0, 0));
        return true;
    }
    if (id == IDC_FX_FILE && code == EN_CHANGE) {
        // The file is not part of the preset: changing it keeps the preset.
        fx_.effects[kind_].file = Trim(ui::GetText(txt_file_));
        if (preview_) preview_(fx_);
        return true;
    }
    if (id == IDC_FX_BROWSE && code == BN_CLICKED) {
        std::wstring path;
        if (PickFile(hwnd_, tr("Arquivo de Impulso"), tr("Arquivos de Áudio"), BackgroundPattern(), &path)) {
            SetWindowTextW(txt_file_, path.c_str());
            SetFocus(txt_file_);
        }
        return true;
    }
    int i = id - IDC_FX_CHECK;
    if (i >= 0 && i < (int)check_of_.size() && check_of_[i] && code == BN_CLICKED) {
        fx_.effects[kind_].values[i] = Button_GetCheck(check_of_[i]) == BST_CHECKED ? 1.0f : 0.0f;
        Changed(true);
        return true;
    }
    return SliderDialog::OnCommand(id, code, control);
}

void EffectConfigDialog::OnOk() {
    const std::wstring& file = fx_.effects[kind_].file;
    if (txt_file_ && !file.empty() && !FileExists(file)) {
        ui::MsgBox(hwnd_, Format(tr("O arquivo de impulso não foi encontrado:\n{arquivo}"), {{L"arquivo", file}}),
                   tr("Aviso"), MB_ICONWARNING | MB_OK);
        SetFocus(txt_file_);
        return;
    }
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

/* ---- Side chain ---- */

SidechainDialog::SidechainDialog(const FxSettings& fx)
    : SliderDialog(tr("Cadeia Lateral")), result(fx), fx_(fx) {}

void SidechainDialog::OnInit() {
    auto vbox = VBox();
    auto top = HBox();
    HWND lbl = Add(WC_STATICW, tr("&Papel desta fonte:"), 0);
    // In the order of ROLE_ORDER.
    cmb_role_ = Add(WC_COMBOBOXW, L"", COMBO, IDC_FX_ROLE);
    ui::ComboSetItems(cmb_role_, {tr("Não participa"), tr("É reduzida pelas outras fontes"),
                                  tr("Reduz o volume das outras fontes")});
    for (int i = 0; i < 3; ++i)
        if (ROLE_ORDER[i] == fx_.sidechain.role) SendMessageW(cmb_role_, CB_SETCURSEL, i, 0);
    top->Add(lbl, 0, ui::CENTER_V | ui::RIGHT, S(5));
    top->Add(cmb_role_, 1, ui::EXPAND, 0);
    vbox->Add(std::move(top), 0, ui::ALL | ui::EXPAND, S(10));

    const auto& params = SidechainParams();
    for (int i = 0; i < SC_PARAMS; ++i) {
        const FxParam& p = params[i];
        auto row = HBox();
        AddSlider(row.get(), tr(p.label), ParamPos(p, fx_.sidechain.values[i]),
                  [p, i](int pos) {
                      if (i == SC_REDUCTION && pos == ParamSteps(p)) return tr("Silêncio");
                      return FormatParam(p, ParamValue(p, pos));
                  },
                  ParamSteps(p), ParamPage(p), true, (int)std::lround(p.min / p.step));
        vbox->Add(std::move(row), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM | ui::EXPAND, S(10));
    }
    AddOkCancel(vbox.get());
    root_ = std::move(vbox);
    ShowRole();
}

/* Only the controls of the role chosen are shown. */
void SidechainDialog::ShowRole() {
    for (int i = 0; i < SC_PARAMS; ++i) {
        bool show = fx_.sidechain.role == SC_REDUCES   ? i <= SC_HOLD
                    : fx_.sidechain.role == SC_REDUCED ? i >= SC_REDUCTION
                                                       : false;
        ui::Show(sliders_[i].label, show);
        ui::Show(sliders_[i].track, show);
        ui::Show(sliders_[i].value, show);
    }
    Fit();
}

void SidechainDialog::OnSliderChanged(int i) {
    fx_.sidechain.values[i] = ParamValue(SidechainParams()[i], SliderPos(i));
}

bool SidechainDialog::OnCommand(int id, int code, HWND control) {
    if (id == IDC_FX_ROLE && code == CBN_SELCHANGE) {
        int sel = (int)SendMessageW(cmb_role_, CB_GETCURSEL, 0, 0);
        if (sel >= 0 && sel < 3) fx_.sidechain.role = ROLE_ORDER[sel];
        ShowRole();
        return true;
    }
    return SliderDialog::OnCommand(id, code, control);
}

void SidechainDialog::OnOk() {
    result = fx_;
    EndModal(IDOK);
}
