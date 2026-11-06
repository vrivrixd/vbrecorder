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

#include "background.h"
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

/* A spin box: an edit field with its arrows, which change the number by
   one; it can still be typed. The arrow keys work in the field. */
class SpinItem : public ui::Item {
public:
    SpinItem(HWND edit, HWND arrows) : edit_(edit), arrows_(arrows) {}
    SIZE Min() override { return SIZE{S(90), ui::BestSize(edit_).cy}; }
    bool Shown() override { return ui::IsShownStyle(edit_); }
    void Place(int x, int y, int w, int h) override {
        int aw = GetSystemMetrics(SM_CXVSCROLL);
        SetWindowPos(edit_, nullptr, x, y, w - aw, h, SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(arrows_, nullptr, x + w - aw, y, aw, h, SWP_NOZORDER | SWP_NOACTIVATE);
    }

private:
    HWND edit_, arrows_;
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
// The volume slider goes from -30 to +30 dB in steps of 0.5 dB, as the
// gains of the equalizer: the arrows move 0.5 dB, Page Up and Down 3 dB.
const double VOLUME_DB = 30.0;
const double VOLUME_STEP = 0.5;
const int VOLUME_CENTER = (int)(VOLUME_DB / VOLUME_STEP);  // 0 dB
const int VOLUME_MAX = 2 * VOLUME_CENTER;
const int VOLUME_PAGE = 6;
// The balance: 0 to 100, 50 in the center; Page Up and Down move 5.
const int PAN_MAX = 100;
const int PAN_PAGE = 5;

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
        {L"renomear", tr("Renomear Gravação:")},
        {L"excluir", tr("Excluir Gravação:")},
        {L"abrir_pasta", tr("Abrir Pasta de Gravações:")},
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
    IDC_ST_MULTIPLE,
    IDC_ST_BUFFER,
    IDC_ST_KEYS,
    IDC_ST_UPDATES,
    IDC_ST_CHECK,
    IDC_ST_STARTUP,
    IDC_ST_COUNTDOWN,
    IDC_ST_SPLIT,
    IDC_ST_PREROLL,
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
    btn_check_ = Add(WC_BUTTONW, tr("Verificar &Atualização..."), BUTTON, IDC_ST_CHECK);
    vbox->Add(btn_check_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    chk_startup_ = Add(WC_BUTTONW, tr("Iniciar com o &Windows"), CHECK, IDC_ST_STARTUP);
    Button_SetCheck(chk_startup_, config_.GetBoolean(L"iniciar_com_windows"));
    vbox->Add(chk_startup_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    chk_start_min_ = Add(WC_BUTTONW, tr("Iniciar &Minimizado"), CHECK, IDC_ST_START_MIN);
    Button_SetCheck(chk_start_min_, config_.GetBoolean(L"iniciar_minimizado"));
    vbox->Add(chk_start_min_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    chk_tray_ = Add(WC_BUTTONW, tr("Minimizar &Para a Bandeja do Sistema"), CHECK, IDC_ST_TRAY);
    Button_SetCheck(chk_tray_, config_.GetBoolean(L"minimizar_bandeja"));
    vbox->Add(chk_tray_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    chk_on_top_ = Add(WC_BUTTONW, tr("Janela Sempre no &Topo"), CHECK, IDC_ST_ON_TOP);
    Button_SetCheck(chk_on_top_, config_.GetBoolean(L"sempre_no_topo", true));
    vbox->Add(chk_on_top_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    chk_multiple_ = Add(WC_BUTTONW, tr("Permitir Múltiplas I&nstâncias"), CHECK, IDC_ST_MULTIPLE);
    Button_SetCheck(chk_multiple_, config_.GetBoolean(L"multiplas_instancias"));
    vbox->Add(chk_multiple_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    chk_countdown_ = Add(WC_BUTTONW, tr("Contagem &Regressiva Antes de Gravar"), CHECK,
                         IDC_ST_COUNTDOWN);
    Button_SetCheck(chk_countdown_, config_.GetBoolean(L"contagem_regressiva"));
    vbox->Add(chk_countdown_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    // A spin box, as the buffer: an edit field with its arrows.
    auto spin = [&](ui::Box* row, HWND label, HWND* edit, int id, int value, int min, int max) {
        *edit = Add(WC_EDITW, std::to_wstring(value), EDIT, id, WS_EX_CLIENTEDGE);
        HWND arrows = Add(UPDOWN_CLASSW, L"", UDS_SETBUDDYINT | UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0);
        SendMessageW(arrows, UDM_SETBUDDY, (WPARAM)*edit, 0);
        SendMessageW(arrows, UDM_SETRANGE32, min, max);
        SendMessageW(arrows, UDM_SETPOS32, 0, value);
        row->Add(label, 0, ui::CENTER_V | ui::RIGHT, S(5));
        row->Add(std::unique_ptr<ui::Item>(new SpinItem(*edit, arrows)), 0, ui::CENTER_V, 0);
    };

    // The seconds before Record that are recorded too; 0 is off.
    auto hb_pre = HBox();
    HWND lbl_pre = Add(WC_STATICW, tr("Pré-Gravação (&segundos):"), 0);
    spin(hb_pre.get(), lbl_pre, &txt_preroll_, IDC_ST_PREROLL, config_.GetPreroll(), 0, PREROLL_MAX);
    ui::SetToolTip(hwnd_, txt_preroll_,
                   Format(tr("Com o programa aberto, as fontes ficam sendo captadas e os últimos segundos são "
                             "guardados: a gravação começa esse tanto antes de você clicar em Gravar. O áudio de "
                             "fundo começa ao clicar. 0 desativa; até {maximo}."),
                          {{L"maximo", std::to_wstring(PREROLL_MAX)}}));
    vbox->Add(std::move(hb_pre), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    auto hb_split = HBox();
    // As the recording duration: 30:00 is half an hour, a single number is seconds.
    HWND lbl_split = Add(WC_STATICW, tr("Dividir &Gravação a Cada:"), 0);
    txt_split_ = Add(WC_EDITW, config_.Get(L"dividir"), EDIT, IDC_ST_SPLIT, WS_EX_CLIENTEDGE);
    ui::SetToolTip(hwnd_, txt_split_,
                   tr("A cada tanto tempo, a gravação continua em um novo arquivo, numerado: nome 001, nome 002... "
                      "30:00 são 30 minutos, 1:00:00 é uma hora. Em branco, não divide."));
    hb_split->Add(lbl_split, 0, ui::CENTER_V | ui::RIGHT, S(5));
    hb_split->Add(txt_split_, 0, ui::CENTER_V, 0, SIZE{S(80), 0});
    vbox->Add(std::move(hb_split), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    auto hb_buf = HBox();
    HWND lbl_buf = Add(WC_STATICW, tr("Tamanho do &Buffer (ms):"), 0);
    spin(hb_buf.get(), lbl_buf, &txt_buffer_, IDC_ST_BUFFER, config_.GetBuffer(), BUFFER_MIN, BUFFER_MAX);
    // The explanation goes in the field's tooltip, not in loose text in the
    // dialog: screen readers read loose text first, when the dialog opens.
    ui::SetToolTip(hwnd_, txt_buffer_,
                   Format(tr("Aumente o buffer se o áudio estalar em um computador ocupado. "
                             "De {minimo} a {maximo} ms; o padrão é {padrao}."),
                          {{L"minimo", std::to_wstring(BUFFER_MIN)},
                           {L"maximo", std::to_wstring(BUFFER_MAX)},
                           {L"padrao", std::to_wstring(BUFFER_DEFAULT)}}));
    vbox->Add(std::move(hb_buf), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));


    btn_keys_ = Add(WC_BUTTONW, tr("Configurar Teclas &de Atalho..."), BUTTON, IDC_ST_KEYS);
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
    {
        split_text = Trim(ui::GetText(txt_split_));
        if (SplitSeconds(split_text) < 0) {
            ui::MsgBox(hwnd_,
                       tr("Tempo de divisão inválido. Use horas, minutos e segundos, como 30:00 ou 1:00:00, ou "
                          "deixe em branco para não dividir."),
                       tr("Aviso"), MB_ICONWARNING | MB_OK);
            SetFocus(txt_split_);
            SendMessageW(txt_split_, EM_SETSEL, 0, -1);
            return;
        }
    }
    {
        std::wstring text = Trim(ui::GetText(txt_preroll_));
        int value = text.empty() || text.size() > 3 || text.find_first_not_of(L"0123456789") != std::wstring::npos
                        ? -1
                        : _wtoi(text.c_str());
        if (value < 0 || value > PREROLL_MAX) {
            ui::MsgBox(hwnd_,
                       Format(tr("A pré-gravação deve ser um número de 0 a {maximo} segundos."),
                              {{L"maximo", std::to_wstring(PREROLL_MAX)}}),
                       tr("Aviso"), MB_ICONWARNING | MB_OK);
            SetFocus(txt_preroll_);
            SendMessageW(txt_preroll_, EM_SETSEL, 0, -1);
            return;
        }
        preroll = value;
    }
    int sel = (int)SendMessageW(cmb_lang_, CB_GETCURSEL, 0, 0);
    language_ = (sel >= 0 && sel < (int)codes_.size()) ? codes_[sel] : L"";
    start_minimized = Button_GetCheck(chk_start_min_) == BST_CHECKED;
    check_updates = Button_GetCheck(chk_updates_) == BST_CHECKED;
    start_with_windows = Button_GetCheck(chk_startup_) == BST_CHECKED;
    countdown = Button_GetCheck(chk_countdown_) == BST_CHECKED;
    tray = Button_GetCheck(chk_tray_) == BST_CHECKED;
    on_top = Button_GetCheck(chk_on_top_) == BST_CHECKED;
    multiple = Button_GetCheck(chk_multiple_) == BST_CHECKED;
    EndModal(IDOK);
}

/* ---- Renaming the last recording ---- */

enum { IDC_RN_FOLDER = 1401, IDC_RN_FILE = 1402 };  // + file

RenameDialog::RenameDialog(const std::wstring& folder, const std::vector<std::wstring>& names)
    : ui::Dialog(tr("Renomear Gravação")), folder_(folder), names_(names) {}

void RenameDialog::OnInit() {
    auto vbox = VBox();
    auto field = [&](const std::wstring& label, const std::wstring& value, int id) {
        HWND lbl = Add(WC_STATICW, label, 0);
        HWND edit = Add(WC_EDITW, value, EDIT, id, WS_EX_CLIENTEDGE);
        auto row = HBox();
        row->Add(lbl, 0, ui::CENTER_V | ui::RIGHT, S(5), SIZE{S(110), 0});
        row->Add(edit, 1, ui::EXPAND, 0, SIZE{S(320), 0});
        vbox->Add(std::move(row), 0, ui::LEFT | ui::RIGHT | ui::TOP | ui::EXPAND, S(10));
        return edit;
    };
    if (!folder_.empty()) folder_edit_ = field(tr("&Pasta:"), folder_, IDC_RN_FOLDER);
    for (size_t i = 0; i < names_.size(); ++i) {
        // The same letter for all the files: it goes from one to the next.
        std::wstring label = names_.size() == 1
                                 ? tr("&Nome do Arquivo:")
                                 : Format(tr("&Arquivo {n}:"), {{L"n", std::to_wstring(i + 1)}});
        edits_.push_back(field(label, names_[i], IDC_RN_FILE + (int)i));
    }
    auto hb = HBox();
    HWND ok = Add(WC_BUTTONW, tr("&Ok"), BUTTON | BS_DEFPUSHBUTTON, IDOK);
    HWND cancel = Add(WC_BUTTONW, tr("&Cancelar"), BUTTON, IDCANCEL);
    hb->Add(ok, 0, ui::RIGHT, S(10));
    hb->Add(cancel, 0, 0, 0);
    vbox->Add(std::move(hb), 0, ui::CENTER | ui::ALL, S(15));
    root_ = std::move(vbox);
    Fit();
    // The name selected, ready to be typed over.
    HWND first = folder_edit_ ? folder_edit_ : (edits_.empty() ? nullptr : edits_[0]);
    if (first) {
        SetFocus(first);
        SendMessageW(first, EM_SETSEL, 0, -1);
    }
}

bool RenameDialog::OnCommand(int id, int, HWND) {
    if (id != IDOK) return false;
    // A name Windows accepts: not empty, without \ / : * ? " < > |, and not
    // ending in a dot or a space, which Windows would drop.
    auto valid = [](const std::wstring& name) {
        if (name.empty() || name.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos) return false;
        return name.back() != L'.' && name.back() != L' ';
    };
    std::vector<HWND> all = edits_;
    if (folder_edit_) all.insert(all.begin(), folder_edit_);
    for (HWND edit : all) {
        if (valid(Trim(ui::GetText(edit)))) continue;
        ui::MsgBox(hwnd_,
                   tr("O nome não pode ficar vazio, terminar em ponto ou espaço, nem ter os caracteres "
                      "\\ / : * ? \" < > |"),
                   tr("Aviso"), MB_ICONWARNING | MB_OK);
        SetFocus(edit);
        SendMessageW(edit, EM_SETSEL, 0, -1);
        return true;
    }
    folder_result = folder_edit_ ? Trim(ui::GetText(folder_edit_)) : L"";
    result.clear();
    for (HWND edit : edits_) result.push_back(Trim(ui::GetText(edit)));
    EndModal(IDOK);
    return true;
}

/* ---- Tags ---- */

namespace {
enum { IDC_TAG_EDIT = 1101, IDC_TAG_VARS = 1201, IDC_TAG_MENU = 1301 };  // + field, + variable
}  // namespace

TagsDialog::TagsDialog(const TagValues& tags) : ui::Dialog(tr("Tags")), result(tags), tags_(tags) {}

void TagsDialog::OnInit() {
    // In the order of TagField; the comment last.
    const char* labels[TAG_COUNT] = {"&Título:", "&Artistas Participantes:", "Artista &do Álbum:", "Á&lbum:",
                                     "A&no:",    "&Gênero:",                "&Faixa:",           "Co&mentário:"};
    auto vbox = VBox();
    for (int f = 0; f < TAG_COUNT; ++f) {
        HWND label = Add(WC_STATICW, tr(labels[f]), 0);
        edit_[f] = Add(WC_EDITW, tags_.value[f], EDIT, IDC_TAG_EDIT + f, WS_EX_CLIENTEDGE);
        // The same letter for all of them: it goes from one to the next.
        vars_[f] = Add(WC_BUTTONW, tr("&Variáveis..."), BUTTON, IDC_TAG_VARS + f);
        auto row = HBox();
        row->Add(label, 0, ui::CENTER_V | ui::RIGHT, S(5), SIZE{S(110), 0});
        row->Add(edit_[f], 1, ui::EXPAND | ui::RIGHT, S(5), SIZE{S(280), 0});
        row->Add(vars_[f], 0, 0, 0);
        vbox->Add(std::move(row), 0, ui::LEFT | ui::RIGHT | ui::TOP | ui::EXPAND, S(10));
    }
    auto hb = HBox();
    btn_ok_ = Add(WC_BUTTONW, tr("&Ok"), BUTTON | BS_DEFPUSHBUTTON, IDOK);
    btn_cancel_ = Add(WC_BUTTONW, tr("&Cancelar"), BUTTON, IDCANCEL);
    hb->Add(btn_ok_, 0, ui::RIGHT, S(10));
    hb->Add(btn_cancel_, 0, 0, 0);
    vbox->Add(std::move(hb), 0, ui::CENTER | ui::ALL, S(15));
    root_ = std::move(vbox);
    Fit();
}

/* A menu of the variables, below the button; the one chosen goes into the
   field, where its cursor was, and the focus goes back there. */
void TagsDialog::ShowVariables(int field) {
    HWND edit = edit_[field];
    DWORD start = 0, end = 0;
    SendMessageW(edit, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    HMENU menu = CreatePopupMenu();
    const auto& vars = TagVariables();
    for (size_t i = 0; i < vars.size(); ++i) {
        std::wstring item = tr(vars[i].label) + L" (" + vars[i].code + L")";
        AppendMenuW(menu, MF_STRING, IDC_TAG_MENU + i, item.c_str());
    }
    RECT r;
    GetWindowRect(vars_[field], &r);
    int cmd = (int)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_NONOTIFY, r.left,
                                  r.bottom, 0, hwnd_, nullptr);
    DestroyMenu(menu);
    SetFocus(edit);
    SendMessageW(edit, EM_SETSEL, start, end);
    int i = cmd - IDC_TAG_MENU;
    if (i >= 0 && i < (int)vars.size()) SendMessageW(edit, EM_REPLACESEL, TRUE, (LPARAM)vars[i].code);
}

bool TagsDialog::OnCommand(int id, int code, HWND) {
    int f = id - IDC_TAG_VARS;
    if (f >= 0 && f < TAG_COUNT && code == BN_CLICKED) {
        ShowVariables(f);
        return true;
    }
    if (id == IDOK) {
        for (int i = 0; i < TAG_COUNT; ++i) result.value[i] = Trim(ui::GetText(edit_[i]));
        EndModal(IDOK);
        return true;
    }
    return false;
}

bool TagsDialog::OnCharHook(const MSG& msg) {
    // Enter in a field confirms; on a button it does what the button does.
    HWND focus = GetFocus();
    for (int f = 0; f < TAG_COUNT; ++f)
        if (IsEnter(msg) && focus == edit_[f]) {
            OnCommand(IDOK, BN_CLICKED, nullptr);
            return true;
        }
    return false;
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
    IDC_AS_EXE_BROWSE,
    IDC_AS_REFRESH,
    IDC_AS_ALL_PROCESSES,
    IDC_AS_STEREO,
    IDC_AS_EQ,
    IDC_AS_FX,
    IDC_AS_NOISE,
    IDC_AS_FILE,
    IDC_AS_FILE_PATH,
    IDC_AS_FILE_BROWSE,
    IDC_AS_FOLDER_BROWSE,
    IDC_AS_LOOP,
    IDC_AS_SHUFFLE,
    IDC_AS_SUBFOLDERS,
    IDC_AS_LISTEN,
    IDC_AS_SIDECHAIN,
    IDC_AS_PAN,
    IDC_AS_URL,
    IDC_AS_URL_TEXT,
    IDC_AS_URL_BROWSE,
    IDC_AS_PICK,
};

/* The test of a stream URL connected (wParam 0) or failed (the BASS error). */
const UINT WM_URL_TEST = WM_APP + 20;

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
    rb_file_ = Add(WC_BUTTONW, tr("Áudio de Fundo"), BS_AUTORADIOBUTTON, IDC_AS_FILE);
    rb_url_ = Add(WC_BUTTONW, tr("URL de Stream"), BS_AUTORADIOBUTTON, IDC_AS_URL);
    Button_SetCheck(rb_devices_, BST_CHECKED);
    if (!available) {
        EnableWindow(rb_processes_, FALSE);
        EnableWindow(rb_exe_, FALSE);
    }
    vbox->Add(std::unique_ptr<ui::Item>(
                  new RadioBoxItem(grp_, {rb_devices_, rb_processes_, rb_exe_, rb_file_, rb_url_})),
              0, ui::ALL | ui::EXPAND, S(10));

    lbl_src_ = Add(WC_STATICW, tr("Selecionar Fonte:"), WS_GROUP);
    cmb_ = Add(WC_COMBOBOXW, L"", COMBO, IDC_AS_SOURCE);
    vbox->Add(lbl_src_, 0, ui::LEFT | ui::RIGHT, S(10));
    vbox->Add(cmb_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM | ui::EXPAND, S(10));
    // Right after the list of processes, which they change: the controls
    // after them belong to every kind of source.
    chk_all_ = Add(WC_BUTTONW, tr("&Mostrar Todos os Processos"), CHECK, IDC_AS_ALL_PROCESSES);
    vbox->Add(chk_all_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));
    // Reads the processes again, for programs opened after the dialog.
    btn_refresh_ = Add(WC_BUTTONW, tr("&Atualizar"), BUTTON, IDC_AS_REFRESH);
    vbox->Add(btn_refresh_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));
    // An executable is typed by name: it does not need to be running.
    lbl_exe_ = ui::Create(hwnd_, WC_STATICW, tr("Nome do Executável:"), 0, 0);
    txt_exe_ = ui::Create(hwnd_, WC_EDITW, L"", WS_TABSTOP | ES_AUTOHSCROLL, IDC_AS_EXE_NAME,
                          WS_EX_CLIENTEDGE);
    // Finds the program on disk; only its file name goes into the field.
    btn_exe_browse_ = ui::Create(hwnd_, WC_BUTTONW, tr("P&rocurar..."), BUTTON, IDC_AS_EXE_BROWSE);
    vbox->Add(lbl_exe_, 0, ui::LEFT | ui::RIGHT, S(10));
    {
        auto row = HBox();
        row->Add(txt_exe_, 1, ui::EXPAND | ui::RIGHT, S(5));
        row->Add(btn_exe_browse_, 0, 0, 0);
        vbox->Add(std::move(row), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM | ui::EXPAND, S(10));
    }
    // Background audio: a file, or a folder whose audio files play in name
    // order. It is typed or found with the buttons.
    lbl_file_ = ui::Create(hwnd_, WC_STATICW, tr("Arquivo ou Pasta:"), 0, 0);
    txt_file_ = ui::Create(hwnd_, WC_EDITW, L"", WS_TABSTOP | ES_AUTOHSCROLL, IDC_AS_FILE_PATH,
                           WS_EX_CLIENTEDGE);
    btn_file_browse_ = ui::Create(hwnd_, WC_BUTTONW, tr("Procurar Arq&uivo..."), BUTTON, IDC_AS_FILE_BROWSE);
    btn_folder_browse_ =
        ui::Create(hwnd_, WC_BUTTONW, tr("Procurar P&asta..."), BUTTON, IDC_AS_FOLDER_BROWSE);
    vbox->Add(lbl_file_, 0, ui::LEFT | ui::RIGHT, S(10));
    {
        auto row = HBox();
        row->Add(txt_file_, 1, ui::EXPAND | ui::RIGHT, S(5));
        row->Add(btn_file_browse_, 0, ui::RIGHT, S(5));
        row->Add(btn_folder_browse_, 0, 0, 0);
        vbox->Add(std::move(row), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM | ui::EXPAND, S(10));
    }
    // Unchecked, it plays once and then the source is silent.
    chk_loop_ = ui::Create(hwnd_, WC_BUTTONW, tr("Repet&ir ao Terminar"), CHECK, IDC_AS_LOOP);
    Button_SetCheck(chk_loop_, !editing_ || edit_source_.loop ? BST_CHECKED : BST_UNCHECKED);
    vbox->Add(chk_loop_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));
    // A folder in a random order instead of by name; a single file has no order.
    chk_shuffle_ = ui::Create(hwnd_, WC_BUTTONW, tr("A&leatório"), CHECK, IDC_AS_SHUFFLE);
    Button_SetCheck(chk_shuffle_, editing_ && edit_source_.shuffle ? BST_CHECKED : BST_UNCHECKED);
    vbox->Add(chk_shuffle_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));
    // The audio of the folders inside the folder, and of the ones inside those.
    chk_subfolders_ = ui::Create(hwnd_, WC_BUTTONW, tr("Incluir Su&bpastas"), CHECK, IDC_AS_SUBFOLDERS);
    Button_SetCheck(chk_subfolders_, editing_ && edit_source_.subfolders ? BST_CHECKED : BST_UNCHECKED);
    vbox->Add(chk_subfolders_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));
    // A stream URL: an internet radio, or a playlist on the computer.
    lbl_url_ = ui::Create(hwnd_, WC_STATICW, tr("&Link do Stream:"), 0, 0);
    txt_url_ = ui::Create(hwnd_, WC_EDITW, L"", WS_TABSTOP | ES_AUTOHSCROLL, IDC_AS_URL_TEXT, WS_EX_CLIENTEDGE);
    ui::SetToolTip(hwnd_, txt_url_, tr("O endereço de uma rádio da internet, ou de uma lista M3U, PLS ou M3U8, na "
                                       "internet ou no computador."));
    btn_url_browse_ = ui::Create(hwnd_, WC_BUTTONW, tr("Procurar Arq&uivo..."), BUTTON, IDC_AS_URL_BROWSE);
    vbox->Add(lbl_url_, 0, ui::LEFT | ui::RIGHT, S(10));
    {
        auto row = HBox();
        row->Add(txt_url_, 1, ui::EXPAND | ui::RIGHT, S(5));
        row->Add(btn_url_browse_, 0, 0, 0);
        vbox->Add(std::move(row), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM | ui::EXPAND, S(10));
    }
    // Background audio and stream URLs: also heard while recording, not only
    // in the file. Off by default.
    chk_listen_ = ui::Create(hwnd_, WC_BUTTONW, tr("Re&produzir Enquanto Grava"), CHECK, IDC_AS_LISTEN);
    Button_SetCheck(chk_listen_, editing_ && edit_source_.listen ? BST_CHECKED : BST_UNCHECKED);
    vbox->Add(chk_listen_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));
    // Which channels of the source are recorded, before Stereo, which
    // still decides whether they stay on their side or go to both.
    lbl_pick_ = Add(WC_STATICW, tr("Canais a &Gravar:"), 0);
    cmb_pick_ = Add(WC_COMBOBOXW, L"", COMBO, IDC_AS_PICK);
    ui::ComboSetItems(cmb_pick_, {tr("Ambos"), tr("Somente o Esquerdo"), tr("Somente o Direito")});
    SendMessageW(cmb_pick_, CB_SETCURSEL, fx_.channel, 0);
    {
        auto row = HBox();
        row->Add(lbl_pick_, 0, ui::CENTER_V | ui::RIGHT, S(5));
        row->Add(cmb_pick_, 1, ui::EXPAND, 0);
        vbox->Add(std::move(row), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM | ui::EXPAND, S(10));
    }
    // Unchecked, this source is recorded in mono, whatever the recording is.
    chk_stereo_ = Add(WC_BUTTONW, tr("E&stéreo"), CHECK, IDC_AS_STEREO);
    Button_SetCheck(chk_stereo_, fx_.stereo ? BST_CHECKED : BST_UNCHECKED);
    vbox->Add(chk_stereo_, 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    auto vol_box = HBox();
    HWND lbl_vol = Add(WC_STATICW, tr("&Volume:"), 0);
    sld_ = Add(TRACKBAR_CLASSW, L"", WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, IDC_AS_VOLUME);
    SendMessageW(sld_, TBM_SETRANGE, TRUE, MAKELPARAM(0, VOLUME_MAX));
    SendMessageW(sld_, TBM_SETPAGESIZE, 0, VOLUME_PAGE);
    SendMessageW(sld_, TBM_SETPOS, TRUE, VOLUME_CENTER);
    SetWindowSubclass(sld_, SliderProc, 1, (DWORD_PTR)this);
    // The sliders say their values; written next to them, a screen reader
    // would read them all as the window opens.
    vol_box->Add(lbl_vol, 0, ui::CENTER_V | ui::RIGHT, S(5));
    vol_box->Add(sld_, 1, ui::EXPAND, 0);
    vbox->Add(std::move(vol_box), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM | ui::EXPAND, S(10));

    // The balance: 50 is the center, 0 all to the left, 100 all to the right.
    auto pan_box = HBox();
    HWND lbl_pan = Add(WC_STATICW, tr("Bala&nço:"), 0);
    sld_pan_ = Add(TRACKBAR_CLASSW, L"", WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, IDC_AS_PAN);
    SendMessageW(sld_pan_, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
    SendMessageW(sld_pan_, TBM_SETPAGESIZE, 0, 10);
    SendMessageW(sld_pan_, TBM_SETPOS, TRUE, 50);
    SetWindowSubclass(sld_pan_, SliderProc, 1, (DWORD_PTR)this);
    pan_box->Add(lbl_pan, 0, ui::CENTER_V | ui::RIGHT, S(5));
    pan_box->Add(sld_pan_, 1, ui::EXPAND, 0);
    vbox->Add(std::move(pan_box), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM | ui::EXPAND, S(10));

    btn_eq_ = Add(WC_BUTTONW, tr("&Equalizador"), BUTTON, IDC_AS_EQ);
    btn_fx_ = Add(WC_BUTTONW, tr("E&feitos"), BUTTON, IDC_AS_FX);
    // Only audio devices: a program's sound is already clean.
    btn_noise_ = Add(WC_BUTTONW, tr("Removedor de &Ruído"), BUTTON, IDC_AS_NOISE);
    // How this source and the others turn each other down, in the recording.
    btn_sidechain_ = Add(WC_BUTTONW, tr("Ca&deia Lateral"), BUTTON, IDC_AS_SIDECHAIN);
    // Last: it plays what the controls above set.
    btn_test_ = Add(WC_BUTTONW, tr("&Testar"), BUTTON, IDC_AS_TEST);
    ui::SetToolTip(hwnd_, btn_test_, tr("Ouvir o dispositivo selecionado com o volume ajustado"));
    auto tools = HBox();
    tools->Add(btn_eq_, 0, ui::RIGHT, S(5));
    tools->Add(btn_fx_, 0, ui::RIGHT, S(5));
    tools->Add(btn_noise_, 0, ui::RIGHT, S(5));
    tools->Add(btn_sidechain_, 0, ui::RIGHT, S(5));
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
    } else if (editing_ && edit_source_.type == L"url") {
        Button_SetCheck(rb_devices_, BST_UNCHECKED);
        Button_SetCheck(rb_url_, BST_CHECKED);
        PopulateFilter(4);
        SetWindowTextW(txt_url_, edit_source_.name.c_str());
    } else if (editing_ && edit_source_.type == L"file") {
        Button_SetCheck(rb_devices_, BST_UNCHECKED);
        Button_SetCheck(rb_file_, BST_CHECKED);
        PopulateFilter(3);
        SetWindowTextW(txt_file_, edit_source_.name.c_str());
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
        int slider_val = std::min(VOLUME_MAX, std::max(0, (int)std::nearbyint(db / VOLUME_STEP + VOLUME_CENTER)));
        SendMessageW(sld_, TBM_SETPOS, TRUE, slider_val);
        int pan_val = (int)std::lround(std::max(-1.0, std::min(1.0, edit_source_.pan)) * 50 + 50);
        SendMessageW(sld_pan_, TBM_SETPOS, TRUE, pan_val);
    }
    UpdateDbLabel();
    UpdatePanLabel();
    Fit();
}

bool AddSourceDialog::OnCharHook(const MSG& msg) {
    HWND focus = GetFocus();
    if (IsEnter(msg) && focus != btn_test_ && focus != btn_refresh_ && focus != btn_eq_ && focus != btn_fx_ &&
        focus != btn_noise_ && focus != btn_sidechain_ && focus != btn_exe_browse_ && focus != btn_file_browse_ &&
        focus != btn_folder_browse_ && focus != btn_url_browse_ && focus != btn_cancel_) {
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
    if (msg == WM_URL_TEST) {
        *handled = true;
        return OnUrlTestDone((int)wp);
    }
    if (msg == WM_HSCROLL && (HWND)lp == sld_pan_) {
        UpdatePanLabel();
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
        case IDC_AS_FILE:
        case IDC_AS_URL:
            if (code == BN_CLICKED && CheckedFilter() != filter_) OnFilterChange();
            return true;
        case IDC_AS_FILE_PATH:
            if (code == EN_CHANGE) {
                UpdateOkState();
                UpdateFolderOptions();
            }
            return true;
        case IDC_AS_FILE_BROWSE:
        case IDC_AS_FOLDER_BROWSE:
            if (code == BN_CLICKED) OnBrowseBackground(id == IDC_AS_FOLDER_BROWSE);
            return true;
        case IDC_AS_EXE_NAME:
            if (code == EN_CHANGE) UpdateOkState();
            return true;
        case IDC_AS_URL_TEXT:
            if (code == EN_CHANGE) {
                StopTest();
                UpdateOkState();
                UpdateTestState();
            }
            return true;
        case IDC_AS_URL_BROWSE:
            if (code == BN_CLICKED) OnBrowsePlaylist();
            return true;
        case IDC_AS_PICK:
            if (code == CBN_SELCHANGE) {
                int sel = (int)SendMessageW(cmb_pick_, CB_GETCURSEL, 0, 0);
                fx_.channel = sel >= 0 && sel <= CHANNEL_RIGHT ? sel : CHANNEL_BOTH;
                Preview(fx_);
            }
            return true;
        case IDC_AS_EXE_BROWSE:
            if (code == BN_CLICKED) OnBrowseExe();
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
        case IDC_AS_FX:
            if (code == BN_CLICKED) OnEffects();
            return true;
        case IDC_AS_NOISE:
            if (code == BN_CLICKED) OnNoise();
            return true;
        case IDC_AS_SIDECHAIN:
            if (code == BN_CLICKED) OnSidechain();
            return true;
        case IDOK:
            OnOk();
            return true;
    }
    return false;
}

int AddSourceDialog::CheckedFilter() const {
    if (Button_GetCheck(rb_url_) == BST_CHECKED) return 4;
    if (Button_GetCheck(rb_file_) == BST_CHECKED) return 3;
    if (Button_GetCheck(rb_exe_) == BST_CHECKED) return 2;
    return Button_GetCheck(rb_processes_) == BST_CHECKED ? 1 : 0;
}

void AddSourceDialog::UpdateOkState() {
    if (!current_items_) return;  // the dialog is still being built
    bool ok = filter_ == 2   ? !Trim(ui::GetText(txt_exe_)).empty()
              : filter_ == 3 ? !BackgroundPath().empty()
              : filter_ == 4 ? !StreamUrl().empty()
                             : !current_items_->empty();
    EnableWindow(btn_ok_, ok);
}

void AddSourceDialog::PopulateFilter(int sel) {
    filter_ = sel;
    bool exe = sel == 2, file = sel == 3, url = sel == 4;
    ui::Show(lbl_src_, !exe && !file && !url);
    ui::Show(cmb_, !exe && !file && !url);
    ui::Show(chk_all_, sel == 1);
    ui::Show(btn_refresh_, sel == 1);
    ui::Show(btn_noise_, sel == 0);
    // Background audio has no test: it is only heard in the recording.
    ui::Show(btn_test_, !file);
    ui::Show(lbl_exe_, exe);
    ui::Show(txt_exe_, exe);
    ui::Show(btn_exe_browse_, exe);
    ui::Show(lbl_file_, file);
    ui::Show(txt_file_, file);
    ui::Show(btn_file_browse_, file);
    ui::Show(btn_folder_browse_, file);
    ui::Show(chk_loop_, file);
    ui::Show(chk_shuffle_, file);
    ui::Show(chk_subfolders_, file);
    ui::Show(lbl_url_, url);
    ui::Show(txt_url_, url);
    ui::Show(btn_url_browse_, url);
    ui::Show(chk_listen_, file || url);
    UpdateFolderOptions();
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

void AddSourceDialog::OnBrowseExe() {
    std::wstring path;
    if (!PickFile(hwnd_, tr("Arquivo Executável"), tr("Arquivo Executável"), L"*.exe", &path))
        return;
    SetWindowTextW(txt_exe_, BaseName(path).c_str());
    SetFocus(txt_exe_);
}

/* The path typed, without the quotes Explorer's "Copy as path" adds. */
std::wstring AddSourceDialog::BackgroundPath() const {
    std::wstring path = Trim(ui::GetText(txt_file_));
    if (path.size() >= 2 && path.front() == L'"' && path.back() == L'"')
        path = Trim(path.substr(1, path.size() - 2));
    return path;
}

/* Shuffle and subfolders are for folders only: with a file, they are there
   but disabled. */
void AddSourceDialog::UpdateFolderOptions() {
    DWORD attr = GetFileAttributesW(BackgroundPath().c_str());
    bool folder = attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
    EnableWindow(chk_shuffle_, folder);
    EnableWindow(chk_subfolders_, folder);
}

void AddSourceDialog::OnBrowseBackground(bool folder) {
    std::wstring path;
    bool picked = folder ? PickFolder(hwnd_, tr("Áudio de Fundo"), &path)
                         : PickFile(hwnd_, tr("Áudio de Fundo"), tr("Arquivos de Áudio"),
                                    BackgroundPattern(), &path);
    if (!picked) return;
    SetWindowTextW(txt_file_, path.c_str());
    SetFocus(txt_file_);
}

std::wstring AddSourceDialog::StreamUrl() const {
    std::wstring url = Trim(ui::GetText(txt_url_));
    if (url.size() >= 2 && url.front() == L'"' && url.back() == L'"') url = Trim(url.substr(1, url.size() - 2));
    return url;
}

void AddSourceDialog::OnBrowsePlaylist() {
    std::wstring path;
    if (!PickFile(hwnd_, tr("Lista de Reprodução"), tr("Listas de Reprodução"), L"*.m3u;*.m3u8;*.pls", &path))
        return;
    SetWindowTextW(txt_url_, path.c_str());
    SetFocus(txt_url_);
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
    bool can = SelectedDevice() >= 0 || (filter_ == 4 && !StreamUrl().empty());
    EnableWindow(btn_test_, can && !audio_.is_recording());
}

void AddSourceDialog::OnTest() {
    if (testing_) {
        StopTest();
        return;
    }
    std::pair<int, int> fmt = output_format_ ? output_format_() : std::make_pair(44100, 2);
    if (filter_ == 4) {
        // It connects in the background; until then the button already
        // stops it, and a failure is told when it comes.
        std::wstring url = StreamUrl();
        if (url.empty()) return;
        HWND self = hwnd_;
        audio_.SetMonitorPan(Pan());
        audio_.StartUrlTest(url, Volume(), fmt.second, fx_,
                            [self](int err) { PostMessageW(self, WM_URL_TEST, (WPARAM)err, 0); });
        testing_ = true;
        ui::SetText(btn_test_, tr("&Parar"));
        return;
    }
    int dev_idx = SelectedDevice();
    if (dev_idx < 0) return;
    int err = 0;
    audio_.SetMonitorPan(Pan());
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

/* The test follows the dialogs of the equalizer, effects and noise remover
   while they are open, and the stereo check box. */
void AddSourceDialog::Preview(const FxSettings& fx) {
    if (testing_) audio_.SetMonitorFx(fx);
}

void AddSourceDialog::OnEqualizer() {
    EqualizerDialog dlg(fx_, [this](const FxSettings& fx) { Preview(fx); });
    if (dlg.ShowModal(hwnd_) == IDOK) fx_ = dlg.result;
    Preview(fx_);
}

void AddSourceDialog::OnEffects() {
    EffectsDialog dlg(fx_, [this](const FxSettings& fx) { Preview(fx); });
    if (dlg.ShowModal(hwnd_) == IDOK) fx_ = dlg.result;
    Preview(fx_);
}

void AddSourceDialog::OnNoise() {
    NoiseDialog dlg(fx_, [this](const FxSettings& fx) { Preview(fx); });
    if (dlg.ShowModal(hwnd_) == IDOK) fx_ = dlg.result;
    Preview(fx_);
}

void AddSourceDialog::OnSidechain() {
    SidechainDialog dlg(fx_);
    if (dlg.ShowModal(hwnd_) == IDOK) fx_ = dlg.result;
}

void AddSourceDialog::StopTest() {
    if (!testing_) return;
    audio_.StopMonitor();
    testing_ = false;
    ui::SetText(btn_test_, tr("&Testar"));
}

INT_PTR AddSourceDialog::OnUrlTestDone(int error) {
    if (!testing_ || !error) return 0;
    StopTest();
    ui::MsgBox(hwnd_,
               Format(tr("Não foi possível conectar a este link (erro BASS {erro})."), {{L"erro", std::to_wstring(error)}}),
               tr("Erro"), MB_ICONERROR | MB_OK);
    return 0;
}

double AddSourceDialog::Volume() const {
    int pos = (int)SendMessageW(sld_, TBM_GETPOS, 0, 0);
    return std::pow(10.0, ((pos - VOLUME_CENTER) * VOLUME_STEP) / 20.0);
}

void AddSourceDialog::UpdateDbLabel() {
    int pos = (int)SendMessageW(sld_, TBM_GETPOS, 0, 0);
    double db = (pos - VOLUME_CENTER) * VOLUME_STEP;
    wchar_t text[32];
    swprintf(text, 32, L"%+.1f dB", db);
    ui::SetAccessibleValue(sld_, text);
    if (testing_) audio_.SetMonitorVolume(Volume());
}

double AddSourceDialog::Pan() const {
    int pos = (int)SendMessageW(sld_pan_, TBM_GETPOS, 0, 0);
    return (pos - 50) / 50.0;
}

/* The balance slider says its position, 0 to 100. */
void AddSourceDialog::UpdatePanLabel() {
    int pos = (int)SendMessageW(sld_pan_, TBM_GETPOS, 0, 0);
    ui::SetAccessibleValue(sld_pan_, std::to_wstring(pos));
    if (testing_) audio_.SetMonitorPan(Pan());
}

LRESULT CALLBACK AddSourceDialog::SliderProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR,
                                             DWORD_PTR ref) {
    if (msg == WM_KEYDOWN) {
        AddSourceDialog* self = (AddSourceDialog*)ref;
        const bool pan = hwnd == self->sld_pan_;
        const int max = pan ? PAN_MAX : VOLUME_MAX, page = pan ? PAN_PAGE : VOLUME_PAGE;
        int val = (int)SendMessageW(hwnd, TBM_GETPOS, 0, 0);
        int new_val = -1;
        switch (wp) {
            case VK_HOME: new_val = 0; break;
            case VK_END: new_val = max; break;
            case VK_UP:
            case VK_RIGHT: new_val = std::min(max, val + 1); break;
            case VK_DOWN:
            case VK_LEFT: new_val = std::max(0, val - 1); break;
            // To the next multiple of the page: round values (3 dB, 6 dB...).
            case VK_PRIOR: new_val = std::min(max, (val / page + 1) * page); break;
            case VK_NEXT: new_val = std::max(0, (int)std::floor((val - 1) / (double)page) * page); break;
        }
        if (new_val >= 0) {
            SendMessageW(hwnd, TBM_SETPOS, TRUE, new_val);
            if (hwnd == self->sld_pan_) self->UpdatePanLabel();
            else self->UpdateDbLabel();
            return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

void AddSourceDialog::OnOk() {
    StopTest();
    if (filter_ == 3) {
        std::wstring path = BackgroundPath();
        if (path.empty()) return;
        wchar_t full[MAX_PATH * 4];
        DWORD n = GetFullPathNameW(path.c_str(), MAX_PATH * 4, full, nullptr);
        if (n > 0 && n < MAX_PATH * 4) path = full;
        bool subfolders = Button_GetCheck(chk_subfolders_) == BST_CHECKED;
        if (BackgroundFiles(path, subfolders).empty()) {
            ui::MsgBox(hwnd_,
                       Format(tr("Nenhum arquivo de áudio foi encontrado em:\n{caminho}"),
                              {{L"caminho", path}}),
                       tr("Aviso"), MB_ICONWARNING | MB_OK);
            SetFocus(txt_file_);
            return;
        }
        selected.type = L"file";
        selected.id = 0;
        selected.name = path;
        selected.volume = Volume();
        selected.pan = Pan();
        selected.fx = fx_;
        // A recorded file needs no cleaning: no noise remover.
        selected.fx.noise.on = false;
        selected.loop = Button_GetCheck(chk_loop_) == BST_CHECKED;
        selected.shuffle = Button_GetCheck(chk_shuffle_) == BST_CHECKED;
        selected.subfolders = subfolders;
        selected.listen = Button_GetCheck(chk_listen_) == BST_CHECKED;
        has_selected = true;
        EndModal(IDOK);
        return;
    }
    if (filter_ == 4) {
        std::wstring url = StreamUrl();
        if (url.empty()) return;
        std::wstring low = ToLower(url);
        bool web = low.compare(0, 7, L"http://") == 0 || low.compare(0, 8, L"https://") == 0 ||
                   low.compare(0, 6, L"ftp://") == 0;
        if (!web && !FileExists(url)) {
            ui::MsgBox(hwnd_,
                       tr("Digite o link de um stream (começando com http:// ou https://) ou o caminho de "
                          "uma lista M3U, PLS ou M3U8 do computador."),
                       tr("Aviso"), MB_ICONWARNING | MB_OK);
            SetFocus(txt_url_);
            return;
        }
        selected.type = L"url";
        selected.id = 0;
        selected.name = url;
        selected.volume = Volume();
        selected.pan = Pan();
        selected.fx = fx_;
        selected.fx.noise.on = false;  // only for audio devices
        selected.listen = Button_GetCheck(chk_listen_) == BST_CHECKED;
        has_selected = true;
        EndModal(IDOK);
        return;
    }
    if (filter_ == 2) {
        // Only the file name counts, and ".exe" is added when missing.
        std::wstring name = BaseName(Trim(ui::GetText(txt_exe_)));
        if (name.empty()) return;
        if (name.size() < 4 || ToLower(name.substr(name.size() - 4)) != L".exe") name += L".exe";
        selected.type = L"exe";
        selected.id = 0;
        selected.name = name;
        selected.volume = Volume();
        selected.pan = Pan();
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
    selected.pan = Pan();
    selected.fx = fx_;
    if (selected.type != L"device") selected.fx.noise.on = false;
    has_selected = true;
    EndModal(IDOK);
}

/* ---- Scheduled recordings ---- */

namespace {
enum {
    IDC_SCH_LIST = 451,
    IDC_SCH_ADD,
    IDC_SCH_EDIT,
    IDC_SCH_REMOVE,
    IDC_SCH_DATE,
    IDC_SCH_TIME,
    IDC_SCH_DURATION,
    IDC_SCH_REMOVE_AFTER,
};
}  // namespace

ScheduleListDialog::ScheduleListDialog(const std::vector<Schedule>& schedules)
    : ui::Dialog(tr("Agendamento de Gravações")), result(schedules), list_(schedules) {}

void ScheduleListDialog::OnInit() {
    auto vbox = VBox();
    HWND label = Add(WC_STATICW, tr("A&gendamentos:"), 0);
    lst_ = Add(WC_LISTVIEWW, L"", WS_TABSTOP | LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SINGLESEL |
                                      LVS_SHOWSELALWAYS,
               IDC_SCH_LIST, WS_EX_CLIENTEDGE);
    ListView_SetExtendedListViewStyle(lst_, LVS_EX_FULLROWSELECT);
    LVCOLUMNW col = {};
    col.mask = LVCF_WIDTH;
    col.cx = S(400);
    ListView_InsertColumn(lst_, 0, &col);
    vbox->Add(label, 0, ui::LEFT | ui::RIGHT | ui::TOP, S(10));
    vbox->Add(lst_, 1, ui::ALL | ui::EXPAND, S(10), SIZE{S(420), S(180)});

    auto tools = HBox();
    HWND add = Add(WC_BUTTONW, tr("&Adicionar"), BUTTON, IDC_SCH_ADD);
    btn_edit_ = Add(WC_BUTTONW, tr("&Editar"), BUTTON, IDC_SCH_EDIT);
    btn_remove_ = Add(WC_BUTTONW, tr("&Remover"), BUTTON, IDC_SCH_REMOVE);
    tools->Add(add, 0, ui::RIGHT, S(5));
    tools->Add(btn_edit_, 0, ui::RIGHT, S(5));
    tools->Add(btn_remove_, 0, 0, 0);
    vbox->Add(std::move(tools), 0, ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));

    auto hb = HBox();
    HWND ok = Add(WC_BUTTONW, tr("&Ok"), BUTTON | BS_DEFPUSHBUTTON, IDOK);
    HWND cancel = Add(WC_BUTTONW, tr("&Cancelar"), BUTTON, IDCANCEL);
    hb->Add(ok, 0, ui::RIGHT, S(5));
    hb->Add(cancel, 0, 0, 0);
    vbox->Add(std::move(hb), 0, ui::CENTER | ui::ALL, S(15));
    root_ = std::move(vbox);
    Refresh(0);
    Fit();
    SetFocus(list_.empty() ? add : lst_);
}

void ScheduleListDialog::Refresh(int select) {
    ListView_DeleteAllItems(lst_);
    for (auto& s : list_) {
        std::wstring text = ScheduleText(s);
        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = ListView_GetItemCount(lst_);
        item.pszText = const_cast<wchar_t*>(text.c_str());
        ListView_InsertItem(lst_, &item);
    }
    ListView_SetColumnWidth(lst_, 0, LVSCW_AUTOSIZE_USEHEADER);
    if (!list_.empty()) {
        select = std::max(0, std::min((int)list_.size() - 1, select));
        ListView_SetItemState(lst_, select, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(lst_, select, FALSE);
    }
    EnableWindow(btn_edit_, !list_.empty());
    EnableWindow(btn_remove_, !list_.empty());
}

int ScheduleListDialog::Selected() const { return ListView_GetNextItem(lst_, -1, LVNI_SELECTED); }

void ScheduleListDialog::OnAdd() {
    ScheduleDialog dlg(nullptr);
    if (dlg.ShowModal(hwnd_) != IDOK) return;
    list_.push_back(dlg.result);
    Refresh((int)list_.size() - 1);
    SetFocus(lst_);
}

void ScheduleListDialog::OnEdit() {
    int sel = Selected();
    if (sel < 0) return;
    ScheduleDialog dlg(&list_[sel]);
    if (dlg.ShowModal(hwnd_) != IDOK) return;
    list_[sel] = dlg.result;
    Refresh(sel);
    SetFocus(lst_);
}

void ScheduleListDialog::OnRemove() {
    int sel = Selected();
    if (sel < 0) return;
    list_.erase(list_.begin() + sel);
    Refresh(sel);
    SetFocus(list_.empty() ? GetDlgItem(hwnd_, IDC_SCH_ADD) : lst_);
}

bool ScheduleListDialog::OnCharHook(const MSG& msg) {
    // Delete in the list removes, as in the list of sources.
    if (msg.message == WM_KEYDOWN && msg.wParam == VK_DELETE && msg.hwnd == lst_) {
        OnRemove();
        return true;
    }
    return false;
}

bool ScheduleListDialog::OnCommand(int id, int code, HWND) {
    switch (id) {
        case IDC_SCH_ADD:
            if (code == BN_CLICKED) OnAdd();
            return true;
        case IDC_SCH_EDIT:
            if (code == BN_CLICKED) OnEdit();
            return true;
        case IDC_SCH_REMOVE:
            if (code == BN_CLICKED) OnRemove();
            return true;
        case IDOK:
            result = list_;
            EndModal(IDOK);
            return true;
    }
    return false;
}

ScheduleDialog::ScheduleDialog(const Schedule* edit)
    : ui::Dialog(edit ? tr("Editar Agendamento") : tr("Adicionar Agendamento")), editing_(edit != nullptr) {
    if (edit) result = *edit;
}

void ScheduleDialog::OnInit() {
    auto vbox = VBox();
    // Without a date it records every day.
    HWND lbl_date = Add(WC_STATICW, tr("&Data (Opcional):"), 0);
    txt_date_ = Add(WC_EDITW, editing_ ? ScheduleDateForEdit(result.date) : L"", EDIT, IDC_SCH_DATE,
                    WS_EX_CLIENTEDGE);
    HWND lbl_time = Add(WC_STATICW, tr("&Horário:"), 0);
    txt_time_ = Add(WC_EDITW, editing_ ? ScheduleTimeForEdit(result) : L"", EDIT, IDC_SCH_TIME,
                    WS_EX_CLIENTEDGE);
    // Without one, the duration field of the main window counts.
    HWND lbl_duration = Add(WC_STATICW, tr("D&uração da Gravação (Opcional):"), 0);
    txt_duration_ = Add(WC_EDITW, editing_ && result.duration > 0 ? FormatDuration(result.duration) : L"",
                        EDIT, IDC_SCH_DURATION, WS_EX_CLIENTEDGE);
    chk_remove_ = Add(WC_BUTTONW, tr("&Remover após concluído"), CHECK, IDC_SCH_REMOVE_AFTER);
    Button_SetCheck(chk_remove_, editing_ && result.remove_after ? BST_CHECKED : BST_UNCHECKED);
    vbox->Add(lbl_date, 0, ui::LEFT | ui::RIGHT | ui::TOP, S(10));
    vbox->Add(txt_date_, 0, ui::LEFT | ui::RIGHT | ui::EXPAND, S(10), SIZE{S(260), 0});
    vbox->Add(lbl_time, 0, ui::LEFT | ui::RIGHT | ui::TOP, S(10));
    vbox->Add(txt_time_, 0, ui::LEFT | ui::RIGHT | ui::EXPAND, S(10));
    vbox->Add(lbl_duration, 0, ui::LEFT | ui::RIGHT | ui::TOP, S(10));
    vbox->Add(txt_duration_, 0, ui::LEFT | ui::RIGHT | ui::EXPAND, S(10));
    vbox->Add(chk_remove_, 0, ui::ALL, S(10));

    auto hb = HBox();
    HWND ok = Add(WC_BUTTONW, tr("&Ok"), BUTTON | BS_DEFPUSHBUTTON, IDOK);
    HWND cancel = Add(WC_BUTTONW, tr("&Cancelar"), BUTTON, IDCANCEL);
    hb->Add(ok, 0, ui::RIGHT, S(5));
    hb->Add(cancel, 0, 0, 0);
    vbox->Add(std::move(hb), 0, ui::CENTER | ui::ALL, S(15));
    root_ = std::move(vbox);
    Fit();
    SetFocus(txt_date_);
}

void ScheduleDialog::OnOk() {
    Schedule s = result;
    std::wstring date = Trim(ui::GetText(txt_date_));
    s.date.clear();
    if (!date.empty() && !ParseScheduleDate(date, &s.date)) {
        ui::MsgBox(hwnd_,
                   Format(tr("Data inválida. Digite como {exemplo}, ou deixe em branco para gravar todos os dias."),
                          {{L"exemplo", ScheduleDateExample()}}),
                   tr("Aviso"), MB_ICONWARNING | MB_OK);
        SetFocus(txt_date_);
        return;
    }
    if (!ParseScheduleTime(ui::GetText(txt_time_), &s.hour, &s.minute, &s.second)) {
        ui::MsgBox(hwnd_, tr("Horário inválido. Digite as horas e os minutos, e os segundos se quiser, por exemplo 14:30 ou 14:30:15."),
                   tr("Aviso"), MB_ICONWARNING | MB_OK);
        SetFocus(txt_time_);
        return;
    }
    if (!ParseDuration(ui::GetText(txt_duration_), &s.duration)) {
        ui::MsgBox(hwnd_,
                   tr("Duração da gravação inválida. Use horas, minutos e segundos, como 1:30:00, "
                      "ou deixe em branco para usar a duração da janela principal."),
                   tr("Aviso"), MB_ICONWARNING | MB_OK);
        SetFocus(txt_duration_);
        return;
    }
    if (SchedulePassed(s)) {
        ui::MsgBox(hwnd_, tr("Essa data e esse horário já passaram."), tr("Aviso"), MB_ICONWARNING | MB_OK);
        SetFocus(txt_date_);
        return;
    }
    s.remove_after = Button_GetCheck(chk_remove_) == BST_CHECKED;
    result = s;
    EndModal(IDOK);
}

bool ScheduleDialog::OnCommand(int id, int, HWND) {
    if (id == IDOK) {
        OnOk();
        return true;
    }
    return false;
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

namespace {

/* The Windows open dialog, for a folder or for one file. */
bool PickPath(HWND parent, const std::wstring& title, DWORD flags, const COMDLG_FILTERSPEC* filter,
              std::wstring* path) {
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dlg))))
        return false;
    DWORD options = 0;
    dlg->GetOptions(&options);
    dlg->SetOptions(options | flags | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dlg->SetTitle(title.c_str());
    if (filter) dlg->SetFileTypes(1, filter);
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

}  // namespace

bool PickFolder(HWND parent, const std::wstring& title, std::wstring* path) {
    return PickPath(parent, title, FOS_PICKFOLDERS, nullptr, path);
}

bool PickFile(HWND parent, const std::wstring& title, const std::wstring& kind,
              const std::wstring& pattern, std::wstring* path) {
    std::wstring name = kind + L" (" + pattern + L")";
    COMDLG_FILTERSPEC filter = {name.c_str(), pattern.c_str()};
    return PickPath(parent, title, FOS_FILEMUSTEXIST, &filter, path);
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

/* ---- Update offer ---- */

UpdateDialog::UpdateDialog(const UpdateInfo& info)
    : ui::Dialog(tr("Nova atualização disponível!")), info_(info) {}

void UpdateDialog::OnInit() {
    auto vbox = VBox();
    if (!info_.notes.empty()) {
        HWND label = Add(WC_STATICW, tr("Novidades:"), SS_NOPREFIX);
        // No word wrap: a screen reader reads the box line by line, and a
        // wrapped paragraph would stop at the edge of the box, mid-sentence.
        // Each paragraph is one line, read whole; it scrolls sideways.
        notes_ = Add(WC_EDITW, info_.notes,
                     WS_TABSTOP | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL |
                         ES_AUTOHSCROLL | ES_WANTRETURN,
                     0, WS_EX_CLIENTEDGE);
        vbox->Add(label, 0, ui::LEFT | ui::RIGHT | ui::TOP, S(10));
        vbox->Add(notes_, 1, ui::LEFT | ui::RIGHT | ui::TOP | ui::EXPAND, S(10), SIZE{S(440), S(220)});
    }
    std::wstring text = Format(tr("Sua versão: {atual};\nVersão mais recente: {nova}.\nDeseja atualizar agora?"),
                               {{L"atual", VB_VERSION}, {L"nova", info_.version}});
    HWND message = Add(WC_STATICW, text, SS_NOPREFIX);
    HWND yes = Add(WC_BUTTONW, tr("&Sim"), BUTTON, IDYES);
    HWND no = Add(WC_BUTTONW, tr("&Não"), BUTTON, IDCANCEL);
    vbox->Add(message, 0, ui::ALL, S(10));
    auto buttons = HBox();
    buttons->Add(yes, 0, ui::RIGHT, S(10));
    buttons->Add(no, 0, 0, 0);
    vbox->Add(std::move(buttons), 0, ui::CENTER | ui::LEFT | ui::RIGHT | ui::BOTTOM, S(10));
    root_ = std::move(vbox);
    Fit();
    SetFocus(notes_ ? notes_ : yes);
    if (notes_) SendMessageW(notes_, EM_SETSEL, 0, 0);
}

bool UpdateDialog::OnCharHook(const MSG& msg) {
    // Enter in the notes: nothing, not even the button.
    return IsEnter(msg) && msg.hwnd == notes_;
}

bool UpdateDialog::OnCommand(int id, int, HWND) {
    if (id == IDYES) {
        EndModal(IDOK);
        return true;
    }
    return id == IDOK;  // Enter outside the buttons does nothing
}

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
