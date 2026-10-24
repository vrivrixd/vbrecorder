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
 * The dialogs of the equalizer, the effects and the noise remover of a
 * source. Each change goes to preview right away, so the test of the source
 * dialog sounds the way the dialog is; Cancel gives back what was there
 * before.
 */
#pragma once

#include <functional>
#include <vector>

#include "fx.h"
#include "ui.h"

typedef std::function<void(const FxSettings&)> FxPreview;

/* A dialog made of labeled sliders, with the value in its own unit shown next
   to each. The noise remover's goes from 0 to 100, which is what the screen
   reader says, like the volume of the source dialog; the equalizer's and the
   effects' say the value itself (dB, ms...). */
class SliderDialog : public ui::Dialog {
protected:
    explicit SliderDialog(const std::wstring& title) : ui::Dialog(title) {}

    /* Turns a position into the text shown next to the slider. */
    typedef std::function<std::wstring(int)> ValueText;

    struct Slider {
        HWND track = nullptr, value = nullptr;
        ValueText text;
        int max = 100, page = 5;
        int origin = 0;  // the value of position 0, in steps: Page Up lands on round values
        bool says_value = false;  // the screen reader says the text, not the position
    };

    /* Adds "label  slider  value" to the row; returns the slider's index. The
       arrows move it by one position, Page Up and Page Down by `page`. */
    int AddSlider(ui::Box* row, const std::wstring& label, int pos, ValueText text,
                  int max = 100, int page = 5, bool says_value = false, int origin = 0);
    int SliderPos(int i) const;
    void SetSliderPos(int i, int pos);  // without OnSliderChanged
    virtual void OnSliderChanged(int i) {}
    virtual void OnOk() = 0;
    /* The preset list: the presets, then "Custom". */
    HWND AddPresetCombo(ui::Box* box, const std::vector<std::wstring>& names, int selected,
                        int id);

    bool OnCommand(int id, int code, HWND control) override;
    bool OnCharHook(const MSG& msg) override;
    INT_PTR OnMessage(UINT msg, WPARAM wp, LPARAM lp, bool* handled) override;
    void AddOkCancel(ui::Box* box);

    std::vector<Slider> sliders_;

private:
    void ShowValue(int i);
    int FindSlider(HWND track) const;
    static LRESULT CALLBACK TrackProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
};

class EqualizerDialog : public SliderDialog {
public:
    EqualizerDialog(const FxSettings& fx, FxPreview preview);
    FxSettings result;

protected:
    void OnInit() override;
    bool OnCommand(int id, int code, HWND control) override;
    void OnSliderChanged(int i) override;
    void OnOk() override;

private:
    void ApplyPreset(int index);
    void Changed(bool custom);

    // Slider 0 is the preamp; then the gain of each band.
    FxSettings fx_;
    FxPreview preview_;
    HWND cmb_preset_ = nullptr;
    bool updating_ = false;
};

/* The list of effects: checking one turns it on, Configure sets it up. */
class EffectsDialog : public SliderDialog {
public:
    EffectsDialog(const FxSettings& fx, FxPreview preview);
    FxSettings result;

protected:
    void OnInit() override;
    bool OnCommand(int id, int code, HWND control) override;
    INT_PTR OnMessage(UINT msg, WPARAM wp, LPARAM lp, bool* handled) override;
    void OnOk() override;

private:
    void Configure();

    FxSettings fx_;
    FxPreview preview_;
    HWND list_ = nullptr;
    bool ready_ = false;
};

/* One effect: its preset and controls. */
class EffectConfigDialog : public SliderDialog {
public:
    EffectConfigDialog(int kind, const FxSettings& fx, FxPreview preview);
    FxSettings result;

protected:
    void OnInit() override;
    bool OnCommand(int id, int code, HWND control) override;
    void OnSliderChanged(int i) override;
    void OnOk() override;

private:
    void ApplyPreset(int index);
    void Changed(bool custom);

    int kind_;
    FxSettings fx_;
    FxPreview preview_;
    HWND cmb_preset_ = nullptr;
    std::vector<int> param_of_slider_;
    std::vector<HWND> check_of_;  // per parameter, for the check boxes
    bool updating_ = false;
};

class NoiseDialog : public SliderDialog {
public:
    NoiseDialog(const FxSettings& fx, FxPreview preview);
    FxSettings result;

protected:
    void OnInit() override;
    bool OnCommand(int id, int code, HWND control) override;
    void OnSliderChanged(int i) override;
    void OnOk() override;

private:
    FxSettings fx_;
    FxPreview preview_;
    HWND chk_on_ = nullptr;
};
