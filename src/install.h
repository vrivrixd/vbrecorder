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
 * The copy put in place by the installer (installer/VBRecorder.iss), next to
 * the portable one. It installs for the current user only, so its entry in
 * "Installed apps" is in HKEY_CURRENT_USER, where the program can write.
 */
#pragma once

#include <string>

/* AppId of the installer; its uninstall entry is "<AppId>_is1". */
#define VB_APP_ID L"{97ED0908-C5B3-45CF-9B21-19663A845A5E}"

/* Whether this executable is the installed copy: the uninstall entry points
   to its folder. A portable copy elsewhere is not. */
bool IsInstalled();

/* The installed copy updates itself like the portable one, by replacing its
   files; this shows the new version in "Installed apps" too. */
void RefreshInstalledVersion();

/* Where the installed copy records by default: Documents\VB Recorder. The
   program's folder may need administrator rights to write in. */
std::wstring InstalledRecordingsDir();
