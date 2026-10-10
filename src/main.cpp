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

/* Main window, system tray and global hotkeys. */

#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>
#include <windowsx.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "audio_engine.h"
#include "config.h"
#include "dialogs.h"
#include "hotkeys.h"
#include "i18n.h"
#include "process_audio.h"
#include "ui.h"
#include "updater.h"
#include "util.h"

using ui::S;

namespace {

enum {
    IDC_RECORD = 1001,
    IDC_PAUSE,
    IDC_PLAY,
    IDC_DELETE,
    IDC_CONTINUE,
    IDC_FORMAT,
    IDC_BITRATE,
    IDC_FLAC,
    IDC_RATE,
    IDC_CHANNELS,
    IDC_SOURCES,
    IDC_ADD,
    IDC_EDIT,
    IDC_REMOVE,
    IDC_SEPARATE,
    IDC_TONES,
    IDC_FILENAME,
    IDC_DIR,
    IDC_BROWSE,
    IDC_OPEN_DIR,
    IDC_SETTINGS,
    IDC_ABOUT,
    IDC_MIN,
    IDC_CLOSE,
};

enum { HK_REC = 1, HK_PAUSE, HK_STOP, HK_SHOW, HK_EXIT };

enum {
    MENU_RECORD = 2001,
    MENU_CONTINUE,
    MENU_PLAY,
    MENU_DELETE,
    MENU_PAUSE,
    MENU_STOP,
    MENU_SHOW,
    MENU_EXIT,
    MENU_SEPARATE,
    MENU_TONES,
    MENU_COMBO = 3000,  // + 100 * combo box + item
};

/* Marks the running instance; a second one only warns and quits. */
const wchar_t* const INSTANCE_MUTEX = L"VBRecorder_Mutex";
HANDLE g_instance_mutex = nullptr;

const UINT WM_TRAY = WM_APP + 1;
const UINT WM_UPDATE_FOUND = WM_APP + 3;  // lParam: an UpdateInfo to delete
const UINT_PTR TIMER_PLAY = 1;

const std::vector<std::wstring> BITRATES = {L"8 KBPS",   L"16 KBPS",  L"32 KBPS",  L"64 KBPS",
                                            L"96 KBPS",  L"128 KBPS", L"160 KBPS", L"192 KBPS",
                                            L"256 KBPS", L"320 KBPS"};

std::unique_ptr<ui::Box> HBox() { return std::unique_ptr<ui::Box>(new ui::Box(false)); }

class MainFrame {
public:
    ConfigManager config;
    AudioEngine audio;

    bool Create();
    void ShowAtStart();
    void StartUpdateCheck();
    bool OnCharHook(const MSG& msg);
    HWND hwnd() const { return hwnd_; }

private:
    static INT_PTR CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    INT_PTR Handle(UINT msg, WPARAM wp, LPARAM lp);
    static LRESULT CALLBACK ListProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

    HWND Add(const wchar_t* cls, DWORD style, int id, DWORD ex_style = 0);
    void SetupUi();
    void Retranslate();
    void RefreshDeviceNames();
    void RefreshList();
    void Layout();

    bool MinimizeToTray() const { return config.GetBoolean(L"minimizar_bandeja"); }
    void ApplyAlwaysOnTop();
    void Iconize();
    void ShowMain();
    void RegisterAllHotkeys();
    void OnHotkey(int id);

    void OnAddSource();
    bool AddToRecording(const Source& src);
    void OnEditSource();
    void OnRemoveSource();
    void OnSettings();
    bool OfferUpdate(const UpdateInfo& info, HWND parent);
    bool CheckUpdateNow(HWND parent);
    static DWORD WINAPI UpdateCheckThread(void* window);
    void OnFormatChange();
    void LoadSettings();
    void SaveSettings();
    void OnRecordToggle();
    void OnPauseToggle();
    void RecordingStarted();
    bool CanContinue();
    void OnContinueLast();
    void OnBrowse();
    std::wstring CurrentOutputDir();
    void OnOpenDir();
    std::vector<std::wstring> ExistingRecordingFiles();
    bool HasRecording();
    HWND DialogParent();
    void Warn(const std::wstring& message, const std::wstring& title, UINT icon);
    void UpdateRecordingButtons();
    void UpdatePlayButton();
    void OnPlayToggle();
    void OnDeleteRecording();
    void OnAbout();
    void ToggleUi(bool enable);
    bool OnClose();

    void AddTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu();
    void AddSubmenu(HMENU parent, const std::wstring& label, int index, HWND combo);
    void OnTrayCommand(UINT cmd);
    std::pair<int, int> OutputFormat();

    HWND hwnd_ = nullptr;
    std::unique_ptr<ui::Box> root_;
    std::vector<Source> sources_;
    std::vector<std::wstring> last_recording_;
    bool play_timer_ = false;
    bool tray_added_ = false;
    UINT taskbar_created_ = 0;
    HICON tray_icon_ = nullptr;
    std::vector<HWND> menu_combos_;

    HWND btn_record_, btn_pause_, btn_continue_, btn_play_, btn_delete_;
    HWND lbl_format_, cmb_format_, lbl_bitrate_, cmb_bitrate_, lbl_flac_level_, cmb_flac_level_;
    HWND lbl_rate_, cmb_rate_, lbl_channels_, cmb_channels_;
    HWND lbl_sources_, lst_fontes_, btn_add_, btn_edit_, btn_rem_;
    HWND chk_separate_, chk_tones_;
    HWND lbl_filename_, txt_filename_, lbl_dir_, txt_dir_, btn_browse_, btn_open_dir_;
    HWND btn_settings_, btn_about_, btn_min_, btn_close_;
};

bool Checked(HWND hwnd) { return Button_GetCheck(hwnd) == BST_CHECKED; }
void SetChecked(HWND hwnd, bool on) { Button_SetCheck(hwnd, on ? BST_CHECKED : BST_UNCHECKED); }

bool MainFrame::Create() {
    hwnd_ = ui::CreateDialogWindow(nullptr, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 0,
                                   L"VB Recorder", Proc, (LPARAM)this);
    if (!hwnd_) return false;
    HICON icon = LoadIconW(nullptr, IDI_APPLICATION);
    SendMessageW(hwnd_, WM_SETICON, ICON_BIG, (LPARAM)icon);
    SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, (LPARAM)icon);

    // Window size of the previous version: 600 by 850, without exceeding the screen.
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTOPRIMARY), &mi);
    RECT work = mi.rcWork;
    int w = std::min<int>(S(600), work.right - work.left);
    int h = std::min<int>(S(850), work.bottom - work.top);
    SetWindowPos(hwnd_, nullptr, work.left + (work.right - work.left - w) / 2,
                 work.top + (work.bottom - work.top - h) / 2, w, h, SWP_NOZORDER | SWP_NOACTIVATE);

    SetupUi();
    LoadSettings();
    ApplyAlwaysOnTop();

    taskbar_created_ = RegisterWindowMessageW(L"TaskbarCreated");
    AddTrayIcon();
    RegisterAllHotkeys();
    Layout();
    return true;
}

void MainFrame::ShowAtStart() {
    // When starting minimized, the program stays in the tray without opening the window.
    if (!config.GetBoolean(L"iniciar_minimizado")) {
        ShowWindow(hwnd_, SW_SHOW);
        SetFocus(btn_record_);
    }
}

INT_PTR CALLBACK MainFrame::Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainFrame* self = (MainFrame*)GetWindowLongPtrW(hwnd, DWLP_USER);
    if (msg == WM_INITDIALOG) {
        self = (MainFrame*)lp;
        SetWindowLongPtrW(hwnd, DWLP_USER, (LONG_PTR)self);
        self->hwnd_ = hwnd;
        return FALSE;
    }
    if (!self) return FALSE;
    return self->Handle(msg, wp, lp);
}

INT_PTR MainFrame::Handle(UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == taskbar_created_ && taskbar_created_) {
        tray_added_ = false;
        AddTrayIcon();
        return TRUE;
    }
    switch (msg) {
        case WM_COMMAND: {
            int id = LOWORD(wp), code = HIWORD(wp);
            if (code == BN_CLICKED) {
                switch (id) {
                    case IDC_RECORD: OnRecordToggle(); return TRUE;
                    case IDC_PAUSE: OnPauseToggle(); return TRUE;
                    case IDC_PLAY: OnPlayToggle(); return TRUE;
                    case IDC_DELETE: OnDeleteRecording(); return TRUE;
                    case IDC_CONTINUE: OnContinueLast(); return TRUE;
                    case IDC_ADD: OnAddSource(); return TRUE;
                    case IDC_EDIT: OnEditSource(); return TRUE;
                    case IDC_REMOVE: OnRemoveSource(); return TRUE;
                    case IDC_BROWSE: OnBrowse(); return TRUE;
                    case IDC_OPEN_DIR: OnOpenDir(); return TRUE;
                    case IDC_SETTINGS: OnSettings(); return TRUE;
                    case IDC_ABOUT: OnAbout(); return TRUE;
                    case IDC_MIN: Iconize(); return TRUE;
                    case IDC_CLOSE: OnClose(); return TRUE;
                }
            }
            if (id == IDC_FORMAT && code == CBN_SELCHANGE) OnFormatChange();
            return TRUE;  // IDOK and IDCANCEL (Enter and Esc) do not close the window
        }
        case WM_CLOSE:
            OnClose();
            return TRUE;
        case WM_SIZE:
            // Goes to the tray only if the option is on; otherwise it minimizes
            // to the taskbar like any other window.
            if (wp == SIZE_MINIMIZED) {
                if (MinimizeToTray()) ShowWindow(hwnd_, SW_HIDE);
            } else {
                Layout();
            }
            return TRUE;
        case WM_HOTKEY:
            OnHotkey((int)wp);
            return TRUE;
        case WM_TIMER:
            if (wp == TIMER_PLAY && !audio.IsPlaying()) UpdatePlayButton();
            return TRUE;
        case WM_TRAY:
            if (LOWORD(lp) == WM_LBUTTONDBLCLK) ShowMain();
            else if (LOWORD(lp) == WM_RBUTTONUP) ShowTrayMenu();
            return TRUE;
        case WM_UPDATE_FOUND: {
            // Found when the program started; the window is already open.
            std::unique_ptr<UpdateInfo> info((UpdateInfo*)lp);
            if (!audio.is_recording() && OfferUpdate(*info, DialogParent())) OnClose();
            return TRUE;
        }
        case WM_ENDSESSION:
            // Windows is shutting down: the file being recorded is closed properly.
            if (wp) {
                audio.StopRecording();
                SaveSettings();
            }
            return TRUE;
    }
    return FALSE;
}

HWND MainFrame::Add(const wchar_t* cls, DWORD style, int id, DWORD ex_style) {
    return ui::Create(hwnd_, cls, L"", WS_VISIBLE | style, id, ex_style);
}

void MainFrame::SetupUi() {
    const DWORD BUTTON = WS_TABSTOP | BS_PUSHBUTTON;
    const DWORD COMBO = WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST;
    const DWORD CHECK = WS_TABSTOP | BS_AUTOCHECKBOX;
    const DWORD EDIT = WS_TABSTOP | ES_AUTOHSCROLL;

    root_ = std::unique_ptr<ui::Box>(new ui::Box(true));
    ui::Box* vbox = root_.get();

    auto hbox_ctrl = HBox();
    btn_record_ = Add(WC_BUTTONW, BUTTON, IDC_RECORD);
    btn_pause_ = Add(WC_BUTTONW, BUTTON, IDC_PAUSE);
    EnableWindow(btn_pause_, FALSE);
    btn_play_ = Add(WC_BUTTONW, BUTTON, IDC_PLAY);
    EnableWindow(btn_play_, FALSE);
    btn_delete_ = Add(WC_BUTTONW, BUTTON, IDC_DELETE);
    EnableWindow(btn_delete_, FALSE);
    btn_continue_ = Add(WC_BUTTONW, BUTTON, IDC_CONTINUE);
    EnableWindow(btn_continue_, FALSE);
    // The continue button sits next to Pause, which is what it resembles.
    SetWindowPos(btn_continue_, btn_pause_, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    for (HWND b : {btn_record_, btn_pause_, btn_continue_, btn_play_, btn_delete_})
        hbox_ctrl->Add(b, 0, ui::ALL, S(5));
    vbox->Add(std::move(hbox_ctrl), 0, ui::CENTER | ui::ALL, S(10));

    auto labeled_combo = [&](HWND* lbl, HWND* cmb, int id, const std::vector<std::wstring>& choices) {
        *lbl = Add(WC_STATICW, 0, 0);
        *cmb = Add(WC_COMBOBOXW, COMBO, id);
        ui::ComboSetItems(*cmb, choices);
        auto hb = HBox();
        hb->Add(*lbl, 0, ui::CENTER_V | ui::RIGHT, S(5));
        hb->Add(*cmb, 1, ui::EXPAND, 0);
        vbox->Add(std::move(hb), 0, ui::ALL | ui::EXPAND, S(5));
    };
    labeled_combo(&lbl_format_, &cmb_format_, IDC_FORMAT,
                  {L"mp3", L"wav", L"aiff", L"ogg", L"opus", L"flac"});
    labeled_combo(&lbl_bitrate_, &cmb_bitrate_, IDC_BITRATE, BITRATES);
    labeled_combo(&lbl_flac_level_, &cmb_flac_level_, IDC_FLAC,
                  {L"0", L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8"});
    labeled_combo(&lbl_rate_, &cmb_rate_, IDC_RATE,
                  {L"8000", L"11025", L"22050", L"32000", L"44100", L"48000"});
    labeled_combo(&lbl_channels_, &cmb_channels_, IDC_CHANNELS, {L"mono", L"stereo"});

    lbl_sources_ = Add(WC_STATICW, 0, 0);
    vbox->Add(lbl_sources_, 0, ui::LEFT | ui::TOP, S(10));
    lst_fontes_ = Add(WC_LISTVIEWW, WS_TABSTOP | LVS_REPORT | LVS_NOCOLUMNHEADER, IDC_SOURCES,
                      WS_EX_CLIENTEDGE);
    ListView_SetExtendedListViewStyle(lst_fontes_, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    LVCOLUMNW col = {};
    col.mask = LVCF_WIDTH;
    col.cx = S(100);
    ListView_InsertColumn(lst_fontes_, 0, &col);
    SetWindowSubclass(lst_fontes_, ListProc, 1, 0);
    vbox->Add(lst_fontes_, 1, ui::ALL | ui::EXPAND, S(5));

    auto hb_src = HBox();
    btn_add_ = Add(WC_BUTTONW, BUTTON, IDC_ADD);
    btn_edit_ = Add(WC_BUTTONW, BUTTON, IDC_EDIT);
    btn_rem_ = Add(WC_BUTTONW, BUTTON, IDC_REMOVE);
    hb_src->Add(btn_add_, 0, ui::RIGHT, S(5));
    hb_src->Add(btn_edit_, 0, ui::RIGHT, S(5));
    hb_src->Add(btn_rem_, 0, 0, 0);
    vbox->Add(std::move(hb_src), 0, ui::LEFT | ui::BOTTOM, S(10));

    chk_separate_ = Add(WC_BUTTONW, CHECK, IDC_SEPARATE);
    vbox->Add(chk_separate_, 0, ui::ALL, S(10));
    chk_tones_ = Add(WC_BUTTONW, CHECK, IDC_TONES);
    vbox->Add(chk_tones_, 0, ui::ALL, S(10));

    lbl_filename_ = Add(WC_STATICW, 0, 0);
    txt_filename_ = Add(WC_EDITW, EDIT, IDC_FILENAME, WS_EX_CLIENTEDGE);
    {
        auto hb = HBox();
        hb->Add(lbl_filename_, 0, ui::CENTER_V | ui::RIGHT, S(5));
        hb->Add(txt_filename_, 1, ui::EXPAND, 0);
        vbox->Add(std::move(hb), 0, ui::ALL | ui::EXPAND, S(5));
    }

    auto hbox_dir = HBox();
    lbl_dir_ = Add(WC_STATICW, 0, 0);
    txt_dir_ = Add(WC_EDITW, EDIT, IDC_DIR, WS_EX_CLIENTEDGE);
    btn_browse_ = Add(WC_BUTTONW, BUTTON, IDC_BROWSE);
    btn_open_dir_ = Add(WC_BUTTONW, BUTTON, IDC_OPEN_DIR);
    hbox_dir->Add(lbl_dir_, 0, ui::CENTER_V | ui::RIGHT, S(5));
    hbox_dir->Add(txt_dir_, 1, ui::EXPAND | ui::RIGHT, S(5));
    hbox_dir->Add(btn_browse_, 0, ui::RIGHT, S(5));
    hbox_dir->Add(btn_open_dir_, 0, 0, 0);
    vbox->Add(std::move(hbox_dir), 0, ui::ALL | ui::EXPAND, S(10));

    auto hbox_bottom = HBox();
    btn_settings_ = Add(WC_BUTTONW, BUTTON, IDC_SETTINGS);
    btn_about_ = Add(WC_BUTTONW, BUTTON, IDC_ABOUT);
    btn_min_ = Add(WC_BUTTONW, BUTTON, IDC_MIN);
    btn_close_ = Add(WC_BUTTONW, BUTTON, IDC_CLOSE);
    hbox_bottom->Add(btn_settings_, 0, ui::RIGHT, S(5));
    hbox_bottom->Add(btn_about_, 0, ui::RIGHT, S(5));
    hbox_bottom->Add(btn_min_, 0, ui::RIGHT, S(5));
    hbox_bottom->Add(btn_close_, 0, 0, 0);
    vbox->Add(std::move(hbox_bottom), 0, ui::ALIGN_RIGHT | ui::ALL, S(10));

    Retranslate();
}

/* Writes every text of the window in the current language. It is the only
   place they appear, so switching languages leaves nothing behind. */
void MainFrame::Retranslate() {
    ui::SetText(lbl_format_, tr("&Formato de Saída:"));
    ui::SetText(lbl_bitrate_, tr("&Bitrate:"));
    ui::SetText(lbl_flac_level_, tr("Compressão F&LAC:"));
    ui::SetText(lbl_rate_, tr("&Taxa de Amostragem:"));
    ui::SetText(lbl_channels_, tr("&Canais:"));
    ui::SetText(lbl_sources_, tr("Fontes de &Gravação:"));
    ui::SetText(lbl_filename_, tr("&Nome do Arquivo:"));
    ui::SetText(lbl_dir_, tr("&Diretório de Saída:"));

    ui::SetText(btn_add_, tr("&Adicionar"));
    ui::SetText(btn_edit_, tr("&Editar"));
    ui::SetText(btn_rem_, tr("&Remover"));
    ui::SetText(btn_browse_, tr("P&rocurar..."));
    ui::SetText(btn_open_dir_, tr("Abr&ir Pasta de Gravações"));
    ui::SetText(btn_settings_, tr("Config&urações..."));
    ui::SetText(btn_about_, tr("S&obre"));
    ui::SetText(btn_min_, tr("&Minimizar"));
    ui::SetText(btn_close_, tr("&Fechar"));
    ui::SetText(btn_delete_, tr("E&xcluir Gravação"));
    ui::SetText(btn_continue_, tr("Continuar Última Gra&vação"));

    ui::SetText(chk_separate_, tr("Gravar fontes em arquivos se&parados"));
    ui::SetText(chk_tones_, tr("Reproduzir &tons ao iniciar ou parar gravação"));

    ui::SetText(btn_record_, audio.is_recording() ? tr("Para&r") : tr("&Gravar"));
    ui::SetText(btn_pause_, audio.is_paused() ? tr("C&ontinuar") : tr("&Pausar"));
    UpdatePlayButton();

    RefreshDeviceNames();
    Layout();
}

/* Renames the sources that are devices, in case the language changed: the
   name of the default device is translated. */
void MainFrame::RefreshDeviceNames() {
    if (sources_.empty()) return;
    std::map<int, std::wstring> names;
    for (auto& d : audio.GetInputDevices()) names[d.first] = d.second;
    bool changed = false;
    for (auto& src : sources_) {
        if (src.type == L"device" && names.count(src.id) && src.name != names[src.id]) {
            src.name = names[src.id];
            changed = true;
        }
    }
    if (changed) RefreshList();
}

void MainFrame::Layout() {
    if (!root_ || !hwnd_) return;
    RECT rc;
    GetClientRect(hwnd_, &rc);
    root_->Place(0, 0, rc.right, rc.bottom);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void MainFrame::RefreshList() {
    ListView_DeleteAllItems(lst_fontes_);
    for (auto& s : sources_) {
        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = ListView_GetItemCount(lst_fontes_);
        item.pszText = const_cast<wchar_t*>(s.name.c_str());
        ListView_InsertItem(lst_fontes_, &item);
    }
    ListView_SetColumnWidth(lst_fontes_, 0, LVSCW_AUTOSIZE);
    ui::Show(chk_separate_, sources_.size() >= 2);
    Layout();
}

LRESULT CALLBACK MainFrame::ListProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR,
                                     DWORD_PTR) {
    // At the ends of the list, the arrows do nothing.
    if (msg == WM_KEYDOWN && (wp == VK_DOWN || wp == VK_UP)) {
        int count = ListView_GetItemCount(hwnd);
        int focused = ListView_GetNextItem(hwnd, -1, LVNI_FOCUSED);
        if (count > 0 && focused != -1) {
            if (wp == VK_DOWN && focused >= count - 1) return 0;
            if (wp == VK_UP && focused <= 0) return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

void MainFrame::ApplyAlwaysOnTop() {
    bool on_top = config.GetBoolean(L"sempre_no_topo", true);
    SetWindowPos(hwnd_, on_top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void MainFrame::Iconize() { ShowWindow(hwnd_, SW_MINIMIZE); }

void MainFrame::ShowMain() {
    ShowWindow(hwnd_, SW_SHOW);
    ShowWindow(hwnd_, SW_RESTORE);
    SetForegroundWindow(hwnd_);
    // Opened from the tray for the first time, the window has no focused
    // control yet.
    HWND focus = GetFocus();
    if (!focus || !IsChild(hwnd_, focus)) SetFocus(btn_record_);
}

bool MainFrame::OnCharHook(const MSG& msg) {
    // Esc minimizes, or closes the program when the tray option is off.
    if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
        if (MinimizeToTray()) Iconize();
        else OnClose();
        return true;
    }
    return false;
}

void MainFrame::RegisterAllHotkeys() {
    for (int id = HK_REC; id <= HK_EXIT; ++id) UnregisterHotKey(hwnd_, id);
    const std::pair<int, const wchar_t*> keys_map[] = {
        {HK_REC, L"gravar"}, {HK_PAUSE, L"pausar"}, {HK_STOP, L"parar"},
        {HK_SHOW, L"mostrar"}, {HK_EXIT, L"fechar"}};
    for (auto& k : keys_map) {
        UINT mods = 0, vk = 0;
        ParseHotkey(config.GetKey(k.second), &mods, &vk);
        if (vk) RegisterHotKey(hwnd_, k.first, mods, vk);
    }
}

void MainFrame::OnHotkey(int id) {
    if (id == HK_REC && !audio.is_recording()) OnRecordToggle();
    else if (id == HK_PAUSE && audio.is_recording()) OnPauseToggle();
    // Stopped, the same hotkey continues the last recording.
    else if (id == HK_PAUSE && CanContinue()) OnContinueLast();
    else if (id == HK_STOP && audio.is_recording()) OnRecordToggle();
    else if (id == HK_SHOW) ShowMain();
    else if (id == HK_EXIT) OnClose();
}

std::pair<int, int> MainFrame::OutputFormat() {
    std::wstring rate = ui::ComboValue(cmb_rate_);
    if (rate.empty() || rate.find_first_not_of(L"0123456789") != std::wstring::npos)
        return {44100, 2};
    return {_wtoi(rate.c_str()), ui::ComboValue(cmb_channels_) == L"mono" ? 1 : 2};
}

void MainFrame::OnAddSource() {
    AddSourceDialog dlg(audio, sources_, nullptr, [this] { return OutputFormat(); });
    int result = dlg.ShowModal(hwnd_);
    audio.StopMonitor();
    if (result == IDOK && dlg.has_selected) {
        // The same executable twice would be recorded twice.
        for (auto& s : sources_)
            if (SameSource(s, dlg.selected)) return;
        if (dlg.selected.type == L"device") audio.InitDevice(dlg.selected.id);
        // While recording, the source joins the recording right away.
        if (audio.is_recording() && !AddToRecording(dlg.selected)) return;
        sources_.push_back(dlg.selected);
        RefreshList();
        SaveSettings();
    }
}

bool MainFrame::AddToRecording(const Source& src) {
    int err = 0;
    if (audio.AddLiveSource(src, &err)) return true;
    ui::MsgBox(DialogParent(),
               Format(tr("Erro BASS {erro} ao adicionar a fonte de gravação."),
                      {{L"erro", std::to_wstring(err)}}),
               tr("Erro"), MB_ICONERROR | MB_OK);
    return false;
}

void MainFrame::OnEditSource() {
    int sel = ListView_GetNextItem(lst_fontes_, -1, LVNI_SELECTED);
    if (sel == -1 || ListView_GetNextItem(lst_fontes_, sel, LVNI_SELECTED) != -1) return;
    Source src = sources_[sel];
    AddSourceDialog dlg(audio, sources_, &src, [this] { return OutputFormat(); });
    int result = dlg.ShowModal(hwnd_);
    audio.StopMonitor();
    if (result == IDOK && dlg.has_selected) {
        const Source& now = dlg.selected;
        if (now.type == L"device") audio.InitDevice(now.id);
        if (audio.is_recording()) {
            if (SameSource(now, src)) {
                // Same source: only the volume changes, right away.
                audio.SetSourceVolume(now, now.volume);
            } else {
                if (!AddToRecording(now)) return;
                audio.RemoveLiveSource(src);
            }
        }
        sources_[sel] = now;
        RefreshList();
        SaveSettings();
    }
}

void MainFrame::OnRemoveSource() {
    std::vector<int> to_remove;
    int idx = ListView_GetNextItem(lst_fontes_, -1, LVNI_SELECTED);
    while (idx != -1) {
        to_remove.push_back(idx);
        idx = ListView_GetNextItem(lst_fontes_, idx, LVNI_SELECTED);
    }
    if (to_remove.empty()) return;
    std::sort(to_remove.rbegin(), to_remove.rend());
    for (int i : to_remove) {
        // While recording, the source leaves the recording; with separate
        // files, its file goes on in silence until the end.
        if (audio.is_recording()) audio.RemoveLiveSource(sources_[i]);
        sources_.erase(sources_.begin() + i);
    }
    RefreshList();
    SaveSettings();
}

/* ---- Updates ---- */

DWORD WINAPI MainFrame::UpdateCheckThread(void* window) {
    auto info = std::make_unique<UpdateInfo>();
    if (CheckForUpdate(info.get()) == UpdateCheck::Available &&
        PostMessageW((HWND)window, WM_UPDATE_FOUND, 0, (LPARAM)info.get()))
        info.release();
    return 0;
}

void MainFrame::StartUpdateCheck() {
    // In the background: the window opens normally meanwhile.
    HANDLE thread = CreateThread(nullptr, 0, UpdateCheckThread, hwnd_, 0, nullptr);
    if (thread) CloseHandle(thread);
}

/* Asks, downloads and installs. True when the new version is starting and this
   one must close. */
bool MainFrame::OfferUpdate(const UpdateInfo& info, HWND parent) {
    std::wstring text = Format(tr("Sua versão: {atual};\nVersão mais recente: {nova}.\nDeseja atualizar agora?"),
                               {{L"atual", VB_VERSION}, {L"nova", info.version}});
    if (ui::MsgBox(parent, text, tr("Nova atualização disponível!"),
                   MB_YESNO | MB_ICONINFORMATION) != IDYES)
        return false;
    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    std::wstring zip = JoinPath(temp, L"VBRecorder-update.zip");
    bool downloaded, cancelled;
    {
        DownloadDialog dlg(info, zip);
        dlg.ShowModal(parent);
        downloaded = dlg.ok;
        cancelled = dlg.cancelled;
    }
    if (!downloaded) {
        if (!cancelled)
            ui::MsgBox(parent, tr("Não foi possível baixar a atualização."), tr("Erro"),
                       MB_ICONERROR | MB_OK);
        return false;
    }
    std::wstring error;
    bool installed = InstallUpdate(zip, info, &error);
    // The new version must not find this one still marked as running.
    if (installed && g_instance_mutex) {
        CloseHandle(g_instance_mutex);
        g_instance_mutex = nullptr;
    }
    if (!installed || !StartUpdatedProgram()) {
        if (!g_instance_mutex) g_instance_mutex = CreateMutexW(nullptr, FALSE, INSTANCE_MUTEX);
        DeleteFileW(zip.c_str());
        ui::MsgBox(parent, Format(tr("Não foi possível instalar a atualização:\n{erro}"), {{L"erro", error}}),
                   tr("Erro"), MB_ICONERROR | MB_OK);
        return false;
    }
    return true;
}

/* The "Check for update" button of the settings. */
bool MainFrame::CheckUpdateNow(HWND parent) {
    HCURSOR old = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    UpdateInfo info;
    UpdateCheck result = CheckForUpdate(&info);
    SetCursor(old);
    if (result == UpdateCheck::Failed) {
        ui::MsgBox(parent, tr("Não foi possível verificar se há atualizações."), tr("Erro"),
                   MB_ICONERROR | MB_OK);
        return false;
    }
    if (result == UpdateCheck::UpToDate) {
        ui::MsgBox(parent, tr("Você já está usando a versão mais recente do VB Recorder."),
                   tr("Verificar Atualização"), MB_ICONINFORMATION | MB_OK);
        return false;
    }
    return OfferUpdate(info, parent);
}

void MainFrame::OnSettings() {
    SettingsDialog dlg(config, [this] { RegisterAllHotkeys(); },
                       [this](HWND parent) { return CheckUpdateNow(parent); });
    int result = dlg.ShowModal(hwnd_);
    if (dlg.updating) {
        // The new version is starting: this one closes.
        OnClose();
        return;
    }
    if (result != IDOK) return;
    std::wstring old_language = config.Get(L"idioma");
    std::wstring new_language = dlg.LanguageCode();
    config.SetConfig(L"idioma", new_language);
    config.SetConfig(L"iniciar_minimizado", dlg.start_minimized);
    config.SetConfig(L"minimizar_bandeja", dlg.tray);
    config.SetConfig(L"sempre_no_topo", dlg.on_top);
    config.SetConfig(L"verificar_atualizacoes", dlg.check_updates);
    config.SetConfig(L"buffer", dlg.BufferValue());
    config.Save();

    audio.SetBuffer(dlg.BufferValue());
    ApplyAlwaysOnTop();
    if (new_language != old_language) {
        i18n::SetLanguage(new_language);
        Retranslate();
    }
}

void MainFrame::OnFormatChange() {
    std::wstring fmt = ui::ComboValue(cmb_format_);
    bool needs_br = fmt == L"mp3" || fmt == L"ogg" || fmt == L"opus";
    ui::Show(lbl_bitrate_, needs_br);
    ui::Show(cmb_bitrate_, needs_br);
    ui::Show(lbl_flac_level_, fmt == L"flac");
    ui::Show(cmb_flac_level_, fmt == L"flac");
    if (needs_br) {
        std::wstring cur = ui::ComboValue(cmb_bitrate_);
        std::vector<std::wstring> base = BITRATES;
        if (fmt == L"ogg" || fmt == L"opus") base.push_back(L"500 KBPS");
        ui::ComboSetItems(cmb_bitrate_, base);
        bool found = std::find(base.begin(), base.end(), cur) != base.end();
        ui::ComboSetValue(cmb_bitrate_, found ? cur : L"320 KBPS");
    }
    Layout();
}

void MainFrame::LoadSettings() {
    auto or_default = [&](const wchar_t* key, const wchar_t* fallback) {
        std::wstring v = config.Get(key);
        return v.empty() ? std::wstring(fallback) : v;
    };
    ui::ComboSetValue(cmb_format_, or_default(L"formato", L"mp3"));
    OnFormatChange();
    ui::ComboSetValue(cmb_bitrate_, or_default(L"bitrate", L"128 KBPS"));
    ui::ComboSetValue(cmb_flac_level_, or_default(L"flac_nivel", L"8"));
    ui::ComboSetValue(cmb_rate_, or_default(L"taxa", L"44100"));
    ui::ComboSetValue(cmb_channels_, or_default(L"canais", L"stereo"));
    SetChecked(chk_tones_, config.GetBoolean(L"reproduzir_tons"));
    SetChecked(chk_separate_, config.GetBoolean(L"arquivos_separados"));
    SetWindowTextW(txt_filename_, config.Get(L"arquivo").c_str());
    SetWindowTextW(txt_dir_, config.Get(L"diretorio").c_str());

    auto all_devices = audio.GetInputDevices();
    sources_.clear();
    for (auto& entry : config.GetSources()) {
        if (entry.type == L"device") {
            for (auto& d : all_devices) {
                if (d.first != entry.id) continue;
                sources_.push_back(Source{L"device", entry.id, d.second, entry.volume});
                audio.InitDevice(entry.id);
                break;
            }
        } else if (entry.type == L"exe") {
            // Kept even with the program closed: it is recorded whenever it runs.
            sources_.push_back(Source{L"exe", 0, entry.exe, entry.volume});
        } else {
            std::wstring resolved = GetProcessName((DWORD)entry.id);
            if (!resolved.empty()) {
                sources_.push_back(Source{L"process", entry.id,
                                          resolved + L"  (PID " + std::to_wstring(entry.id) + L")",
                                          entry.volume});
            }
        }
    }
    if (sources_.empty() && !all_devices.empty()) {
        sources_.push_back(Source{L"device", all_devices[0].first, all_devices[0].second, 1.0});
        audio.InitDevice(all_devices[0].first);
    }
    RefreshList();
}

void MainFrame::SaveSettings() {
    config.SetConfig(L"formato", ui::ComboValue(cmb_format_));
    config.SetConfig(L"bitrate", ui::ComboValue(cmb_bitrate_));
    config.SetConfig(L"flac_nivel", ui::ComboValue(cmb_flac_level_));
    config.SetConfig(L"taxa", ui::ComboValue(cmb_rate_));
    config.SetConfig(L"canais", ui::ComboValue(cmb_channels_));
    config.SetConfig(L"reproduzir_tons", Checked(chk_tones_));
    config.SetConfig(L"arquivos_separados", Checked(chk_separate_));
    config.SetConfig(L"arquivo", ui::GetText(txt_filename_));
    config.SetConfig(L"diretorio", ui::GetText(txt_dir_));
    config.SetSources(sources_);
    config.Save();
}

void MainFrame::OnRecordToggle() {
    if (!audio.is_recording()) {
        audio.StopMonitor();
        if (sources_.empty()) {
            ui::MsgBox(DialogParent(), tr("Adicione ao menos uma fonte de gravação antes de iniciar."),
                           tr("Aviso"), MB_ICONWARNING | MB_OK);
            return;
        }
        audio.StopPlayback();
        UpdatePlayButton();
        if (Checked(chk_tones_)) audio.PlayTone(880, 150);
        std::wstring out_dir = CurrentOutputDir();
        if (!DirExists(out_dir)) MakeDirs(out_dir);
        std::wstring ext = ui::ComboValue(cmb_format_);
        std::wstring filename = Trim(ui::GetText(txt_filename_));
        if (filename.empty()) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            wchar_t stamp[32];
            swprintf(stamp, 32, L"%04d-%02d-%02d_%02d-%02d-%02d", st.wYear, st.wMonth, st.wDay,
                     st.wHour, st.wMinute, st.wSecond);
            filename = stamp;
        }
        std::wstring full_path = JoinPath(out_dir, filename + L"." + ext);
        std::wstring base = filename;
        for (int counter = 1; FileExists(full_path); ++counter) {
            filename = base + L" " + std::to_wstring(counter);
            full_path = JoinPath(out_dir, filename + L"." + ext);
        }
        std::wstring bitrate_val = ui::ComboValue(cmb_bitrate_);
        bitrate_val = bitrate_val.substr(0, bitrate_val.find(L' '));
        int err = 0;
        bool ok = audio.StartRecording(full_path, ext, bitrate_val,
                                       _wtoi(ui::ComboValue(cmb_rate_).c_str()),
                                       ui::ComboValue(cmb_channels_), sources_, Checked(chk_separate_),
                                       _wtoi(ui::ComboValue(cmb_flac_level_).c_str()), &err);
        if (ok) {
            RecordingStarted();
        } else {
            ui::MsgBox(DialogParent(),
                           Format(tr("Erro BASS {erro} ao iniciar a gravação."),
                                  {{L"erro", std::to_wstring(err)}}),
                           tr("Erro"), MB_ICONERROR | MB_OK);
        }
    } else {
        audio.StopRecording();
        if (Checked(chk_tones_)) audio.PlayTone(440, 150);
        ui::SetText(btn_record_, tr("&Gravar"));
        ui::SetText(btn_pause_, tr("&Pausar"));
        bool pause_had_focus = GetFocus() == btn_pause_;
        EnableWindow(btn_pause_, FALSE);
        ToggleUi(true);
        if (pause_had_focus) SetFocus(btn_record_);
        last_recording_.clear();
        for (auto& p : audio.output_files())
            if (FileExists(p)) last_recording_.push_back(p);
        UpdateRecordingButtons();
    }
}

void MainFrame::RecordingStarted() {
    ui::SetText(btn_record_, tr("Para&r"));
    EnableWindow(btn_pause_, TRUE);
    ToggleUi(false);
    UpdateRecordingButtons();
}

bool MainFrame::CanContinue() { return HasRecording() && audio.CanContinue(); }

/* Goes on with the last recording, in the same files, as if it had only been
   paused. */
void MainFrame::OnContinueLast() {
    if (audio.is_recording() || !CanContinue()) return;
    audio.StopMonitor();
    audio.StopPlayback();  // the file being played is the one to continue
    UpdatePlayButton();
    if (Checked(chk_tones_)) audio.PlayTone(880, 150);
    bool continue_had_focus = GetFocus() == btn_continue_;
    int err = 0;
    std::wstring failed;
    if (audio.ContinueRecording(sources_, &err, &failed)) {
        RecordingStarted();
        if (continue_had_focus) SetFocus(btn_record_);
        return;
    }
    if (!failed.empty()) {
        ui::MsgBox(DialogParent(),
                   Format(tr("Não foi possível continuar a gravação. Verifique se o arquivo não "
                             "está aberto em outro programa:\n{arquivo}"),
                          {{L"arquivo", failed}}),
                   tr("Erro"), MB_ICONERROR | MB_OK);
    } else {
        ui::MsgBox(DialogParent(),
                   Format(tr("Erro BASS {erro} ao iniciar a gravação."), {{L"erro", std::to_wstring(err)}}),
                   tr("Erro"), MB_ICONERROR | MB_OK);
    }
}

void MainFrame::OnPauseToggle() {
    if (!audio.is_paused()) {
        audio.PauseRecording();
        if (Checked(chk_tones_)) audio.PlayTone(659, 150);
        ui::SetText(btn_pause_, tr("C&ontinuar"));
    } else {
        audio.ResumeRecording();
        if (Checked(chk_tones_)) audio.PlayTone(880, 150);
        ui::SetText(btn_pause_, tr("&Pausar"));
    }
}

void MainFrame::OnBrowse() {
    std::wstring path;
    if (PickFolder(hwnd_, tr("Selecionar Pasta"), &path)) SetWindowTextW(txt_dir_, path.c_str());
}

/* Folder in use: the one in the edit field, or the default if it is empty. */
std::wstring MainFrame::CurrentOutputDir() {
    std::wstring dir = Trim(ui::GetText(txt_dir_));
    return dir.empty() ? JoinPath(CurrentDir(), tr("Gravações")) : dir;
}

void MainFrame::OnOpenDir() {
    std::wstring out_dir = CurrentOutputDir();
    DWORD err = MakeDirs(out_dir);
    if (err == ERROR_SUCCESS) {
        HINSTANCE r = ShellExecuteW(nullptr, L"open", out_dir.c_str(), nullptr, nullptr,
                                    SW_SHOWNORMAL);
        if ((INT_PTR)r > 32) return;
        err = GetLastError() ? GetLastError() : ERROR_PATH_NOT_FOUND;
    }
    ui::MsgBox(DialogParent(),
                   Format(tr("Não foi possível abrir a pasta:\n{pasta}\n\n{erro}"),
                          {{L"pasta", out_dir}, {L"erro", WinErrorText(err) + L": '" + out_dir + L"'"}}),
                   tr("Erro"), MB_ICONERROR | MB_OK);
}

std::vector<std::wstring> MainFrame::ExistingRecordingFiles() {
    std::vector<std::wstring> files;
    for (auto& p : last_recording_)
        if (FileExists(p)) files.push_back(p);
    return files;
}

bool MainFrame::HasRecording() { return !ExistingRecordingFiles().empty() && !audio.is_recording(); }

/* No owner while the window is in the tray, so the dialog comes to the front. */
HWND MainFrame::DialogParent() { return IsWindowVisible(hwnd_) ? hwnd_ : nullptr; }

void MainFrame::Warn(const std::wstring& message, const std::wstring& title, UINT icon) {
    ui::MsgBox(DialogParent(), message, title, icon | MB_OK);
}

void MainFrame::UpdateRecordingButtons() {
    bool available = HasRecording();
    EnableWindow(btn_play_, available);
    EnableWindow(btn_delete_, available);
    EnableWindow(btn_continue_, CanContinue());
    UpdatePlayButton();
}

void MainFrame::UpdatePlayButton() {
    bool playing = audio.IsPlaying();
    ui::SetText(btn_play_, playing ? tr("Parar Reprod&ução") : tr("Reprod&uzir Gravação"));
    if (playing && !play_timer_) {
        SetTimer(hwnd_, TIMER_PLAY, 300, nullptr);
        play_timer_ = true;
    } else if (!playing && play_timer_) {
        KillTimer(hwnd_, TIMER_PLAY);
        play_timer_ = false;
    }
}

void MainFrame::OnPlayToggle() {
    if (audio.IsPlaying()) {
        audio.StopPlayback();
        UpdatePlayButton();
        return;
    }
    std::vector<std::wstring> files = ExistingRecordingFiles();
    if (files.empty()) {
        last_recording_.clear();
        UpdateRecordingButtons();
        return;
    }
    std::wstring path = files[0];
    if (files.size() > 1) {
        HWND parent = DialogParent();
        std::vector<std::wstring> names;
        for (auto& f : files) names.push_back(BaseName(f));
        int sel = SingleChoice(parent, tr("Escolha o arquivo a reproduzir:"),
                               tr("Reproduzir Gravação"), names, parent == nullptr);
        if (sel < 0) return;
        path = files[sel];
    }
    std::wstring dll = audio.MissingPlaybackPlugin(path);
    if (!dll.empty()) {
        std::wstring base, ext;
        SplitExt(path, &base, &ext);
        Warn(Format(tr("Para reproduzir arquivos {extensao} dentro do programa, "
                        "coloque {dll} na pasta lib do VB Recorder."),
                     {{L"extensao", ext}, {L"dll", dll}}),
              tr("Aviso"), MB_ICONWARNING);
        return;
    }
    int err = 0;
    if (!audio.PlayFile(path, &err)) {
        Warn(Format(tr("Erro BASS {erro} ao reproduzir:\n{arquivo}"),
                     {{L"erro", std::to_wstring(err)}, {L"arquivo", path}}),
              tr("Erro"), MB_ICONERROR);
        return;
    }
    UpdatePlayButton();
}

void MainFrame::OnDeleteRecording() {
    std::vector<std::wstring> files = ExistingRecordingFiles();
    if (files.empty()) {
        last_recording_.clear();
        UpdateRecordingButtons();
        return;
    }
    std::wstring list_text;
    for (size_t i = 0; i < files.size(); ++i) list_text += (i ? L"\n" : L"") + BaseName(files[i]);
    std::wstring msg = Format(ntr("Excluir a gravação?\n\n{lista}",
                                  "Excluir os {n} arquivos da última gravação?\n\n{lista}",
                                  (unsigned long)files.size()),
                              {{L"n", std::to_wstring(files.size())}, {L"lista", list_text}});
    if (!ConfirmYesNo(DialogParent(), msg, tr("Confirmar Exclusão"))) return;
    audio.StopPlayback();
    audio.ForgetLastRecording();
    std::vector<std::wstring> failures;
    for (auto& path : files) {
        if (!DeleteFileW(path.c_str()))
            failures.push_back(BaseName(path) + L": " + WinErrorText(GetLastError()) + L": '" +
                             path + L"'");
    }
    last_recording_.clear();
    UpdateRecordingButtons();
    if (!failures.empty()) {
        std::wstring text = tr("Não foi possível excluir:\n");
        for (size_t i = 0; i < failures.size(); ++i) text += (i ? L"\n" : L"") + failures[i];
        Warn(text, tr("Erro"), MB_ICONERROR);
    }
}

void MainFrame::OnAbout() {
    ui::MsgBox(DialogParent(),
                   Format(tr("VB Recorder v{versao}\nAutor: Vitor Bruski"), {{L"versao", VB_VERSION}}),
                   tr("Sobre"), MB_OK | MB_ICONINFORMATION);
}

void MainFrame::ToggleUi(bool enable) {
    HWND focus = GetFocus();
    // The sources stay available: they can be added, edited and removed while
    // recording.
    for (HWND ctrl : {cmb_format_, cmb_bitrate_, cmb_flac_level_, cmb_rate_, cmb_channels_,
                      chk_separate_, txt_dir_, btn_browse_, txt_filename_, btn_settings_})
        EnableWindow(ctrl, enable);
    // Focus cannot stay on a disabled control: it goes to the record button.
    if (focus && IsChild(hwnd_, focus) && !IsWindowEnabled(focus)) SetFocus(btn_record_);
}

bool MainFrame::OnClose() {
    if (audio.is_recording() &&
        !ConfirmYesNo(DialogParent(),
                      tr("Há uma gravação em andamento.\n\nDeseja mesmo fechar o VB Recorder?"),
                      tr("Confirmar Saída")))
        return false;
    if (play_timer_) KillTimer(hwnd_, TIMER_PLAY);
    play_timer_ = false;
    audio.StopRecording();
    SaveSettings();
    audio.Cleanup();
    RemoveTrayIcon();
    for (int id = HK_REC; id <= HK_EXIT; ++id) UnregisterHotKey(hwnd_, id);
    DestroyWindow(hwnd_);
    PostQuitMessage(0);
    return true;
}

/* ---- Tray ---- */

void MainFrame::AddTrayIcon() {
    if (tray_added_) return;
    if (!tray_icon_ &&
        FAILED(LoadIconWithScaleDown(nullptr, IDI_INFORMATION, GetSystemMetrics(SM_CXSMICON),
                                     GetSystemMetrics(SM_CYSMICON), &tray_icon_)))
        tray_icon_ = LoadIconW(nullptr, IDI_INFORMATION);
    NOTIFYICONDATAW nid = {sizeof(nid)};
    nid.hWnd = hwnd_;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = tray_icon_;
    lstrcpynW(nid.szTip, L"VB Recorder", ARRAYSIZE(nid.szTip));
    tray_added_ = Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
}

void MainFrame::RemoveTrayIcon() {
    if (!tray_added_) return;
    NOTIFYICONDATAW nid = {sizeof(nid)};
    nid.hWnd = hwnd_;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    tray_added_ = false;
}

void MainFrame::AddSubmenu(HMENU parent, const std::wstring& label, int index, HWND combo) {
    HMENU sub = CreatePopupMenu();
    std::vector<std::wstring> items = ui::ComboItems(combo);
    std::wstring value = ui::ComboValue(combo);
    UINT first = MENU_COMBO + index * 100;
    for (size_t i = 0; i < items.size(); ++i) {
        AppendMenuW(sub, MF_STRING, first + i, items[i].c_str());
        MENUITEMINFOW mii = {sizeof(mii)};
        mii.fMask = MIIM_FTYPE;
        mii.fType = MFT_STRING | MFT_RADIOCHECK;
        SetMenuItemInfoW(sub, first + (UINT)i, FALSE, &mii);
        if (items[i] == value) CheckMenuItem(sub, first + (UINT)i, MF_BYCOMMAND | MF_CHECKED);
    }
    AppendMenuW(parent, MF_POPUP, (UINT_PTR)sub, label.c_str());
}

void MainFrame::ShowTrayMenu() {
    HMENU menu = CreatePopupMenu();
    menu_combos_ = {cmb_format_, cmb_bitrate_, cmb_flac_level_, cmb_rate_, cmb_channels_};
    if (!audio.is_recording()) {
        AppendMenuW(menu, MF_STRING, MENU_RECORD, tr("Gravar").c_str());
        if (CanContinue())
            AppendMenuW(menu, MF_STRING, MENU_CONTINUE, tr("Continuar Última Gravação").c_str());
        if (HasRecording()) {
            std::wstring label_play = audio.IsPlaying() ? tr("Parar Reprodução") : tr("Reproduzir Gravação");
            AppendMenuW(menu, MF_STRING, MENU_PLAY, label_play.c_str());
            AppendMenuW(menu, MF_STRING, MENU_DELETE, tr("Excluir Gravação").c_str());
        }
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AddSubmenu(menu, tr("Formato de Saída"), 0, cmb_format_);
        std::wstring fmt = ui::ComboValue(cmb_format_);
        if (fmt == L"mp3" || fmt == L"ogg" || fmt == L"opus")
            AddSubmenu(menu, tr("Bitrate"), 1, cmb_bitrate_);
        if (fmt == L"flac") AddSubmenu(menu, tr("Compressão FLAC"), 2, cmb_flac_level_);
        AddSubmenu(menu, tr("Taxa de Amostragem"), 3, cmb_rate_);
        AddSubmenu(menu, tr("Canais"), 4, cmb_channels_);
        if (sources_.size() >= 2) {
            AppendMenuW(menu, MF_STRING | (Checked(chk_separate_) ? MF_CHECKED : 0), MENU_SEPARATE,
                        tr("Gravar fontes em arquivos separados").c_str());
        }
        AppendMenuW(menu, MF_STRING | (Checked(chk_tones_) ? MF_CHECKED : 0), MENU_TONES,
                    tr("Reproduzir tons ao iniciar ou parar gravação").c_str());
    } else {
        std::wstring label_pause = audio.is_paused() ? tr("Continuar") : tr("Pausar");
        AppendMenuW(menu, MF_STRING, MENU_PAUSE, label_pause.c_str());
        AppendMenuW(menu, MF_STRING, MENU_STOP, tr("Parar").c_str());
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, MENU_SHOW, tr("Mostrar janela principal").c_str());
    AppendMenuW(menu, MF_STRING, MENU_EXIT, tr("Sair").c_str());

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd_);
    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y, 0,
                              hwnd_, nullptr);
    PostMessageW(hwnd_, WM_NULL, 0, 0);
    DestroyMenu(menu);
    if (cmd) OnTrayCommand(cmd);
}

void MainFrame::OnTrayCommand(UINT cmd) {
    switch (cmd) {
        case MENU_RECORD:
        case MENU_STOP: OnRecordToggle(); return;
        case MENU_PLAY: OnPlayToggle(); return;
        case MENU_CONTINUE: OnContinueLast(); return;
        case MENU_DELETE: OnDeleteRecording(); return;
        case MENU_PAUSE: OnPauseToggle(); return;
        case MENU_SHOW: ShowMain(); return;
        case MENU_EXIT: OnClose(); return;
        case MENU_SEPARATE:
            SetChecked(chk_separate_, !Checked(chk_separate_));
            SaveSettings();
            return;
        case MENU_TONES:
            SetChecked(chk_tones_, !Checked(chk_tones_));
            SaveSettings();
            return;
    }
    if (cmd >= MENU_COMBO) {
        int index = (cmd - MENU_COMBO) / 100, item = (cmd - MENU_COMBO) % 100;
        if (index >= (int)menu_combos_.size()) return;
        HWND combo = menu_combos_[index];
        std::vector<std::wstring> items = ui::ComboItems(combo);
        if (item >= (int)items.size()) return;
        ui::ComboSetValue(combo, items[item]);
        SaveSettings();
        if (combo == cmb_format_) OnFormatChange();
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc = {sizeof(icc),
                                ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES |
                                    ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icc);
    ui::Init(instance);
    // Right after an update: waits for the old version to close, and removes
    // what it left behind.
    FinishUpdate(GetCommandLineW());

    // Settings come first: they define the language of the messages,
    // including the one saying the program is already running.
    std::unique_ptr<MainFrame> frame(new MainFrame());
    i18n::SetLanguage(frame->config.Get(L"idioma"));

    g_instance_mutex = CreateMutexW(nullptr, FALSE, INSTANCE_MUTEX);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        ui::MsgBox(nullptr, tr("O VB Recorder já está em execução."), tr("Aviso"),
                       MB_ICONWARNING | MB_OK);
        return 0;
    }

    if (!frame->audio.Load()) return 1;
    frame->audio.SetBuffer(frame->config.GetBuffer());
    if (!frame->Create()) return 1;
    frame->ShowAtStart();
    if (frame->config.GetBoolean(L"verificar_atualizacoes", true)) frame->StartUpdateCheck();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        HWND main = frame->hwnd();
        if (IsWindow(main)) {
            if (ui::IsKeyFor(main, msg) && frame->OnCharHook(msg)) continue;
            if (IsDialogMessageW(main, &msg)) continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (g_instance_mutex) CloseHandle(g_instance_mutex);
    CoUninitialize();
    return (int)msg.wParam;
}
