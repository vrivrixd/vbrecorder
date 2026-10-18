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

#include "dialogs.h"

#include <commctrl.h>
#include <shobjidl.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <set>

#include "fx_dialogs.h"
#include "hotkeys.h"
#include "i18n.h"
#include "process_audio.h"
#include "util.h"

using ui::S;

namespace {

/* Group of radio buttons with frame and title (wx.RadioBox), on one row. */
class RadioBoxItem : public ui::Item {
public:
    RadioBoxItem(HWND group, std::vector<HWND> radios) : group_(group), radios_(radios) {}

    SIZE Min() override {
        SIZE label = ui::BestSize(group_);
        LONG width = 0, height = 0;
        for (HWND r : radios_) {
            SIZE m = ui::BestSize(r);
            width += m.cx + S(10);
            height = std::max(height, m.cy);
        }
        return SIZE{std::max(label.cx + S(20), width + S(10)), label.cy + height + S(14)};
    }
    bool Shown() override { return true; }
    void Place(int x, int y, int w, int h) override {
        SetWindowPos(group_, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
        SIZE label = ui::BestSize(group_);
        int rx = x + S(10);
        for (HWND r : radios_) {
            SIZE m = ui::BestSize(r);
            int ry = y + label.cy + (h - label.cy - m.cy) / 2 - S(2);
            SetWindowPos(r, nullptr, rx, ry, m.cx, m.cy, SWP_NOZORDER | SWP_NOACTIVATE);
            rx += m.cx + S(10);
        }
    }

private:
    HWND group_;
    std::vector<HWND> radios_;
};

std::unique_ptr<ui::Box> HBox() { return std::unique_ptr<ui::Box>(new ui::Box(false)); }
std::unique_ptr<ui::Box> VBox() { return std::unique_ptr<ui::Box>(new ui::Box(true)); }

bool IsEnter(const MSG& msg) { return msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN; }

const DWORD BUTTON = WS_TABSTOP | BS_PUSHBUTTON;
const DWORD CHECK = WS_TABSTOP | BS_AUTOCHECKBOX;
const DWORD COMBO = WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST;
const DWORD EDIT = WS_TABSTOP | ES_AUTOHSCROLL;

/* The volume slider: 50 is 0 dB, from -30 dB (0) to +30 dB (100).
   The screen reader says the position. */
const double VOLUME_DB = 30.0;
const double VOLUME_STEP = VOLUME_DB / 50;

}  // namespace

/* ---- Hotkeys ---- */

enum { IDC_HK_FIRST = 301, IDC_HK_OK = 310 };

HotkeysDialog::HotkeysDialog(ConfigManager& config)
    : ui::Dialog(tr("Configurar Teclas")), config_(config) {}

void HotkeysDialog::OnInit() {
    auto vbox = VBox();
    HWND help = Add(WC_STATICW,
                    tr("Pressione a combinação de teclas (ex: Ctrl+Alt+R) nos campos abaixo.\n"
                       "Para limpar, pressione Backspace ou Delete.\nPressione Esc para cancelar."),
                    0);
    // The text wraps at 400 pixels, like Wrap(400) in the previous version.
    HDC dc = GetDC(hwnd_);
    HGDIOBJ old = SelectObject(dc, ui::Font());
    RECT r = {0, 0, S(400), 0};
    std::wstring help_text = ui::GetText(help);
    DrawTextW(dc, help_text.c_str(), -1, &r, DT_CALCRECT | DT_WORDBREAK);
    SelectObject(dc, old);
    ReleaseDC(hwnd_, dc);
    vbox->Add(help, 0, ui::ALL, S(10), SIZE{r.right, r.bottom});

    auto grid = std::unique_ptr<ui::Grid>(new ui::Grid(S(8), S(8)));
    const std::pair<const wchar_t*, std::wstring> keys[] = {
        {L"gravar", tr("Gravar:")},
        {L"pausar", tr("Pausar / Continuar:")},
        {L"parar", tr("Parar Gravação:")},
        {L"reproduzir", tr("Reproduzir / Parar Reprodução:")},
        {L"excluir", tr("Excluir Gravação:")},
        {L"mostrar", tr("Mostrar Janela Principal:")},
        {L"fechar", tr("Fechar Programa:")},
    };
    int id = IDC_HK_FIRST;
    for (auto& k : keys) {
        HWND lbl = Add(WC_STATICW, k.second, 0);
        HWND ctrl = Add(WC_EDITW, config_.GetKey(k.first), EDIT | ES_CENTER, id++,
                        WS_EX_CLIENTEDGE);
        MakeHotkeyInput(ctrl);
        std::wstring tip = k.second;
        tip.erase(std::remove(tip.begin(), tip.end(), L':'), tip.end());
        ui::SetToolTip(hwnd_, ctrl, Trim(tip));
        grid->Add(lbl, ui::CENTER_V);
        grid->Add(ctrl, ui::EXPAND);
        ctrls_.emplace_back(k.first, ctrl);
    }
    vbox->Add(std::move(grid), 0, ui::ALL | ui::EXPAND, S(10));

    auto hb = HBox();
    HWND btn_s = Add(WC_BUTTONW, tr("&Ok"), BUTTON, IDC_HK_OK);
    HWND btn_c = Add(WC_BUTTONW, tr("&Cancelar"), BUTTON, IDCANCEL);
    hb->Add(btn_s, 0, ui::RIGHT, S(10));
    hb->Add(btn_c, 0, 0, 0);
    vbox->Add(std::move(hb), 0, ui::CENTER | ui::ALL, S(15));
    root_ = std::move(vbox);
    Fit();
}

bool HotkeysDialog::OnCommand(int id, int code, HWND) {
    if (id == IDC_HK_OK && code == BN_CLICKED) {
        for (auto& c : ctrls_) config_.SetKey(c.first, ui::GetText(c.second));
        config_.Save();
        EndModal(IDOK);
        return true;
    }
    // Enter outside a button does nothing here: there is no default button.
    return id == IDOK;
}

/* ---- Settings ---- */

enum {
    IDC_ST_LANG = 201,
    IDC_ST_START_MIN,
    IDC_ST_TRAY,
    IDC_ST_ON_TOP,
    IDC_ST_BUFFER,
    IDC_ST_KEYS,
    IDC_ST_UPDATES,
    IDC_ST_CHECK,
    IDC_ST_STARTUP,
    IDC_ST_COUNTDOWN,
};

SettingsDialog::SettingsDialog(ConfigManager& config, std::function<void()> on_keys_saved,
                               std::function<bool(HWND)> check_update)
    : ui::Dialog(tr("Configurações")),
      config_(config),
      on_keys_saved_(on_keys_saved),
      check_update_(check_update) {}

void SettingsDialog::OnInit() {
    auto vbox = VBox();

    // The list shows each language in itself, and the first item lets the
    // program follow Windows.
    codes_ = {L""};
    std::vector<std::wstring> choices = {tr("Padrão do sistema")};
    for (auto& c : i18n::AvailableLanguages()) {
        codes_.push_back(c);
        choices.push_back(i18n::LanguageName(c));
    }
    auto grid = std::unique_ptr<ui::Grid>(new ui::Grid(S(8), S(8)));
    HWND lbl_lang = Add(WC_STATICW, tr("&Idioma:"), 0);
    cmb_lang_ = Add(WC_COMBOBOXW, L"", COMBO, IDC_ST_LANG);
    ui::ComboSetItems(cmb_lang_, choices);
    std::wstring current = config_.Get(L"idioma");
    auto it = std::find(codes_.begin(), codes_.end(), current);
    SendMessageW(cmb_lang_, CB_SETCURSEL, it == codes_.end() ? 0 : it - codes_.begin(), 0);
    grid->Add(lbl_lang, ui::CENTER_V);
    grid->Add(cmb_lang_, ui::EXPAND);
    vbox->Add(std::move(grid), 0, ui::ALL | ui::EXPAND, S(10));

    chk_updates_ = Add(WC_BUTTONW, tr("&Verificar Atualizações Automaticamente"), CHECK,
                       IDC_ST_UPDATES);
    Button_SetCheck(chk_updates_, config_.GetBoolean(L"verificar_atualizacoes", true));
    vbox->Add(chk_updates_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));
    btn_check_ = Add(WC_BUTTONW, tr("Verificar Atuali&zação..."), BUTTON, IDC_ST_CHECK);
    vbox->Add(btn_check_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    chk_startup_ = Add(WC_BUTTONW, tr("Iniciar com o &Windows"), CHECK, IDC_ST_STARTUP);
    Button_SetCheck(chk_startup_, config_.GetBoolean(L"iniciar_com_windows"));
    vbox->Add(chk_startup_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    chk_start_min_ = Add(WC_BUTTONW, tr("Iniciar &Minimizado"), CHECK, IDC_ST_START_MIN);
    Button_SetCheck(chk_start_min_, config_.GetBoolean(L"iniciar_minimizado"));
    vbox->Add(chk_start_min_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    chk_tray_ = Add(WC_BUTTONW, tr("Minimizar Para a &Bandeja do Sistema"), CHECK, IDC_ST_TRAY);
    Button_SetCheck(chk_tray_, config_.GetBoolean(L"minimizar_bandeja"));
    vbox->Add(chk_tray_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    chk_on_top_ = Add(WC_BUTTONW, tr("Janela Sempre no &Topo"), CHECK, IDC_ST_ON_TOP);
    Button_SetCheck(chk_on_top_, config_.GetBoolean(L"sempre_no_topo", true));
    vbox->Add(chk_on_top_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    chk_countdown_ = Add(WC_BUTTONW, tr("Contagem &Regressiva Antes de Gravar"), CHECK,
                         IDC_ST_COUNTDOWN);
    Button_SetCheck(chk_countdown_, config_.GetBoolean(L"contagem_regressiva"));
    vbox->Add(chk_countdown_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    auto hb_buf = HBox();
    HWND lbl_buf = Add(WC_STATICW, tr("Tamanho do B&uffer (ms):"), 0);
    txt_buffer_ = Add(WC_EDITW, std::to_wstring(config_.GetBuffer()), EDIT, IDC_ST_BUFFER,
                      WS_EX_CLIENTEDGE);
    // The explanation goes in the field's tooltip, not in loose text in the
    // dialog: screen readers read loose text first, when the dialog opens.
    ui::SetToolTip(hwnd_, txt_buffer_,
                   Format(tr("Aumente o buffer se o áudio estalar em um computador ocupado. "
                             "De {minimo} a {maximo} ms; o padrão é {padrao}."),
                          {{L"minimo", std::to_wstring(BUFFER_MIN)},
                           {L"maximo", std::to_wstring(BUFFER_MAX)},
                           {L"padrao", std::to_wstring(BUFFER_DEFAULT)}}));
    hb_buf->Add(lbl_buf, 0, ui::CENTER_V | ui::RIGHT, S(5));
    hb_buf->Add(txt_buffer_, 0, ui::CENTER_V, 0, SIZE{S(80), 0});
    vbox->Add(std::move(hb_buf), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    btn_keys_ = Add(WC_BUTTONW, tr("Configurar Teclas de &Atalho..."), BUTTON, IDC_ST_KEYS);
    vbox->Add(btn_keys_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    auto hb = HBox();
    btn_ok_ = Add(WC_BUTTONW, tr("&Ok"), BUTTON | BS_DEFPUSHBUTTON, IDOK);
    btn_cancel_ = Add(WC_BUTTONW, tr("&Cancelar"), BUTTON, IDCANCEL);
    hb->Add(btn_ok_, 0, ui::RIGHT, S(10));
    hb->Add(btn_cancel_, 0, 0, 0);
    vbox->Add(std::move(hb), 0, ui::CENTER | ui::ALL, S(15));
    root_ = std::move(vbox);
    Fit();
}

bool SettingsDialog::OnCharHook(const MSG& msg) {
    // Enter confirms from anywhere, except on a button, where it does what
    // that button does.
    HWND focus = GetFocus();
    if (IsEnter(msg) && focus != btn_ok_ && focus != btn_cancel_ && focus != btn_keys_ &&
        focus != btn_check_) {
        OnOk();
        return true;
    }
    return false;
}

bool SettingsDialog::OnCommand(int id, int code, HWND) {
    if (id == IDOK) {
        OnOk();
        return true;
    }
    if (id == IDC_ST_CHECK && code == BN_CLICKED) {
        if (check_update_ && check_update_(hwnd_)) {
            updating = true;
            EndModal(IDCANCEL);
        }
        return true;
    }
    if (id == IDC_ST_KEYS && code == BN_CLICKED) {
        HotkeysDialog dlg(config_);
        if (dlg.ShowModal(hwnd_) == IDOK && on_keys_saved_) on_keys_saved_();
        return true;
    }
    return false;
}

int SettingsDialog::BufferValue() const {
    std::wstring text = Trim(buffer_text_);
    size_t start = (!text.empty() && (text[0] == L'+' || text[0] == L'-')) ? 1 : 0;
    if (text.size() == start || text.size() > 9 ||
        text.find_first_not_of(L"0123456789", start) != std::wstring::npos)
        return -1;
    int value = _wtoi(text.c_str());
    return (value >= BUFFER_MIN && value <= BUFFER_MAX) ? value : -1;
}

void SettingsDialog::OnOk() {
    buffer_text_ = ui::GetText(txt_buffer_);
    if (BufferValue() < 0) {
        ui::MsgBox(hwnd_,
                       Format(tr("O tamanho do buffer deve ser um número entre {minimo} e "
                                 "{maximo} milissegundos."),
                              {{L"minimo", std::to_wstring(BUFFER_MIN)},
                               {L"maximo", std::to_wstring(BUFFER_MAX)}}),
                       tr("Aviso"), MB_ICONWARNING | MB_OK);
        SetFocus(txt_buffer_);
        SendMessageW(txt_buffer_, EM_SETSEL, 0, -1);
        return;
    }
    int sel = (int)SendMessageW(cmb_lang_, CB_GETCURSEL, 0, 0);
    language_ = (sel >= 0 && sel < (int)codes_.size()) ? codes_[sel] : L"";
    start_minimized = Button_GetCheck(chk_start_min_) == BST_CHECKED;
    check_updates = Button_GetCheck(chk_updates_) == BST_CHECKED;
    start_with_windows = Button_GetCheck(chk_startup_) == BST_CHECKED;
    countdown = Button_GetCheck(chk_countdown_) == BST_CHECKED;
    tray = Button_GetCheck(chk_tray_) == BST_CHECKED;
    on_top = Button_GetCheck(chk_on_top_) == BST_CHECKED;
    EndModal(IDOK);
}

/* ---- Add or edit source ---- */

enum {
    IDC_AS_DEVICES = 101,
    IDC_AS_PROCESSES,
    IDC_AS_SOURCE,
    IDC_AS_VOLUME,
    IDC_AS_TEST,
    IDC_AS_EXE,
    IDC_AS_EXE_NAME,
    IDC_AS_REFRESH,
    IDC_AS_ALL_PROCESSES,
    IDC_AS_STEREO,
    IDC_AS_EQ,
    IDC_AS_NOISE,
};

AddSourceDialog::AddSourceDialog(AudioEngine& audio, const std::vector<Source>& existing,
                                 const Source* edit_source,
                                 std::function<std::pair<int, int>()> output_format)
    : ui::Dialog(edit_source ? tr("Editar Fonte de Gravação") : tr("Adicionar Fonte de Gravação")),
      audio_(audio),
      existing_(existing),
      editing_(edit_source != nullptr),
      output_format_(output_format) {
    if (edit_source) {
        edit_source_ = *edit_source;
        fx_ = edit_source->fx;
    }
}

void AddSourceDialog::OnInit() {
    auto all_devices = audio_.GetInputDevices();
    std::set<std::pair<std::wstring, int>> existing_ids = ExistingIds();
    for (auto& d : all_devices)
        if (!existing_ids.count({L"device", d.first}))
            device_items_.emplace_back(L"device", d.first, d.second);
    const bool available = ProcessLoopbackAvailable();
    if (available) LoadProcesses();

    auto vbox = VBox();

    grp_ = Add(WC_BUTTONW, tr("Filtrar Fontes Por:"), BS_GROUPBOX);
    rb_devices_ = Add(WC_BUTTONW, tr("Dispositivos de Áudio"),
                      WS_TABSTOP | WS_GROUP | BS_AUTORADIOBUTTON, IDC_AS_DEVICES);
    rb_processes_ = Add(WC_BUTTONW, tr("Processos em Execução"), BS_AUTORADIOBUTTON,
                        IDC_AS_PROCESSES);
    rb_exe_ = Add(WC_BUTTONW, tr("Arquivo Executável"), BS_AUTORADIOBUTTON, IDC_AS_EXE);
    Button_SetCheck(rb_devices_, BST_CHECKED);
    if (!available) {
        EnableWindow(rb_processes_, FALSE);
        EnableWindow(rb_exe_, FALSE);
    }
    vbox->Add(std::unique_ptr<ui::Item>(
                  new RadioBoxItem(grp_, {rb_devices_, rb_processes_, rb_exe_})),
              0, ui::ALL | ui::EXPAND, S(10));

    lbl_src_ = Add(WC_STATICW, tr("Selecionar Fonte:"), WS_GROUP);
    cmb_ = Add(WC_COMBOBOXW, L"", COMBO, IDC_AS_SOURCE);
    vbox->Add(lbl_src_, 0, ui::LEFT | ui::RIGHT, S(10));
    vbox->Add(cmb_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM | ui::EXPAND, S(10));
    // An executable is typed by name: it does not need to be running.
    lbl_exe_ = ui::Create(hwnd_, WC_STATICW, tr("Nome do Executável:"), 0, 0);
    txt_exe_ = ui::Create(hwnd_, WC_EDITW, L"", WS_TABSTOP | ES_AUTOHSCROLL, IDC_AS_EXE_NAME,
                          WS_EX_CLIENTEDGE);
    vbox->Add(lbl_exe_, 0, ui::LEFT | ui::RIGHT, S(10));
    vbox->Add(txt_exe_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM | ui::EXPAND, S(10));
    // Unchecked, this source is recorded in mono, whatever the recording is.
    chk_stereo_ = Add(WC_BUTTONW, tr("E&stéreo"), CHECK, IDC_AS_STEREO);
    Button_SetCheck(chk_stereo_, fx_.stereo ? BST_CHECKED : BST_UNCHECKED);
    vbox->Add(chk_stereo_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));
    // Also the processes not playing sound right now, which a program that
    // only plays now and then needs.
    chk_all_ = Add(WC_BUTTONW, tr("&Mostrar Todos os Processos"), CHECK, IDC_AS_ALL_PROCESSES);
    vbox->Add(chk_all_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));
    // Reads the processes again, for programs opened after the dialog.
    btn_refresh_ = Add(WC_BUTTONW, tr("&Atualizar"), BUTTON, IDC_AS_REFRESH);
    vbox->Add(btn_refresh_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    auto vol_box = HBox();
    HWND lbl_vol = Add(WC_STATICW, tr("Volume:"), 0);
    sld_ = Add(TRACKBAR_CLASSW, L"", WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, IDC_AS_VOLUME);
    SendMessageW(sld_, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
    SendMessageW(sld_, TBM_SETPAGESIZE, 0, 10);
    SendMessageW(sld_, TBM_SETPOS, TRUE, 50);
    SetWindowSubclass(sld_, SliderProc, 1, (DWORD_PTR)this);
    lbl_db_ = Add(WC_STATICW, L"  0.0 dB", 0);
    vol_box->Add(lbl_vol, 0, ui::CENTER_V | ui::RIGHT, S(5));
    vol_box->Add(sld_, 1, ui::EXPAND | ui::RIGHT, S(5));
    vol_box->Add(lbl_db_, 0, ui::CENTER_V, 0, SIZE{S(70), 0});
    vbox->Add(std::move(vol_box), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM | ui::EXPAND, S(10));

    btn_eq_ = Add(WC_BUTTONW, tr("&Equalizador"), BUTTON, IDC_AS_EQ);
    // Only audio devices: a program's sound is already clean.
    btn_noise_ = Add(WC_BUTTONW, tr("Removedor de &Ruído"), BUTTON, IDC_AS_NOISE);
    // Last: it plays what the controls above set.
    btn_test_ = Add(WC_BUTTONW, tr("&Testar"), BUTTON, IDC_AS_TEST);
    ui::SetToolTip(hwnd_, btn_test_, tr("Ouvir o dispositivo selecionado com o volume ajustado"));
    auto tools = HBox();
    tools->Add(btn_eq_, 0, ui::RIGHT, S(5));
    tools->Add(btn_noise_, 0, ui::RIGHT, S(5));
    tools->Add(btn_test_, 0, 0, 0);
    vbox->Add(std::move(tools), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    if (!available) {
        note_ = Add(WC_STATICW,
                    tr("Nota: captura por processo requer Windows 10 v2004 ou superior."), 0);
        vbox->Add(note_, 0, ui::LEFT | ui::BOTTOM, S(10));
    }

    auto hb = HBox();
    btn_ok_ = Add(WC_BUTTONW, tr("&Ok"), BUTTON | BS_DEFPUSHBUTTON, IDOK);
    btn_cancel_ = Add(WC_BUTTONW, tr("&Cancelar"), BUTTON, IDCANCEL);
    hb->Add(btn_ok_, 0, ui::RIGHT, S(5));
    hb->Add(btn_cancel_, 0, 0, 0);
    vbox->Add(std::move(hb), 0, ui::CENTER | ui::ALL, S(15));
    root_ = std::move(vbox);

    if (editing_ && edit_source_.type == L"process") {
        Button_SetCheck(rb_devices_, BST_UNCHECKED);
        Button_SetCheck(rb_processes_, BST_CHECKED);
        PopulateFilter(1);
    } else if (editing_ && edit_source_.type == L"exe") {
        Button_SetCheck(rb_devices_, BST_UNCHECKED);
        Button_SetCheck(rb_exe_, BST_CHECKED);
        PopulateFilter(2);
        // Only now: typing in the field updates the dialog, which must be ready.
        SetWindowTextW(txt_exe_, edit_source_.name.c_str());
    } else {
        PopulateFilter(0);
    }

    if (editing_) {
        for (size_t i = 0; i < current_items_->size(); ++i) {
            auto& item = (*current_items_)[i];
            if (std::get<0>(item) == edit_source_.type && std::get<1>(item) == edit_source_.id) {
                SendMessageW(cmb_, CB_SETCURSEL, i, 0);
                break;
            }
        }
        double vol = edit_source_.volume;
        double db = vol > 0 ? 20 * std::log10(vol) : -VOLUME_DB;
        int slider_val = std::min(100, std::max(0, (int)std::nearbyint(db / VOLUME_STEP + 50)));
        SendMessageW(sld_, TBM_SETPOS, TRUE, slider_val);
    }
    UpdateDbLabel();
    Fit();
}

bool AddSourceDialog::OnCharHook(const MSG& msg) {
    HWND focus = GetFocus();
    if (IsEnter(msg) && focus != btn_test_ && focus != btn_refresh_ && focus != btn_eq_ &&
        focus != btn_noise_ && focus != btn_cancel_) {
        OnOk();
        return true;
    }
    return false;
}

INT_PTR AddSourceDialog::OnMessage(UINT msg, WPARAM wp, LPARAM lp, bool* handled) {
    *handled = false;
    if (msg == WM_HSCROLL && (HWND)lp == sld_) {
        UpdateDbLabel();
        *handled = true;
        return 0;
    }
    if (msg == WM_CTLCOLORSTATIC && note_ && (HWND)lp == note_) {
        HDC dc = (HDC)wp;
        SetTextColor(dc, RGB(120, 120, 120));
        SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
        *handled = true;
        return (INT_PTR)GetSysColorBrush(COLOR_BTNFACE);
    }
    return 0;
}

bool AddSourceDialog::OnCommand(int id, int code, HWND) {
    switch (id) {
        case IDC_AS_DEVICES:
        case IDC_AS_PROCESSES:
        case IDC_AS_EXE:
            if (code == BN_CLICKED && CheckedFilter() != filter_) OnFilterChange();
            return true;
        case IDC_AS_EXE_NAME:
            if (code == EN_CHANGE) UpdateOkState();
            return true;
        case IDC_AS_SOURCE:
            if (code == CBN_SELCHANGE) {
                StopTest();
                UpdateTestState();
            }
            return true;
        case IDC_AS_TEST:
            if (code == BN_CLICKED) OnTest();
            return true;
        case IDC_AS_REFRESH:
        case IDC_AS_ALL_PROCESSES:
            if (code == BN_CLICKED) OnRefresh();
            return true;
        case IDC_AS_STEREO:
            if (code == BN_CLICKED) {
                fx_.stereo = Button_GetCheck(chk_stereo_) == BST_CHECKED;
                Preview(fx_);
            }
            return true;
        case IDC_AS_EQ:
            if (code == BN_CLICKED) OnEqualizer();
            return true;
        case IDC_AS_NOISE:
            if (code == BN_CLICKED) OnNoise();
            return true;
        case IDOK:
            OnOk();
            return true;
    }
    return false;
}

int AddSourceDialog::CheckedFilter() const {
    if (Button_GetCheck(rb_exe_) == BST_CHECKED) return 2;
    return Button_GetCheck(rb_processes_) == BST_CHECKED ? 1 : 0;
}

void AddSourceDialog::UpdateOkState() {
    if (!current_items_) return;  // the dialog is still being built
    bool ok = filter_ == 2 ? !Trim(ui::GetText(txt_exe_)).empty() : !current_items_->empty();
    EnableWindow(btn_ok_, ok);
}

void AddSourceDialog::PopulateFilter(int sel) {
    filter_ = sel;
    bool exe = sel == 2;
    ui::Show(lbl_src_, !exe);
    ui::Show(cmb_, !exe);
    ui::Show(chk_all_, sel == 1);
    ui::Show(btn_refresh_, sel == 1);
    ui::Show(btn_noise_, sel == 0);
    ui::Show(lbl_exe_, exe);
    ui::Show(txt_exe_, exe);
    Relayout();
    current_items_ = sel == 0 ? &device_items_ : sel == 1 ? &process_items_ : &exe_items_;
    std::vector<std::wstring> choices;
    for (auto& item : *current_items_) choices.push_back(std::get<2>(item));
    ui::ComboSetItems(cmb_, choices);
    if (!choices.empty()) SendMessageW(cmb_, CB_SETCURSEL, 0, 0);
    UpdateOkState();
    UpdateTestState();
}

/* Sources already in the list, which are not offered again; the one being
   edited stays available. */
std::set<std::pair<std::wstring, int>> AddSourceDialog::ExistingIds() const {
    std::set<std::pair<std::wstring, int>> ids;
    for (auto& s : existing_) ids.insert({s.type, s.id});
    if (editing_) ids.erase({edit_source_.type, edit_source_.id});
    return ids;
}

void AddSourceDialog::LoadProcesses() {
    std::set<std::pair<std::wstring, int>> existing_ids = ExistingIds();
    process_items_.clear();
    bool all = chk_all_ && Button_GetCheck(chk_all_) == BST_CHECKED;  // none yet in OnInit
    for (auto& p : all ? GetAllProcesses() : GetAudioSessions())
        if (!existing_ids.count({L"process", (int)p.first}))
            process_items_.emplace_back(L"process", (int)p.first,
                                        p.second + L"  (PID " + std::to_wstring(p.first) + L")");
}

void AddSourceDialog::OnRefresh() {
    // The selected process stays selected if it is still running.
    int sel = (int)SendMessageW(cmb_, CB_GETCURSEL, 0, 0);
    int pid = sel >= 0 && sel < (int)process_items_.size() ? std::get<1>(process_items_[sel]) : -1;
    LoadProcesses();
    PopulateFilter(1);
    for (size_t i = 0; i < process_items_.size(); ++i)
        if (std::get<1>(process_items_[i]) == pid) SendMessageW(cmb_, CB_SETCURSEL, i, 0);
}

void AddSourceDialog::OnFilterChange() {
    StopTest();
    PopulateFilter(CheckedFilter());
}

int AddSourceDialog::SelectedDevice() const {
    int sel = (int)SendMessageW(cmb_, CB_GETCURSEL, 0, 0);
    if (sel == CB_ERR || current_items_ != &device_items_) return -1;
    return std::get<1>((*current_items_)[sel]);
}

void AddSourceDialog::UpdateTestState() {
    // While recording, the test would compete with the recording for the device.
    EnableWindow(btn_test_, SelectedDevice() >= 0 && !audio_.is_recording());
}

void AddSourceDialog::OnTest() {
    if (testing_) {
        StopTest();
        return;
    }
    int dev_idx = SelectedDevice();
    if (dev_idx < 0) return;
    std::pair<int, int> fmt = output_format_ ? output_format_() : std::make_pair(44100, 2);
    int err = 0;
    if (!audio_.StartMonitor(dev_idx, Volume(), fmt.first, fmt.second, fx_, &err)) {
        ui::MsgBox(hwnd_,
                       Format(tr("Não foi possível testar este dispositivo (erro BASS {erro})."),
                              {{L"erro", std::to_wstring(err)}}),
                       tr("Erro"), MB_ICONERROR | MB_OK);
        return;
    }
    testing_ = true;
    ui::SetText(btn_test_, tr("&Parar"));
}

/* The test follows the dialogs of the equalizer and noise remover while they
   are open, and the stereo check box. */
void AddSourceDialog::Preview(const FxSettings& fx) {
    if (testing_) audio_.SetMonitorFx(fx);
}

void AddSourceDialog::OnEqualizer() {
    EqualizerDialog dlg(fx_, [this](const FxSettings& fx) { Preview(fx); });
    if (dlg.ShowModal(hwnd_) == IDOK) fx_ = dlg.result;
    Preview(fx_);
}

void AddSourceDialog::OnNoise() {
    NoiseDialog dlg(fx_, [this](const FxSettings& fx) { Preview(fx); });
    if (dlg.ShowModal(hwnd_) == IDOK) fx_ = dlg.result;
    Preview(fx_);
}

void AddSourceDialog::StopTest() {
    if (!testing_) return;
    audio_.StopMonitor();
    testing_ = false;
    ui::SetText(btn_test_, tr("&Testar"));
}

double AddSourceDialog::Volume() const {
    int pos = (int)SendMessageW(sld_, TBM_GETPOS, 0, 0);
    return std::pow(10.0, ((pos - 50) * VOLUME_STEP) / 20.0);
}

void AddSourceDialog::UpdateDbLabel() {
    int pos = (int)SendMessageW(sld_, TBM_GETPOS, 0, 0);
    double db = (pos - 50) * VOLUME_STEP;
    wchar_t text[32];
    swprintf(text, 32, L"  %+.1f dB", db);
    ui::SetText(lbl_db_, text);
    if (testing_) audio_.SetMonitorVolume(Volume());
}

LRESULT CALLBACK AddSourceDialog::SliderProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR,
                                             DWORD_PTR ref) {
    if (msg == WM_KEYDOWN) {
        AddSourceDialog* self = (AddSourceDialog*)ref;
        int val = (int)SendMessageW(hwnd, TBM_GETPOS, 0, 0);
        int new_val = -1;
        switch (wp) {
            case VK_HOME: new_val = 0; break;
            case VK_END: new_val = 100; break;
            case VK_UP:
            case VK_RIGHT: new_val = std::min(100, val + 1); break;
            case VK_DOWN:
            case VK_LEFT: new_val = std::max(0, val - 1); break;
            case VK_PRIOR: new_val = std::min(100, (val / 5 + 1) * 5); break;
            case VK_NEXT: new_val = std::max(0, (int)std::floor((val - 1) / 5.0) * 5); break;
        }
        if (new_val >= 0) {
            SendMessageW(hwnd, TBM_SETPOS, TRUE, new_val);
            self->UpdateDbLabel();
            // Tell the screen reader about the new position, as the trackbar does on its own.
            NotifyWinEvent(EVENT_OBJECT_VALUECHANGE, hwnd, OBJID_CLIENT, CHILDID_SELF);
            return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

void AddSourceDialog::OnOk() {
    StopTest();
    if (filter_ == 2) {
        // Only the file name counts, and ".exe" is added when missing.
        std::wstring name = BaseName(Trim(ui::GetText(txt_exe_)));
        if (name.empty()) return;
        if (name.size() < 4 || ToLower(name.substr(name.size() - 4)) != L".exe") name += L".exe";
        selected.type = L"exe";
        selected.id = 0;
        selected.name = name;
        selected.volume = Volume();
        selected.fx = fx_;
        selected.fx.noise.on = false;  // only for audio devices
        has_selected = true;
        EndModal(IDOK);
        return;
    }
    int sel = (int)SendMessageW(cmb_, CB_GETCURSEL, 0, 0);
    if (sel == CB_ERR || current_items_->empty()) {
        EndModal(IDCANCEL);
        return;
    }
    auto& item = (*current_items_)[sel];
    selected.type = std::get<0>(item);
    selected.id = std::get<1>(item);
    selected.name = std::get<2>(item);
    selected.volume = Volume();
    selected.fx = fx_;
    if (selected.type != L"device") selected.fx.noise.on = false;
    has_selected = true;
    EndModal(IDOK);
}

/* ---- Single choice ---- */

namespace {

enum { IDC_SC_LIST = 401 };

class SingleChoiceDialog : public ui::Dialog {
public:
    SingleChoiceDialog(const std::wstring& message, const std::wstring& title,
                       const std::vector<std::wstring>& items, bool on_top)
        : ui::Dialog(title), message_(message), items_(items), on_top_(on_top) {
        style_ |= WS_THICKFRAME;
    }
    int selection = -1;

protected:
    void OnInit() override {
        if (on_top_) SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        auto vbox = VBox();
        HWND msg = Add(WC_STATICW, message_, SS_NOPREFIX);
        list_ = Add(WC_LISTBOXW, L"", WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                    IDC_SC_LIST, WS_EX_CLIENTEDGE);
        for (auto& item : items_) SendMessageW(list_, LB_ADDSTRING, 0, (LPARAM)item.c_str());
        SendMessageW(list_, LB_SETCURSEL, 0, 0);
        vbox->Add(msg, 0, ui::ALL, S(10));
        vbox->Add(list_, 1, ui::LEFT | ui::RIGHT | ui::EXPAND, S(10), SIZE{S(300), S(150)});
        auto hb = HBox();
        HWND ok = Add(WC_BUTTONW, L"&OK", BUTTON | BS_DEFPUSHBUTTON, IDOK);
        HWND cancel = Add(WC_BUTTONW, L"&Cancel", BUTTON, IDCANCEL);
        hb->Add(ok, 0, ui::RIGHT, S(5));
        hb->Add(cancel, 0, 0, 0);
        vbox->Add(std::move(hb), 0, ui::ALIGN_RIGHT | ui::ALL, S(10));
        root_ = std::move(vbox);
        Fit();
    }
    bool OnCommand(int id, int code, HWND) override {
        if (id == IDOK || (id == IDC_SC_LIST && code == LBN_DBLCLK)) {
            selection = (int)SendMessageW(list_, LB_GETCURSEL, 0, 0);
            EndModal(IDOK);
            return true;
        }
        return false;
    }

private:
    std::wstring message_;
    std::vector<std::wstring> items_;
    bool on_top_;
    HWND list_ = nullptr;
};

}  // namespace

int SingleChoice(HWND parent, const std::wstring& message, const std::wstring& title,
                 const std::vector<std::wstring>& items, bool on_top) {
    SingleChoiceDialog dlg(message, title, items, on_top);
    if (dlg.ShowModal(parent) != IDOK) return -1;
    return dlg.selection;
}

bool PickFolder(HWND parent, const std::wstring& title, std::wstring* path) {
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dlg))))
        return false;
    DWORD options = 0;
    dlg->GetOptions(&options);
    dlg->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dlg->SetTitle(title.c_str());
    bool ok = false;
    if (SUCCEEDED(dlg->Show(parent))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            wchar_t* name = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &name))) {
                *path = name;
                CoTaskMemFree(name);
                ok = true;
            }
            item->Release();
        }
    }
    dlg->Release();
    return ok;
}

bool ConfirmYesNo(HWND parent, const std::wstring& message, const std::wstring& title) {
    return MessageBoxW(parent, message.c_str(), title.c_str(),
                       MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON1 | MB_SETFOREGROUND) == IDYES;
}

/* ---- Update download ---- */

namespace {
const UINT WM_DOWNLOAD_PROGRESS = WM_APP + 20;
const UINT WM_DOWNLOAD_DONE = WM_APP + 21;
}  // namespace

DownloadDialog::DownloadDialog(const UpdateInfo& info, const std::wstring& file)
    : ui::Dialog(tr("Baixando Atualização")), info_(info), file_(file) {}

DownloadDialog::~DownloadDialog() {
    if (thread_) {
        cancel_ = true;
        WaitForSingleObject(thread_, INFINITE);
        CloseHandle(thread_);
    }
}

void DownloadDialog::OnInit() {
    auto vbox = VBox();
    HWND text = Add(WC_STATICW, tr("Por favor aguarde..."), SS_NOPREFIX);
    bar_ = Add(PROGRESS_CLASSW, L"", 0);
    SendMessageW(bar_, PBM_SETRANGE32, 0, 1000);
    HWND cancel = Add(WC_BUTTONW, tr("&Cancelar"), BUTTON, IDCANCEL);
    vbox->Add(text, 0, ui::ALL, S(10));
    vbox->Add(bar_, 0, ui::LEFT | ui::RIGHT | ui::EXPAND, S(10), SIZE{S(300), S(18)});
    vbox->Add(cancel, 0, ui::CENTER | ui::ALL, S(15));
    root_ = std::move(vbox);
    Fit();
    SetFocus(cancel);
    thread_ = CreateThread(nullptr, 0, Worker, this, 0, nullptr);
}

DWORD WINAPI DownloadDialog::Worker(void* param) {
    DownloadDialog* self = static_cast<DownloadDialog*>(param);
    HWND hwnd = self->hwnd_;
    int last = -1;
    bool ok = DownloadUpdate(self->info_, self->file_, self->cancel_,
                             [&](unsigned long long done, unsigned long long total) {
                                 int permille = total ? (int)(done * 1000 / total) : 0;
                                 if (permille != last) {
                                     last = permille;
                                     PostMessageW(hwnd, WM_DOWNLOAD_PROGRESS, permille, 0);
                                 }
                             });
    PostMessageW(hwnd, WM_DOWNLOAD_DONE, ok, 0);
    return 0;
}

INT_PTR DownloadDialog::OnMessage(UINT msg, WPARAM wp, LPARAM, bool* handled) {
    *handled = false;
    if (msg == WM_DOWNLOAD_PROGRESS) {
        SendMessageW(bar_, PBM_SETPOS, wp, 0);
        *handled = true;
    } else if (msg == WM_DOWNLOAD_DONE) {
        ok = wp != 0;
        *handled = true;
        EndModal(ok ? IDOK : IDCANCEL);
    }
    return 0;
}

bool DownloadDialog::OnCommand(int id, int code, HWND) {
    if (id == IDCANCEL) {
        // The download stops at its next piece; the dialog closes right away.
        cancelled = true;
        cancel_ = true;
        EndModal(IDCANCEL);
        return true;
    }
    return id == IDOK;  // Enter outside the button does nothing
}
