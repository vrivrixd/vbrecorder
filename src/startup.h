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
 * Starting with Windows. The program is portable, so the entry in the user's
 * Run key points to wherever the executable is now, and is corrected when the
 * folder moves.
 */
#pragma once

/* The command line Windows uses carries this flag, so the program knows it was
   started by Windows. */
#define STARTUP_FLAG L"--startup"

/* Adds or removes the entry. Adding it also undoes "Disable" from the
   Startup apps page or the Task Manager, since the user is asking for it. */
void SetStartWithWindows(bool on);

/* With the option on, points the entry to this executable again if the
   folder moved, without touching what the user disabled in Windows. */
void RefreshStartWithWindows();
