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
#include <set>
#include <string>
#include <vector>

#include "audio_engine.h"
#include "background.h"
#include "config.h"
#include "dialogs.h"
#include "fx_dialogs.h"
#include "hotkeys.h"
#include "startup.h"
#include "i18n.h"
#include "install.h"
#include "m4a_writer.h"
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
    IDC_BITS,
    IDC_CHANNELS,
    IDC_SOURCES,
    IDC_ADD,
    IDC_EDIT,
    IDC_REMOVE,
    IDC_SEPARATE,
    IDC_TONES,
    IDC_DURATION,
    IDC_FILENAME,
    IDC_DIR,
    IDC_BROWSE,
    IDC_OPEN_DIR,
    IDC_SCHEDULE,
    IDC_SETTINGS,
    IDC_ABOUT,
    IDC_MIN,
    IDC_CLOSE,
    IDC_TAGS,
    IDC_SILENCE,
};

enum { HK_REC = 1, HK_PAUSE, HK_STOP, HK_PLAY, HK_DELETE, HK_OPEN_DIR, HK_SHOW, HK_EXIT };

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
    MENU_TAGS,
    MENU_SCHEDULE,
    MENU_OPEN_DIR,
    MENU_SETTINGS,
    MENU_COMBO = 3000,  // + 100 * combo box + item
};

/* Marks the running instance; a second one only warns and quits, unless
   the settings allow more than one. */
const wchar_t* const INSTANCE_MUTEX = L"VBRecorder_Mutex";
HANDLE g_instance_mutex = nullptr;

const UINT WM_TRAY = WM_APP + 1;
const UINT WM_UPDATE_FOUND = WM_APP + 3;  // lParam: an UpdateInfo to delete
const UINT WM_CHECK_FOCUS = WM_APP + 4;
const UINT_PTR TIMER_PLAY = 1;
const UINT_PTR TIMER_COUNTDOWN = 2;
const UINT_PTR TIMER_DURATION = 3;
const UINT_PTR TIMER_SCHEDULE = 4;  // every second: is a scheduled recording due?
const UINT_PTR TIMER_FILES = 5;     // every second: were the last recording's files deleted?
const UINT_PTR TIMER_DETECT = 6;    // the silence detection: sound, or silence?
/* Low beeps before the recording starts, one per second, like the start of a race. */
const int COUNTDOWN_BEEPS = 3;

/* Sample rates. Above 48000 only for the formats without loss: MP3 does not go
   beyond it, and OGG and OPUS would only spend more space. WMA has the ones
   Windows offers. */
const std::vector<std::wstring> SAMPLE_RATES = {
    L"8000",  L"11025", L"12000", L"16000",  L"22050",  L"24000",  L"32000",  L"44100",
    L"48000", L"64000", L"88200", L"96000", L"176400", L"192000", L"352800", L"384000"};
const int LOSSY_MAX_RATE = 48000;

/* Bit depth of wav, aiff and flac; wav and aiff also in floating point. In
   wma, 16 is WMA and 24 is WMA Pro. */
const wchar_t* const BITS_FLOAT_TEXT = L"32 float";

const wchar_t* const KBPS = L" KBPS";

/* The bitrates of each format, in kbps, at the recording's sample rate and
   channels: only those its encoder takes. */
std::vector<int> Mp3Bitrates(int rate) {
    // By the MPEG version the rate belongs to. Outside these, LAME would
    // quietly lower the sample rate of the file.
    if (rate >= 32000) return {32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};
    if (rate >= 16000) return {8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160};
    return {8, 16, 24, 32, 40, 48, 56, 64};
}

std::vector<int> OggBitrates(int rate, int chans) {
    // Vorbis takes a range of bitrates for each rate and channel count;
    // outside it, the recording does not start. Measured with BASSenc_OGG.
    struct Range {
        int rate, mono_min, mono_max, stereo_min, stereo_max;
    };
    static const Range ranges[] = {
        {8000, 8, 32, 16, 80},     {11025, 16, 48, 16, 96},   {12000, 16, 48, 16, 96},
        {16000, 16, 96, 24, 192},  {22050, 16, 80, 32, 160},  {24000, 16, 80, 32, 160},
        {32000, 32, 160, 48, 320}, {44100, 32, 224, 48, 500}, {48000, 32, 224, 48, 500}};
    static const int values[] = {8,   16,  24,  32,  48,  64,  80,  96,  112,
                                 128, 160, 192, 224, 256, 320, 384, 448, 500};
    std::vector<int> result;
    for (const Range& r : ranges) {
        if (r.rate != rate) continue;
        int lo = chans == 1 ? r.mono_min : r.stereo_min, hi = chans == 1 ? r.mono_max : r.stereo_max;
        for (int v : values)
            if (v >= lo && v <= hi) result.push_back(v);
    }
    return result;
}

std::vector<int> OpusBitrates(int chans) {
    // Up to 256 per channel.
    std::vector<int> all = {6,   8,   12,  16,  24,  32,  48,  64,  80,  96,
                            112, 128, 160, 192, 224, 256, 320, 384, 448, 510};
    if (chans == 1) all.erase(std::upper_bound(all.begin(), all.end(), 256), all.end());
    return all;
}

/* The entry of the list closest in number to the one given, so a value that
   is no longer offered becomes the nearest one instead of a far one. */
std::wstring Nearest(const std::vector<std::wstring>& items, const std::wstring& value) {
    if (items.empty()) return std::wstring();
    if (std::find(items.begin(), items.end(), value) != items.end()) return value;
    if (value.empty()) return std::wstring();
    int want = _wtoi(value.c_str());
    std::wstring best = items.front();
    for (auto& item : items)
        if (std::abs(_wtoi(item.c_str()) - want) < std::abs(_wtoi(best.c_str()) - want)) best = item;
    return best;
}

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
    void OnTags();
    void OnSchedule();
    void OnScheduleTick();
    bool OfferUpdate(const UpdateInfo& info, HWND parent);
    bool CheckUpdateNow(HWND parent);
    static DWORD WINAPI UpdateCheckThread(void* window);
    void OnFormatChange();
    int BitsValue();
    void LoadSettings();
    void SaveSettings();
    void OnRecordToggle();
    /* from: the moment it starts at (QueryPerformanceCounter), captured
       before; -1 is the pre-recording of the settings. */
    bool StartRecordingNow(bool tone, LONGLONG from = -1);
    void StopNow(bool tone);
    /* Waiting before a recording: the countdown. */
    bool Waiting() const { return countdown_ != 0; }
    /* Sources and format the capture before a recording follows. */
    void UpdateStandby();
    LONGLONG PrerollFrom();
    void OnSilence();
    /* The silence detection: it acts once the recording has started. */
    bool DetectionReady();
    void StartDetection();
    void OnDetectTick();
    bool ContinueNow(bool tone, LONGLONG from);
    void StartCountdown();
    void OnCountdownTick();
    void CancelCountdown();
    void OnPauseToggle();
    void RecordingStarted();
    void UpdateShutdownBlock();
    bool CanContinue();
    void OnContinueLast();
    void OnBrowse();
    bool ReadDuration();
    bool BackgroundReady();
    void OnDurationTick();
    std::wstring CurrentOutputDir();
    void OnOpenDir();
    std::vector<std::wstring> ExistingRecordingFiles();
    bool HasRecording();
    HWND DialogParent();
    void Warn(const std::wstring& message, const std::wstring& title, UINT icon);
    void UpdateRecordingButtons();
    void OnFilesTick();
    void UpdatePlayButton();
    void OnPlayToggle();
    void OnDeleteRecording();
    void OnAbout();
    void ToggleUi(bool enable);
    void EnableControl(HWND ctrl, bool enable);
    bool OnClose();

    void AddTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu();
    void FromTray(void (MainFrame::*open)());
    bool FocusTrayDialog();
    void AddButtonItem(HMENU menu, UINT id, HWND button);
    bool tray_dialog_ = false;   // a window opened from the tray, with no owner, is open
    HWND tray_back_ = nullptr;   // what was in front when the tray menu opened
    void AddSubmenu(HMENU parent, const std::wstring& label, int index, HWND combo);
    void OnTrayCommand(UINT cmd);
    std::pair<int, int> OutputFormat();
    std::wstring ChannelsValue();

    HWND hwnd_ = nullptr;
    std::unique_ptr<ui::Box> root_;
    std::vector<Source> sources_;
    std::vector<std::wstring> last_recording_;
    bool play_timer_ = false;
    int countdown_ = 0;  // beeps played so far; 0 when there is no countdown
    // The silence detection of the recording: recording, or held in a
    // silence. since: when it started recording or being held.
    enum { DETECT_OFF, DETECT_ON, DETECT_HELD };
    int detect_ = DETECT_OFF;
    LONGLONG detect_since_ = 0;
    SilenceSettings silence_;
    bool confirming_delete_ = false;
    // What the play, delete and continue buttons last showed, so the check
    // of the files touches them only when that changes.
    int files_state_ = -1;
    bool tray_added_ = false;
    UINT taskbar_created_ = 0;
    HICON tray_icon_ = nullptr;
    std::vector<HWND> menu_combos_;

    HWND btn_record_, btn_pause_, btn_continue_, btn_play_, btn_delete_;
    HWND lbl_format_, cmb_format_, lbl_bitrate_, cmb_bitrate_, lbl_flac_level_, cmb_flac_level_;
    HWND lbl_rate_, cmb_rate_, lbl_bits_, cmb_bits_, lbl_channels_, cmb_channels_;
    HWND btn_tags_;
    HWND lbl_sources_, lst_fontes_, btn_add_, btn_edit_, btn_rem_;
    HWND chk_separate_, chk_tones_;
    HWND lbl_duration_, txt_duration_;
    // The recording stops by itself when it reaches this many seconds; 0 is
    // no limit. Counted from where it started, or was continued, without the
    // pauses.
    double duration_limit_ = 0, duration_start_ = 0;
    HWND lbl_filename_, txt_filename_, lbl_dir_, txt_dir_, btn_browse_, btn_open_dir_;
    HWND btn_schedule_, btn_silence_, btn_settings_, btn_about_, btn_min_, btn_close_;

    // Scheduled recordings. fired_ holds the day and minute each one last
    // started, so it starts only once in that minute; removed_ the ones
    // that left the list by themselves, which an open list dialog must not
    // bring back.
    std::vector<Schedule> schedules_;
    int next_schedule_id_ = 1;
    std::map<int, std::wstring> fired_;
    std::set<int> removed_;
    int schedule_duration_ = 0;  // of the schedule starting now, if it has one
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
    // The pre-recording starts with the program.
    UpdateStandby();
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
                    case IDC_SCHEDULE: OnSchedule(); return TRUE;
                    case IDC_SETTINGS: OnSettings(); return TRUE;
                    case IDC_TAGS: OnTags(); return TRUE;
                    case IDC_SILENCE: OnSilence(); return TRUE;
                    case IDC_ABOUT: OnAbout(); return TRUE;
                    case IDC_MIN: Iconize(); return TRUE;
                    case IDC_CLOSE: OnClose(); return TRUE;
                }
            }
            if ((id == IDC_FORMAT || id == IDC_RATE || id == IDC_CHANNELS || id == IDC_BITS) &&
                code == CBN_SELCHANGE)
                OnFormatChange();
            return TRUE;  // IDOK and IDCANCEL (Enter and Esc) do not close the window
        }
        case WM_ACTIVATE:
            // Coming back to the window, Windows gives the focus back to the
            // control that had it, which may have been disabled meanwhile (the
            // delete button, after deleting from the tray, for example). It is
            // checked once that is done.
            if (LOWORD(wp) != WA_INACTIVE) PostMessageW(hwnd_, WM_CHECK_FOCUS, 0, 0);
            return FALSE;
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
            if (wp == TIMER_COUNTDOWN) OnCountdownTick();
            if (wp == TIMER_DURATION) OnDurationTick();
            if (wp == TIMER_SCHEDULE) OnScheduleTick();
            if (wp == TIMER_FILES) OnFilesTick();
            if (wp == TIMER_DETECT) OnDetectTick();
            return TRUE;
        case WM_TRAY:
            if (LOWORD(lp) == WM_LBUTTONDBLCLK) ShowMain();
            else if (LOWORD(lp) == WM_RBUTTONUP) ShowTrayMenu();
            return TRUE;
        case WM_CHECK_FOCUS: {
            HWND focus = GetFocus();
            if (GetActiveWindow() == hwnd_ && !IsIconic(hwnd_) &&
                (!focus || (IsChild(hwnd_, focus) && !IsWindowEnabled(focus))))
                SetFocus(btn_record_);
            return TRUE;
        }
        case WM_UPDATE_FOUND: {
            // Found when the program started; the window is already open.
            std::unique_ptr<UpdateInfo> info((UpdateInfo*)lp);
            if (!audio.is_recording() && !Waiting() && OfferUpdate(*info, DialogParent()))
                OnClose();
            return TRUE;
        }
        case WM_QUERYENDSESSION:
            // Windows wants to shut down or log off: not while recording,
            // paused or not. Windows then lists the program with the reason,
            // and the user can still shut down anyway; WM_ENDSESSION below
            // closes the files properly in that case.
            if (audio.is_recording()) {
                SetWindowLongPtrW(hwnd_, DWLP_MSGRESULT, FALSE);
                return TRUE;
            }
            return FALSE;
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
    std::vector<std::wstring> formats = {L"mp3", L"wav", L"aiff", L"ogg", L"opus", L"flac"};
    // WMA only where Windows has its codec (not in the N editions without the
    // Media Feature Pack).
    if (!audio.WmaBitrates(44100, 2, 16).empty()) formats.push_back(L"wma");
    // AAC with the encoder of Windows, likewise.
    if (M4aWriter::Available()) formats.push_back(L"m4a");
    labeled_combo(&lbl_format_, &cmb_format_, IDC_FORMAT, formats);
    labeled_combo(&lbl_bitrate_, &cmb_bitrate_, IDC_BITRATE, {});
    labeled_combo(&lbl_flac_level_, &cmb_flac_level_, IDC_FLAC,
                  {L"0", L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8"});
    labeled_combo(&lbl_rate_, &cmb_rate_, IDC_RATE, SAMPLE_RATES);
    labeled_combo(&lbl_bits_, &cmb_bits_, IDC_BITS, {});
    // The channel names are translated: Retranslate fills them in.
    labeled_combo(&lbl_channels_, &cmb_channels_, IDC_CHANNELS, {});

    // The tags of the files, after everything about their format.
    btn_tags_ = Add(WC_BUTTONW, BUTTON, IDC_TAGS);
    vbox->Add(btn_tags_, 0, ui::ALL, S(5));

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

    lbl_duration_ = Add(WC_STATICW, 0, 0);
    txt_duration_ = Add(WC_EDITW, EDIT, IDC_DURATION, WS_EX_CLIENTEDGE);
    // Right after the duration: each schedule has a duration of its own.
    btn_schedule_ = Add(WC_BUTTONW, BUTTON, IDC_SCHEDULE);
    btn_silence_ = Add(WC_BUTTONW, BUTTON, IDC_SILENCE);
    {
        auto hb = HBox();
        hb->Add(lbl_duration_, 0, ui::CENTER_V | ui::RIGHT, S(5));
        hb->Add(txt_duration_, 0, ui::RIGHT, S(10), SIZE{S(90), 0});
        hb->Add(btn_schedule_, 0, ui::RIGHT, S(5));
        hb->Add(btn_silence_, 0, 0, 0);
        vbox->Add(std::move(hb), 0, ui::ALL, S(5));
    }

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
    ui::SetText(lbl_format_, tr("Formato de &Saída:"));
    ui::SetText(lbl_bitrate_, tr("&Bitrate:"));
    ui::SetText(lbl_flac_level_, tr("Compressão &FLAC:"));
    ui::SetText(lbl_rate_, tr("&Taxa de Amostragem:"));
    ui::SetText(lbl_bits_, tr("Profundidade de &Bits:"));
    ui::SetText(lbl_channels_, tr("&Canais:"));
    // Same order as the values in the .ini: mono, stereo.
    int channels = (int)SendMessageW(cmb_channels_, CB_GETCURSEL, 0, 0);
    ui::ComboSetItems(cmb_channels_, {tr("mono"), tr("estéreo")});
    SendMessageW(cmb_channels_, CB_SETCURSEL, channels == 0 ? 0 : 1, 0);
    ui::SetText(btn_tags_, tr("Configurar &Tags..."));
    ui::SetText(lbl_sources_, tr("Fontes de &Gravação:"));
    ui::SetText(lbl_duration_, tr("Duração da Gravação (Opcio&nal):"));
    ui::SetToolTip(hwnd_, txt_duration_,
                   tr("Em branco, grava sem limite. 1:30:00 é uma hora e meia, 45:00 são 45 minutos."));
    ui::SetText(lbl_filename_, tr("&Nome do Arquivo:"));
    ui::SetText(lbl_dir_, tr("&Diretório de Saída:"));

    ui::SetText(btn_add_, tr("Adici&onar"));
    ui::SetText(btn_edit_, tr("&Editar"));
    ui::SetText(btn_rem_, tr("Remo&ver"));
    ui::SetText(btn_browse_, tr("&Procurar..."));
    ui::SetText(btn_open_dir_, tr("Abr&ir Pasta de Gravações"));
    ui::SetText(btn_schedule_, tr("Agenda&mento de Gravações..."));
    ui::SetText(btn_silence_, tr("Detecção de &Silêncio..."));
    ui::SetText(btn_settings_, tr("Configura&ções..."));
    ui::SetText(btn_about_, tr("S&obre"));
    ui::SetText(btn_min_, tr("&Minimizar"));
    ui::SetText(btn_close_, tr("&Fechar"));
    ui::SetText(btn_delete_, tr("E&xcluir Gravação"));
    ui::SetText(btn_continue_, tr("Continuar Ú&ltima Gravação"));

    ui::SetText(chk_separate_, tr("Gravar fontes em ar&quivos separados"));
    ui::SetText(chk_tones_, tr("Reprodu&zir tons ao iniciar ou parar gravação"));

    ui::SetText(btn_record_, audio.is_recording() || Waiting() ? tr("P&arar") : tr("Gr&avar"));
    ui::SetText(btn_pause_, audio.is_paused() ? tr("Contin&uar") : tr("Pa&usar"));
    UpdatePlayButton();

    RefreshDeviceNames();
    UpdateShutdownBlock();  // the reason shown by Windows is in the new language
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
        // Background audio shows the name of its file or folder.
        std::wstring label = s.type == L"file"  ? Format(tr("Áudio de Fundo: {nome}"), {{L"nome", BaseName(s.name)}})
                             : s.type == L"url" ? Format(tr("URL de Stream: {endereco}"), {{L"endereco", s.name}})
                                                : s.name;
        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = ListView_GetItemCount(lst_fontes_);
        item.pszText = const_cast<wchar_t*>(label.c_str());
        ListView_InsertItem(lst_fontes_, &item);
    }
    ListView_SetColumnWidth(lst_fontes_, 0, LVSCW_AUTOSIZE);
    // An empty list has nothing to edit or remove; the focus, if it was on
    // one of them, goes to Add instead of nowhere.
    bool any = !sources_.empty();
    HWND focus = GetFocus();
    if (!any && (focus == btn_edit_ || focus == btn_rem_)) SetFocus(btn_add_);
    EnableWindow(btn_edit_, any);
    EnableWindow(btn_rem_, any);
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
    if (FocusTrayDialog()) return;
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
        {HK_PLAY, L"reproduzir"}, {HK_DELETE, L"excluir"}, {HK_OPEN_DIR, L"abrir_pasta"},
        {HK_SHOW, L"mostrar"}, {HK_EXIT, L"fechar"}};
    for (auto& k : keys_map) {
        UINT mods = 0, vk = 0;
        ParseHotkey(config.GetKey(k.second), &mods, &vk);
        if (vk) RegisterHotKey(hwnd_, k.first, mods, vk);
    }
}

void MainFrame::OnHotkey(int id) {
    // During the countdown, or waiting for sound, only stopping does
    // something: it cancels it.
    if (Waiting()) {
        if (id == HK_STOP) OnRecordToggle();
        else if (id == HK_SHOW) ShowMain();
        else if (id == HK_OPEN_DIR) OnOpenDir();
        else if (id == HK_EXIT) OnClose();
        return;
    }
    if (id == HK_REC && !audio.is_recording()) OnRecordToggle();
    else if (id == HK_PAUSE && audio.is_recording()) OnPauseToggle();
    // Stopped, the same hotkey continues the last recording.
    else if (id == HK_PAUSE && CanContinue()) OnContinueLast();
    else if (id == HK_STOP && audio.is_recording()) OnRecordToggle();
    else if (id == HK_PLAY && (audio.IsPlaying() || HasRecording())) OnPlayToggle();
    // Not while a dialog of the program is open, where it would pile up.
    else if (id == HK_DELETE && HasRecording() && IsWindowEnabled(hwnd_)) OnDeleteRecording();
    else if (id == HK_OPEN_DIR) OnOpenDir();
    else if (id == HK_SHOW) ShowMain();
    else if (id == HK_EXIT) OnClose();
}

std::pair<int, int> MainFrame::OutputFormat() {
    std::wstring rate = ui::ComboValue(cmb_rate_);
    if (rate.empty() || rate.find_first_not_of(L"0123456789") != std::wstring::npos)
        return {44100, 2};
    return {_wtoi(rate.c_str()), ChannelsValue() == L"mono" ? 1 : 2};
}

/* "mono" or "stereo", as in the .ini, whatever the language of the list. */
std::wstring MainFrame::ChannelsValue() {
    return SendMessageW(cmb_channels_, CB_GETCURSEL, 0, 0) == 0 ? L"mono" : L"stereo";
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
        UpdateStandby();
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
                // Same source: the volume, balance, equalizer and noise remover
                // change right away.
                audio.SetSourceVolume(now, now.volume);
                audio.SetSourcePan(now, now.pan);
                audio.SetSourceFx(now);
            } else {
                if (!AddToRecording(now)) return;
                audio.RemoveLiveSource(src);
            }
        }
        sources_[sel] = now;
        RefreshList();
        SaveSettings();
        UpdateStandby();
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
    UpdateStandby();
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
    {
        UpdateDialog offer(info);
        if (offer.ShowModal(parent) != IDOK) return false;
    }
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

/* ---- Scheduled recordings ---- */

void MainFrame::OnSchedule() {
    removed_.clear();
    ScheduleListDialog dlg(schedules_);
    if (dlg.ShowModal(DialogParent()) != IDOK) return;
    std::vector<Schedule> list;
    for (auto& s : dlg.result) {
        if (removed_.count(s.id)) continue;  // started meanwhile, and left the list
        list.push_back(s);
        if (!list.back().id) list.back().id = next_schedule_id_++;
    }
    schedules_ = list;
    SaveSettings();
}

/* At its time, a schedule starts the recording as the Record button does,
   with the countdown and the tones if they are on. One already recording is
   left alone: the schedule waits for its next day. */
void MainFrame::OnScheduleTick() {
    SYSTEMTIME now;
    GetLocalTime(&now);
    for (size_t i = 0; i < schedules_.size(); ++i) {
        Schedule s = schedules_[i];
        std::wstring key = ScheduleDueKey(s, now);
        if (key.empty() || fired_[s.id] == key) continue;
        fired_[s.id] = key;
        if (audio.is_recording() || Waiting()) continue;
        schedule_duration_ = s.duration;
        OnRecordToggle();
        schedule_duration_ = 0;
        if (!audio.is_recording() && !Waiting()) return;  // it did not start: said why
        if (s.remove_after) {
            schedules_.erase(schedules_.begin() + i);
            removed_.insert(s.id);
            SaveSettings();
        }
        return;
    }
}

/* The tags are kept as typed, with their variables; they apply from the
   next recording on. */
void MainFrame::OnTags() {
    TagsDialog dlg(config.GetTags());
    if (dlg.ShowModal(DialogParent()) != IDOK) return;
    config.SetTags(dlg.result);
    config.Save();
}

void MainFrame::OnSettings() {
    SettingsDialog dlg(config, [this] { RegisterAllHotkeys(); },
                       [this](HWND parent) { return CheckUpdateNow(parent); });
    int result = dlg.ShowModal(DialogParent());
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
    config.SetConfig(L"multiplas_instancias", dlg.multiple);
    config.SetConfig(L"verificar_atualizacoes", dlg.check_updates);
    if (dlg.start_with_windows != config.GetBoolean(L"iniciar_com_windows"))
        SetStartWithWindows(dlg.start_with_windows);
    config.SetConfig(L"iniciar_com_windows", dlg.start_with_windows);
    config.SetConfig(L"contagem_regressiva", dlg.countdown);
    config.SetConfig(L"pre_gravacao", dlg.preroll);
    config.SetConfig(L"buffer", dlg.BufferValue());
    config.SetConfig(L"dividir", dlg.split_text);
    config.Save();

    audio.SetBuffer(dlg.BufferValue());
    UpdateStandby();
    ApplyAlwaysOnTop();
    if (new_language != old_language) {
        i18n::SetLanguage(new_language);
        Retranslate();
    }
}

/* Each list offers only what the format takes with the other choices: the
   bit depths go by the format (and the channels, in wma), the sample rates by
   the format and bits, and the bitrates by all of them. A choice that is no
   longer offered becomes the nearest one. */
void MainFrame::OnFormatChange() {
    std::wstring fmt = ui::ComboValue(cmb_format_);
    int chans = ChannelsValue() == L"mono" ? 1 : 2;
    bool needs_br = fmt == L"mp3" || fmt == L"ogg" || fmt == L"opus" || fmt == L"wma" || fmt == L"m4a";
    ui::Show(lbl_bitrate_, needs_br);
    ui::Show(cmb_bitrate_, needs_br);
    ui::Show(lbl_flac_level_, fmt == L"flac");
    ui::Show(cmb_flac_level_, fmt == L"flac");

    bool needs_bits = fmt == L"wav" || fmt == L"aiff" || fmt == L"flac" || fmt == L"wma";
    ui::Show(lbl_bits_, needs_bits);
    ui::Show(cmb_bits_, needs_bits);
    {
        std::vector<std::wstring> depths;
        if (fmt == L"wma") depths = {L"16"};
        else depths = {L"8", L"16", L"24", L"32"};
        if (fmt == L"wav" || fmt == L"aiff") depths.push_back(BITS_FLOAT_TEXT);
        // WMA Pro, at 24 bits, is stereo only.
        if (fmt == L"wma" && !audio.WmaBitrates(44100, chans, 24).empty()) depths.push_back(L"24");
        std::wstring cur = ui::ComboValue(cmb_bits_);
        ui::ComboSetItems(cmb_bits_, depths);
        ui::ComboSetValue(cmb_bits_, cur.empty() ? L"16" : Nearest(depths, cur));
    }
    int bits = BitsValue();
    {
        std::vector<std::wstring> rates;
        for (auto& r : SAMPLE_RATES) {
            int rate = _wtoi(r.c_str());
            const auto& aac = M4aWriter::Rates();
            bool ok = fmt == L"wma"    ? !audio.WmaBitrates(rate, chans, bits).empty()
                      : fmt == L"m4a"  ? std::find(aac.begin(), aac.end(), rate) != aac.end()
                      : fmt == L"ogg"  ? !OggBitrates(rate, chans).empty()
                      : needs_br       ? rate <= LOSSY_MAX_RATE
                                       : true;
            if (ok) rates.push_back(r);
        }
        std::wstring cur = ui::ComboValue(cmb_rate_);
        ui::ComboSetItems(cmb_rate_, rates);
        ui::ComboSetValue(cmb_rate_, cur.empty() ? L"44100" : Nearest(rates, cur));
    }
    if (needs_br) {
        int rate = _wtoi(ui::ComboValue(cmb_rate_).c_str());
        std::vector<int> kbps;
        if (fmt == L"mp3") kbps = Mp3Bitrates(rate);
        else if (fmt == L"ogg") kbps = OggBitrates(rate, chans);
        else if (fmt == L"opus") kbps = OpusBitrates(chans);
        else if (fmt == L"m4a") kbps = M4aWriter::Bitrates();
        else {
            // Windows also has some a single kbps below another (191 next to
            // 192): only the round one is offered.
            for (DWORD b : audio.WmaBitrates(rate, chans, bits)) {
                int k = (int)(b / 1000);
                if (kbps.empty() || kbps.back() != k) kbps.push_back(k);
            }
            std::vector<int> round;
            for (size_t i = 0; i < kbps.size(); ++i)
                if (i + 1 == kbps.size() || kbps[i + 1] != kbps[i] + 1) round.push_back(kbps[i]);
            kbps = round;
        }
        std::vector<std::wstring> items;
        for (int k : kbps) items.push_back(std::to_wstring(k) + KBPS);
        std::wstring cur = ui::ComboValue(cmb_bitrate_);
        ui::ComboSetItems(cmb_bitrate_, items);
        ui::ComboSetValue(cmb_bitrate_, Nearest(items, cur.empty() ? L"128" + std::wstring(KBPS) : cur));
    }
    Layout();
    // The capture before recording follows the sample rate and channels.
    UpdateStandby();
}

/* The bit depth chosen, or BITS_FLOAT. */
int MainFrame::BitsValue() {
    std::wstring bits = ui::ComboValue(cmb_bits_);
    return bits == BITS_FLOAT_TEXT ? BITS_FLOAT : _wtoi(bits.c_str());
}

void MainFrame::LoadSettings() {
    auto or_default = [&](const wchar_t* key, const wchar_t* fallback) {
        std::wstring v = config.Get(key);
        return v.empty() ? std::wstring(fallback) : v;
    };
    ui::ComboSetValue(cmb_format_, or_default(L"formato", L"mp3"));
    // A format this computer cannot record (wma without its codec) gives way to mp3.
    if (ui::ComboValue(cmb_format_).empty()) ui::ComboSetValue(cmb_format_, L"mp3");
    // The saved choices first, alone in their lists: the format then offers
    // what it takes, keeping each one or the nearest to it.
    ui::ComboSetItems(cmb_bits_, {or_default(L"bits", L"16")});
    ui::ComboSetValue(cmb_bits_, or_default(L"bits", L"16"));
    ui::ComboSetItems(cmb_rate_, {or_default(L"taxa", L"44100")});
    ui::ComboSetValue(cmb_rate_, or_default(L"taxa", L"44100"));
    ui::ComboSetItems(cmb_bitrate_, {or_default(L"bitrate", L"128 KBPS")});
    ui::ComboSetValue(cmb_bitrate_, or_default(L"bitrate", L"128 KBPS"));
    SendMessageW(cmb_channels_, CB_SETCURSEL, config.Get(L"canais") == L"mono" ? 0 : 1, 0);
    OnFormatChange();
    ui::ComboSetValue(cmb_flac_level_, or_default(L"flac_nivel", L"8"));
    SetChecked(chk_tones_, config.GetBoolean(L"reproduzir_tons"));
    SetChecked(chk_separate_, config.GetBoolean(L"arquivos_separados"));
    SetWindowTextW(txt_filename_, config.Get(L"arquivo").c_str());
    SetWindowTextW(txt_duration_, config.Get(L"duracao").c_str());
    SetWindowTextW(txt_dir_, config.Get(L"diretorio").c_str());

    auto all_devices = audio.GetInputDevices();
    sources_.clear();
    for (auto& entry : config.GetSources()) {
        size_t before = sources_.size();
        if (entry.type == L"device") {
            for (auto& d : all_devices) {
                if (d.first != entry.id) continue;
                sources_.push_back(Source{L"device", entry.id, d.second, entry.volume, entry.fx});
                audio.InitDevice(entry.id);
                break;
            }
        } else if (entry.type == L"exe") {
            // Kept even with the program closed: it is recorded whenever it runs.
            sources_.push_back(Source{L"exe", 0, entry.exe, entry.volume, entry.fx});
        } else if (entry.type == L"url") {
            // Kept even offline: it connects when recording.
            sources_.push_back(Source{L"url", 0, entry.path, entry.volume, entry.fx});
        } else if (entry.type == L"file") {
            // Kept even if the file is gone for now: recording says so.
            sources_.push_back(
                Source{L"file", 0, entry.path, entry.volume, entry.fx, entry.loop, entry.shuffle,
                       entry.subfolders});
        } else {
            std::wstring resolved = GetProcessName((DWORD)entry.id);
            if (!resolved.empty()) {
                sources_.push_back(Source{L"process", entry.id,
                                          resolved + L"  (PID " + std::to_wstring(entry.id) + L")",
                                          entry.volume, entry.fx});
            }
        }
        if (sources_.size() > before) sources_.back().pan = entry.pan;
    }
    schedules_ = config.GetSchedules();
    for (auto& s : schedules_) s.id = next_schedule_id_++;
    SetTimer(hwnd_, TIMER_SCHEDULE, 1000, nullptr);
    SetTimer(hwnd_, TIMER_FILES, 1000, nullptr);
    if (sources_.empty() && !all_devices.empty()) {
        sources_.push_back(
            Source{L"device", all_devices[0].first, all_devices[0].second, 1.0, FxSettings()});
        audio.InitDevice(all_devices[0].first);
    }
    RefreshList();
}

void MainFrame::SaveSettings() {
    config.SetConfig(L"formato", ui::ComboValue(cmb_format_));
    config.SetConfig(L"bitrate", ui::ComboValue(cmb_bitrate_));
    config.SetConfig(L"flac_nivel", ui::ComboValue(cmb_flac_level_));
    config.SetConfig(L"taxa", ui::ComboValue(cmb_rate_));
    config.SetConfig(L"bits", ui::ComboValue(cmb_bits_));
    config.SetConfig(L"canais", ChannelsValue());
    config.SetConfig(L"reproduzir_tons", Checked(chk_tones_));
    config.SetConfig(L"arquivos_separados", Checked(chk_separate_));
    config.SetConfig(L"arquivo", ui::GetText(txt_filename_));
    config.SetConfig(L"duracao", Trim(ui::GetText(txt_duration_)));
    config.SetConfig(L"diretorio", ui::GetText(txt_dir_));
    config.SetSources(sources_);
    config.SetSchedules(schedules_);
    config.Save();
}

void MainFrame::OnRecordToggle() {
    if (countdown_) {
        CancelCountdown();
        return;
    }
    if (!audio.is_recording()) {
        audio.StopMonitor();
        if (sources_.empty()) {
            ui::MsgBox(DialogParent(), tr("Adicione ao menos uma fonte de gravação antes de iniciar."),
                           tr("Aviso"), MB_ICONWARNING | MB_OK);
            return;
        }
        if (!ReadDuration() || !BackgroundReady() || !DetectionReady()) return;
        audio.StopPlayback();
        UpdatePlayButton();
        if (config.GetBoolean(L"contagem_regressiva")) StartCountdown();
        else StartRecordingNow(Checked(chk_tones_));
    } else {
        StopNow(Checked(chk_tones_));
    }
}

void MainFrame::StopNow(bool tone) {
    KillTimer(hwnd_, TIMER_DURATION);
    KillTimer(hwnd_, TIMER_DETECT);
    detect_ = DETECT_OFF;
    audio.SetSoundThreshold(-1000);
    audio.StopRecording();
    UpdateShutdownBlock();
    if (tone) audio.PlayTone(440, 150);
    ui::SetText(btn_record_, tr("Gr&avar"));
    ui::SetText(btn_pause_, tr("Pa&usar"));
    EnableControl(btn_pause_, false);
    ToggleUi(true);
    last_recording_.clear();
    for (auto& p : audio.output_files())
        if (FileExists(p)) last_recording_.push_back(p);
    UpdateRecordingButtons();
    // Waiting for the next one, with the pre-recording.
    UpdateStandby();
}

LONGLONG MainFrame::PrerollFrom() {
    int seconds = config.GetPreroll();
    if (seconds <= 0) return 0;
    LARGE_INTEGER now, freq;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&freq);
    return now.QuadPart - seconds * freq.QuadPart;
}

void MainFrame::UpdateStandby() {
    if (audio.is_recording()) return;
    double seconds = (double)config.GetPreroll();
    audio.SetStandby(sources_, OutputFormat().first, ChannelsValue(), seconds);
}

void MainFrame::OnSilence() {
    SilenceDialog dlg(config.GetSilence());
    if (dlg.ShowModal(hwnd_) != IDOK) return;
    config.SetSilence(dlg.result);
    config.Save();
}

/* The detection needs a source of its own to listen to: background audio
   never counts as sound. */
bool MainFrame::DetectionReady() {
    if (!config.GetSilence().on) return true;
    bool live = std::any_of(sources_.begin(), sources_.end(), [](const Source& s) { return s.type != L"file"; });
    if (live) return true;
    ui::MsgBox(DialogParent(),
               tr("A detecção de silêncio precisa de ao menos uma fonte que não seja áudio de fundo."),
               tr("Aviso"), MB_ICONWARNING | MB_OK);
    return false;
}

/* The recording has started, by the user or a schedule: from now on,
   silence for the time set pauses it, or stops it. */
void MainFrame::StartDetection() {
    silence_ = config.GetSilence();
    if (!silence_.on) return;
    detect_ = DETECT_ON;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    detect_since_ = now.QuadPart;
    audio.SetSoundThreshold(silence_.threshold_db);
    SetTimer(hwnd_, TIMER_DETECT, 50, nullptr);
}

/* Silence for the time set pauses the recording, or stops it; sound
   continues it. */
void MainFrame::OnDetectTick() {
    LARGE_INTEGER now_qpc, freq;
    QueryPerformanceCounter(&now_qpc);
    QueryPerformanceFrequency(&freq);
    const LONGLONG now = now_qpc.QuadPart, sound = audio.LastSound();
    const LONGLONG before = (LONGLONG)silence_.preroll_ms * freq.QuadPart / 1000;
    const LONGLONG began = audio.FirstSound();
    if (!audio.is_recording()) {
        KillTimer(hwnd_, TIMER_DETECT);
        detect_ = DETECT_OFF;
        return;
    }
    if (detect_ == DETECT_ON) {
        // Paused by the user, it waits: the silence counts from when it goes on.
        if (audio.is_paused()) {
            detect_since_ = now;
            return;
        }
        if (now - std::max(sound, detect_since_) < (LONGLONG)silence_.silence_ms * freq.QuadPart / 1000) return;
        if (silence_.stop) {
            StopNow(silence_.tones);
            return;
        }
        audio.Hold(silence_.preroll_ms / 1000.0);
        audio.WatchForSound();
        detect_ = DETECT_HELD;
        detect_since_ = now;
        if (silence_.tones) audio.PlayTone(659, 150);
        return;
    }
    if (detect_ == DETECT_HELD) {
        if (!began) return;
        audio.Release(began - before);
        detect_ = DETECT_ON;
        detect_since_ = now;
        if (silence_.tones) audio.PlayTone(880, 150);
    }
}

/* Starts a new recording right away, with the start tone if asked. */
bool MainFrame::StartRecordingNow(bool tone, LONGLONG from) {
    if (sources_.empty()) {
        // All sources were removed during the countdown.
        detect_ = DETECT_OFF;
        KillTimer(hwnd_, TIMER_DETECT);
        ui::SetText(btn_record_, tr("Gr&avar"));
        ToggleUi(true);
        UpdateStandby();
        ui::MsgBox(DialogParent(), tr("Adicione ao menos uma fonte de gravação antes de iniciar."),
                   tr("Aviso"), MB_ICONWARNING | MB_OK);
        return false;
    }
    if (from < 0) from = PrerollFrom();
    if (tone) audio.PlayTone(880, 150);
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
    // Split, the first file is "name 001": that is the one that must not exist yet.
    const int split = config.GetSplitSeconds();
    auto taken = [&](const std::wstring& path) {
        return FileExists(path) || (split > 0 && FileExists(AudioEngine::PartPath(path, 1)));
    };
    std::wstring full_path = JoinPath(out_dir, filename + L"." + ext);
    std::wstring base = filename;
    for (int counter = 1; taken(full_path); ++counter) {
        filename = base + L" " + std::to_wstring(counter);
        full_path = JoinPath(out_dir, filename + L"." + ext);
    }
    std::wstring bitrate_val = ui::ComboValue(cmb_bitrate_);
    bitrate_val = bitrate_val.substr(0, bitrate_val.find(L' '));
    int err = 0;
    audio.SetTags(config.GetTags());
    audio.SetSplitSeconds(split);
    bool ok = audio.StartRecording(full_path, ext, bitrate_val,
                                   _wtoi(ui::ComboValue(cmb_rate_).c_str()),
                                   BitsValue(), ChannelsValue(),
                                   sources_, Checked(chk_separate_),
                                   _wtoi(ui::ComboValue(cmb_flac_level_).c_str()), &err, from);
    if (ok) {
        RecordingStarted();
        return true;
    }
    detect_ = DETECT_OFF;
    KillTimer(hwnd_, TIMER_DETECT);
    ui::SetText(btn_record_, tr("Gr&avar"));
    ToggleUi(true);
    UpdateStandby();
    ui::MsgBox(DialogParent(),
               Format(tr("Erro BASS {erro} ao iniciar a gravação."),
                      {{L"erro", std::to_wstring(err)}}),
               tr("Erro"), MB_ICONERROR | MB_OK);
    return false;
}

/* Three low beeps, one per second, and the recording starts with the high
   one. The options are locked meanwhile, as when recording, and Stop cancels
   it. */
void MainFrame::StartCountdown() {
    countdown_ = 1;
    ui::SetText(btn_record_, tr("P&arar"));
    ToggleUi(false);
    SetTimer(hwnd_, TIMER_COUNTDOWN, 1000, nullptr);
    audio.PlayTone(440, 150);
}

void MainFrame::OnCountdownTick() {
    if (!countdown_) return;
    if (countdown_ < COUNTDOWN_BEEPS) {
        ++countdown_;
        audio.PlayTone(440, 150);
        return;
    }
    KillTimer(hwnd_, TIMER_COUNTDOWN);
    countdown_ = 0;
    StartRecordingNow(true);
}

void MainFrame::CancelCountdown() {
    if (!countdown_) return;
    KillTimer(hwnd_, TIMER_COUNTDOWN);
    countdown_ = 0;
    ui::SetText(btn_record_, tr("Gr&avar"));
    ToggleUi(true);
}

void MainFrame::RecordingStarted() {
    ui::SetText(btn_record_, tr("P&arar"));
    EnableWindow(btn_pause_, TRUE);
    ToggleUi(false);
    UpdateRecordingButtons();
    UpdateShutdownBlock();
    StartDetection();
    if (duration_limit_ > 0) {
        duration_start_ = audio.RecordedSeconds();
        SetTimer(hwnd_, TIMER_DURATION, 100, nullptr);
    }
}

/* Every background audio has something to play; the first one that does
   not is reported, and false stops the recording from starting. */
bool MainFrame::BackgroundReady() {
    for (auto& s : sources_) {
        if (s.type != L"file" || !BackgroundFiles(s.name, s.subfolders).empty()) continue;
        ui::MsgBox(DialogParent(),
                   Format(tr("Nenhum arquivo de áudio foi encontrado em:\n{caminho}"),
                          {{L"caminho", s.name}}),
                   tr("Aviso"), MB_ICONWARNING | MB_OK);
        return false;
    }
    return true;
}

/* The duration field: empty is no limit; otherwise seconds, minutes:seconds
   or hours:minutes:seconds. A wrong value is reported, and false stops the
   recording from starting. */
bool MainFrame::ReadDuration() {
    duration_limit_ = 0;
    // A scheduled recording with a duration of its own.
    if (schedule_duration_ > 0) {
        duration_limit_ = schedule_duration_;
        return true;
    }
    int total = 0;
    if (!ParseDuration(ui::GetText(txt_duration_), &total)) {
        ui::MsgBox(DialogParent(),
                   tr("Duração da gravação inválida. Use horas, minutos e segundos, como 1:30:00, "
                      "ou deixe em branco para gravar sem limite."),
                   tr("Aviso"), MB_ICONWARNING | MB_OK);
        SetFocus(txt_duration_);
        return false;
    }
    duration_limit_ = total;
    return true;
}

/* Stops the recording when it reaches the duration, as the Stop button does.
   While paused, the time does not run. */
void MainFrame::OnDurationTick() {
    if (!audio.is_recording()) {
        KillTimer(hwnd_, TIMER_DURATION);
        return;
    }
    // Held by the silence detection, the time does not run either.
    if (audio.RecordedSeconds() - duration_start_ >= duration_limit_) OnRecordToggle();
}

/* While recording, paused or not, Windows asks before shutting down and shows
   why the program is holding it. */
void MainFrame::UpdateShutdownBlock() {
    ShutdownBlockReasonDestroy(hwnd_);
    if (audio.is_recording())
        ShutdownBlockReasonCreate(
            hwnd_, tr("Gravação em andamento. Pare a gravação antes de desligar.").c_str());
}

bool MainFrame::CanContinue() { return HasRecording() && audio.CanContinue(); }

/* Goes on with the last recording, in the same files, as if it had only been
   paused. */
void MainFrame::OnContinueLast() {
    if (audio.is_recording() || Waiting() || !CanContinue()) return;
    if (!ReadDuration() || !BackgroundReady() || !DetectionReady()) return;
    audio.StopMonitor();
    audio.StopPlayback();  // the file being played is the one to continue
    UpdatePlayButton();
    ContinueNow(Checked(chk_tones_), PrerollFrom());
}

bool MainFrame::ContinueNow(bool tone, LONGLONG from) {
    if (tone) audio.PlayTone(880, 150);
    int err = 0;
    std::wstring failed;
    audio.SetTags(config.GetTags());
    if (audio.ContinueRecording(sources_, &err, &failed, from)) {
        RecordingStarted();
        return true;
    }
    detect_ = DETECT_OFF;
    KillTimer(hwnd_, TIMER_DETECT);
    ui::SetText(btn_record_, tr("Gr&avar"));
    ToggleUi(true);
    UpdateRecordingButtons();
    UpdateStandby();
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
    return false;
}

void MainFrame::OnPauseToggle() {
    if (!audio.is_paused()) {
        // Held by the silence detection, it pauses for good, until Continue.
        if (detect_ == DETECT_HELD) detect_ = DETECT_ON;
        audio.PauseRecording();
        if (Checked(chk_tones_)) audio.PlayTone(659, 150);
        ui::SetText(btn_pause_, tr("Contin&uar"));
    } else {
        audio.ResumeRecording();
        if (Checked(chk_tones_)) audio.PlayTone(880, 150);
        ui::SetText(btn_pause_, tr("Pa&usar"));
    }
}

void MainFrame::OnBrowse() {
    std::wstring path;
    if (PickFolder(hwnd_, tr("Selecionar Pasta"), &path)) SetWindowTextW(txt_dir_, path.c_str());
}

/* Folder in use: the one in the edit field, or the default if it is empty:
   next to the portable program, or in Documents for the installed one. */
std::wstring MainFrame::CurrentOutputDir() {
    std::wstring dir = Trim(ui::GetText(txt_dir_));
    if (!dir.empty()) return dir;
    return IsInstalled() ? InstalledRecordingsDir() : JoinPath(CurrentDir(), tr("Gravações"));
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
HWND MainFrame::DialogParent() { return IsWindowVisible(hwnd_) && !IsIconic(hwnd_) ? hwnd_ : nullptr; }

void MainFrame::Warn(const std::wstring& message, const std::wstring& title, UINT icon) {
    ui::MsgBox(DialogParent(), message, title, icon | MB_OK);
}

/* The last recording's files deleted or moved by another program: its
   buttons, here and in the tray, follow at once. */
void MainFrame::OnFilesTick() {
    if (audio.is_recording() || confirming_delete_) return;
    int state = (HasRecording() ? 1 : 0) | (CanContinue() ? 2 : 0);
    if (state != files_state_) UpdateRecordingButtons();
}

void MainFrame::UpdateRecordingButtons() {
    bool available = HasRecording();
    files_state_ = (available ? 1 : 0) | (CanContinue() ? 2 : 0);
    EnableControl(btn_play_, available);
    EnableControl(btn_delete_, available);
    EnableControl(btn_continue_, CanContinue());
    UpdatePlayButton();
}

void MainFrame::UpdatePlayButton() {
    bool playing = audio.IsPlaying();
    ui::SetText(btn_play_, playing ? tr("Parar &Reprodução") : tr("&Reproduzir Gravação"));
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
    if (confirming_delete_) return;
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
    confirming_delete_ = true;
    bool confirmed = ConfirmYesNo(DialogParent(), msg, tr("Confirmar Exclusão"));
    confirming_delete_ = false;
    if (!confirmed) return;
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
    // The sources stay available: they can be added, edited and removed while
    // recording.
    for (HWND ctrl : {cmb_format_, cmb_bitrate_, cmb_flac_level_, cmb_rate_, cmb_bits_, cmb_channels_,
                      chk_separate_, txt_duration_, txt_dir_, btn_browse_, txt_filename_,
                      btn_settings_, btn_tags_, btn_silence_})
        EnableControl(ctrl, enable);
}

/* Disabling the focused control leaves the window with no focus at all, and
   the keyboard does nothing until the user switches windows (wxWidgets, in the
   Python version, moved the focus by itself). So the focus goes first to the
   record button, which is never disabled. */
void MainFrame::EnableControl(HWND ctrl, bool enable) {
    if (!enable) {
        HWND focus = GetFocus();
        if (focus && (focus == ctrl || IsChild(ctrl, focus))) SetFocus(btn_record_);
    }
    EnableWindow(ctrl, enable);
}

bool MainFrame::OnClose() {
    if (audio.is_recording() &&
        !ConfirmYesNo(DialogParent(),
                      tr("Há uma gravação em andamento.\n\nDeseja mesmo fechar o VB Recorder?"),
                      tr("Confirmar Saída")))
        return false;
    if (play_timer_) KillTimer(hwnd_, TIMER_PLAY);
    play_timer_ = false;
    CancelCountdown();
    KillTimer(hwnd_, TIMER_DETECT);
    detect_ = DETECT_OFF;
    audio.StopRecording();
    UpdateShutdownBlock();
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

/* A button of the window as a menu item: its text, without the accelerator. */
void MainFrame::AddButtonItem(HMENU menu, UINT id, HWND button) {
    std::wstring text = ui::GetText(button);
    // "(&T)", as some languages have it, goes whole; otherwise only the "&".
    size_t at = text.find(L"(&");
    if (at != std::wstring::npos && at + 3 < text.size() && text[at + 3] == L')') {
        text.erase(at, 4);
        while (at > 0 && at <= text.size() && text[at - 1] == L' ') text.erase(--at, 1);
    }
    text.erase(std::remove(text.begin(), text.end(), L'&'), text.end());
    AppendMenuW(menu, MF_STRING, id, text.c_str());
}

/* Opened from the tray with the window hidden or minimized, the window has
   no owner and comes to the front; once it is closed, the main window stays
   as it was, and what was in front before gets the focus back. With the
   main window open, it opens over it and goes back to it, as from its
   button. */
void MainFrame::FromTray(void (MainFrame::*open)()) {
    const HWND back = tray_back_;
    const bool hidden = DialogParent() == nullptr;
    tray_dialog_ = hidden;
    (this->*open)();
    tray_dialog_ = false;
    if (!hidden || !back || back == hwnd_ || !IsWindow(back) || !IsWindowVisible(back)) return;
    // The menu was opened from the taskbar itself: nothing to go back to.
    wchar_t cls[64] = {};
    GetClassNameW(back, cls, 64);
    for (const wchar_t* shell : {L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd", L"NotifyIconOverflowWindow",
                                 L"TopLevelWindowForOverflowXamlIsland"})
        if (wcscmp(cls, shell) == 0) return;
    SetForegroundWindow(back);
}

/* While a window opened from the tray is open, the tray and Show bring it
   to the front instead: only one at a time. */
bool MainFrame::FocusTrayDialog() {
    if (!tray_dialog_) return false;
    struct Find {
        HWND main, found;
    } find{hwnd_, nullptr};
    EnumThreadWindows(
        GetCurrentThreadId(),
        [](HWND w, LPARAM lp) -> BOOL {
            Find* f = (Find*)lp;
            if (w == f->main || !IsWindowVisible(w) || GetWindow(w, GW_OWNER)) return TRUE;
            f->found = w;
            return FALSE;
        },
        (LPARAM)&find);
    if (find.found) SetForegroundWindow(GetLastActivePopup(find.found));
    return true;
}

void MainFrame::ShowTrayMenu() {
    // A window opened from the tray, or over the main window, is still
    // open: it comes to the front.
    if (FocusTrayDialog()) return;
    if (!IsWindowEnabled(hwnd_)) {
        SetForegroundWindow(GetLastActivePopup(hwnd_));
        return;
    }
    tray_back_ = GetForegroundWindow();
    HMENU menu = CreatePopupMenu();
    menu_combos_ = {cmb_format_, cmb_bitrate_, cmb_flac_level_, cmb_rate_, cmb_channels_, cmb_bits_};
    if (Waiting()) {
        AppendMenuW(menu, MF_STRING, MENU_STOP, tr("Parar").c_str());
    } else if (!audio.is_recording()) {
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
        if (fmt == L"mp3" || fmt == L"ogg" || fmt == L"opus" || fmt == L"wma" || fmt == L"m4a")
            AddSubmenu(menu, tr("Bitrate"), 1, cmb_bitrate_);
        if (fmt == L"flac") AddSubmenu(menu, tr("Compressão FLAC"), 2, cmb_flac_level_);
        AddSubmenu(menu, tr("Taxa de Amostragem"), 3, cmb_rate_);
        if (fmt == L"wav" || fmt == L"aiff" || fmt == L"flac" || fmt == L"wma")
            AddSubmenu(menu, tr("Profundidade de Bits"), 5, cmb_bits_);
        AddSubmenu(menu, tr("Canais"), 4, cmb_channels_);
        AddButtonItem(menu, MENU_TAGS, btn_tags_);
        if (sources_.size() >= 2) {
            AppendMenuW(menu, MF_STRING | (Checked(chk_separate_) ? MF_CHECKED : 0), MENU_SEPARATE,
                        tr("Gravar fontes em arquivos separados").c_str());
        }
        AppendMenuW(menu, MF_STRING | (Checked(chk_tones_) ? MF_CHECKED : 0), MENU_TONES,
                    tr("Reproduzir tons ao iniciar ou parar gravação").c_str());
        AddButtonItem(menu, MENU_SCHEDULE, btn_schedule_);
        AddButtonItem(menu, MENU_OPEN_DIR, btn_open_dir_);
        AddButtonItem(menu, MENU_SETTINGS, btn_settings_);
    } else {
        std::wstring label_pause = audio.is_paused() ? tr("Continuar") : tr("Pausar");
        AppendMenuW(menu, MF_STRING, MENU_PAUSE, label_pause.c_str());
        AppendMenuW(menu, MF_STRING, MENU_STOP, tr("Parar").c_str());
        // As in the window: while recording, the schedules and the folder stay available.
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AddButtonItem(menu, MENU_SCHEDULE, btn_schedule_);
        AddButtonItem(menu, MENU_OPEN_DIR, btn_open_dir_);
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
        case MENU_TAGS: FromTray(&MainFrame::OnTags); return;
        case MENU_SCHEDULE: FromTray(&MainFrame::OnSchedule); return;
        case MENU_OPEN_DIR: OnOpenDir(); return;
        case MENU_SETTINGS: FromTray(&MainFrame::OnSettings); return;
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
        SendMessageW(combo, CB_SETCURSEL, item, 0);
        // The other lists follow, before saving.
        if (combo != cmb_bitrate_ && combo != cmb_flac_level_) OnFormatChange();
        SaveSettings();
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    // Started by Windows, the working folder is System32: the .ini and the
    // recordings belong next to the program. The installed copy always keeps
    // its .ini there, wherever its shortcut starts it from.
    if (wcsstr(GetCommandLineW(), STARTUP_FLAG) || IsInstalled())
        SetCurrentDirectoryW(ExeDir().c_str());
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc = {sizeof(icc),
                                ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES |
                                    ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icc);
    ui::Init(instance);
    // Right after an update: waits for the old version to close, and removes
    // what it left behind.
    FinishUpdate(GetCommandLineW());
    RefreshInstalledVersion();

    // Settings come first: they define the language of the messages,
    // including the one saying the program is already running.
    std::unique_ptr<MainFrame> frame(new MainFrame());
    i18n::SetLanguage(frame->config.Get(L"idioma"));

    g_instance_mutex = CreateMutexW(nullptr, FALSE, INSTANCE_MUTEX);
    // Another one is already open: this one does not look for updates by
    // itself, as the first one does, or both would replace the same files.
    const bool another = GetLastError() == ERROR_ALREADY_EXISTS;
    if (another && !frame->config.GetBoolean(L"multiplas_instancias")) {
        ui::MsgBox(nullptr, tr("O VB Recorder já está em execução."), tr("Aviso"),
                       MB_ICONWARNING | MB_OK);
        return 0;
    }

    if (!frame->audio.Load()) return 1;
    frame->audio.SetBuffer(frame->config.GetBuffer());
    if (!frame->Create()) return 1;
    // A portable program: if its folder moved, Windows must start it from the new one.
    if (frame->config.GetBoolean(L"iniciar_com_windows")) RefreshStartWithWindows();
    frame->ShowAtStart();
    if (!another && frame->config.GetBoolean(L"verificar_atualizacoes", true)) frame->StartUpdateCheck();

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
