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

#pragma once

#include <functional>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "audio_engine.h"
#include "config.h"
#include "ui.h"
#include "updater.h"

class HotkeysDialog : public ui::Dialog {
public:
    explicit HotkeysDialog(ConfigManager& config);

protected:
    void OnInit() override;
    bool OnCommand(int id, int code, HWND control) override;

private:
    ConfigManager& config_;
    std::vector<std::pair<std::wstring, HWND>> ctrls_;
};

class SettingsDialog : public ui::Dialog {
public:
    /* check_update runs the "Check for update" button; it returns true when
       the program is about to be replaced by the new version, which closes
       the dialog. */
    SettingsDialog(ConfigManager& config, std::function<void()> on_keys_saved,
                   std::function<bool(HWND)> check_update);

    /* Typed buffer, or -1 if it is not a valid number in range. */
    int BufferValue() const;
    std::wstring LanguageCode() const { return language_; }
    bool start_minimized = false;
    bool tray = false;
    bool on_top = true;
    bool check_updates = true;
    bool start_with_windows = false;
    bool countdown = false;
    bool updating = false;  // closed because the update is being installed

protected:
    void OnInit() override;
    bool OnCommand(int id, int code, HWND control) override;
    bool OnCharHook(const MSG& msg) override;

private:
    void OnOk();

    ConfigManager& config_;
    std::function<void()> on_keys_saved_;
    std::function<bool(HWND)> check_update_;
    std::vector<std::wstring> codes_;
    std::wstring language_;
    std::wstring buffer_text_;
    HWND cmb_lang_ = nullptr, chk_start_min_ = nullptr, chk_tray_ = nullptr,
         chk_on_top_ = nullptr, txt_buffer_ = nullptr, btn_keys_ = nullptr, btn_ok_ = nullptr,
         btn_cancel_ = nullptr, chk_updates_ = nullptr, btn_check_ = nullptr,
         chk_startup_ = nullptr, chk_countdown_ = nullptr;
};

/* Downloads an update, with a progress bar and a Cancel button. */
class DownloadDialog : public ui::Dialog {
public:
    DownloadDialog(const UpdateInfo& info, const std::wstring& file);
    ~DownloadDialog();
    bool ok = false;         // downloaded
    bool cancelled = false;  // the user gave up

protected:
    void OnInit() override;
    bool OnCommand(int id, int code, HWND control) override;
    INT_PTR OnMessage(UINT msg, WPARAM wp, LPARAM lp, bool* handled) override;

private:
    static DWORD WINAPI Worker(void* self);
    UpdateInfo info_;
    std::wstring file_;
    std::atomic<bool> cancel_{false};
    HANDLE thread_ = nullptr;
    HWND bar_ = nullptr;
};

class AddSourceDialog : public ui::Dialog {
public:
    /* output_format returns the sample rate and channels of the main window,
       so the test sounds like the recording. */
    AddSourceDialog(AudioEngine& audio, const std::vector<Source>& existing,
                    const Source* edit_source, std::function<std::pair<int, int>()> output_format);

    bool has_selected = false;
    Source selected;

protected:
    void OnInit() override;
    bool OnCommand(int id, int code, HWND control) override;
    bool OnCharHook(const MSG& msg) override;
    INT_PTR OnMessage(UINT msg, WPARAM wp, LPARAM lp, bool* handled) override;

private:
    typedef std::tuple<std::wstring, int, std::wstring> SourceItem;  // type, id, name

    void PopulateFilter(int sel);
    void OnFilterChange();
    std::set<std::pair<std::wstring, int>> ExistingIds() const;
    void LoadProcesses();
    void OnRefresh();
    int SelectedDevice() const;  // -1 if the source is not a device
    int CheckedFilter() const;
    void UpdateOkState();
    void UpdateTestState();
    void OnTest();
    void StopTest();
    double Volume() const;
    void UpdateDbLabel();
    void OnOk();
    static LRESULT CALLBACK SliderProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

    AudioEngine& audio_;
    std::vector<Source> existing_;
    bool editing_ = false;
    Source edit_source_;
    std::function<std::pair<int, int>()> output_format_;
    bool testing_ = false;
    int filter_ = 0;
    std::vector<SourceItem> device_items_, process_items_;
    std::vector<SourceItem> exe_items_;  // always empty: the name is typed
    const std::vector<SourceItem>* current_items_ = nullptr;
    HWND rb_exe_ = nullptr, lbl_src_ = nullptr, lbl_exe_ = nullptr, txt_exe_ = nullptr;
    HWND grp_ = nullptr, rb_devices_ = nullptr, rb_processes_ = nullptr, cmb_ = nullptr,
         sld_ = nullptr, lbl_db_ = nullptr, btn_test_ = nullptr, btn_refresh_ = nullptr, btn_ok_ = nullptr,
         btn_cancel_ = nullptr, note_ = nullptr;
};

/* Single-choice list (wx.SingleChoiceDialog). Returns the index, or -1. */
int SingleChoice(HWND parent, const std::wstring& message, const std::wstring& title,
                 const std::vector<std::wstring>& items, bool on_top);

/* Folder picker. Returns false if the user cancels. */
bool PickFolder(HWND parent, const std::wstring& title, std::wstring* path);

/* Native Yes/No question: warning icon (with the Windows sound) and focus on
   Yes. The question icon is not used because the SystemQuestion sound has no
   assigned sound on Windows 10/11 and the box would be silent.
   MB_SETFOREGROUND brings it to the front when called from the tray menu. */
bool ConfirmYesNo(HWND parent, const std::wstring& message, const std::wstring& title);
