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
 * Scheduled recordings, kept by the program itself (not the Windows Task
 * Scheduler): at the time set, on the date set or every day, the recording
 * starts as if Record had been pressed. Dates are typed and shown as the
 * program's language writes them, and kept as 2026-12-31.
 */
#pragma once

#include <windows.h>

#include <string>

struct Schedule {
    std::wstring date;  // "2026-12-31"; empty is every day
    int hour = 0, minute = 0;
    int duration = 0;           // seconds; 0 follows the main window's duration
    bool remove_after = false;  // leaves the list when its recording starts
    int id = 0;                 // while the program runs, to tell them apart
};

/* A recording duration: seconds, minutes:seconds or hours:minutes:seconds.
   Empty is 0, no limit; false if it is not a duration. */
bool ParseDuration(const std::wstring& text, int* seconds);
/* 5400 as 1:30:00. */
std::wstring FormatDuration(int seconds);

/* "14:30" or "9:05"; false if it is not a time of day. */
bool ParseScheduleTime(const std::wstring& text, int* hour, int* minute);
/* A date as the program's language writes it (31/12/2026), into 2026-12-31. */
bool ParseScheduleDate(const std::wstring& text, std::wstring* iso);
/* 2026-12-31 as it is typed: 31/12/2026. */
std::wstring ScheduleDateForEdit(const std::wstring& iso);
std::wstring ScheduleTimeForEdit(const Schedule& s);
/* Today as the date field expects it, to show how to type one. */
std::wstring ScheduleDateExample();

/* What the list shows: "Every day at 14:30", or the date and the time. */
std::wstring ScheduleText(const Schedule& s);

/* A schedule with a date whose time has already gone by. */
bool SchedulePassed(const Schedule& s);
/* When its recording is due: the day and minute (2026-12-31 14:30), if now is
   within the minute after its time; empty otherwise. The minute of leeway
   covers a timer that came late, or the program opened just after. */
std::wstring ScheduleDueKey(const Schedule& s, const SYSTEMTIME& now);
