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
 * Updates from the GitHub releases: the latest release is checked, the zip of
 * this architecture (32 or 64 bits) is downloaded and verified, and its files
 * take the place of the current ones. Windows lets a running program rename
 * its own files, so the old ones are set aside (.old) and removed by the new
 * version when it starts.
 */
#pragma once

#include <windows.h>

#include <atomic>
#include <functional>
#include <string>

struct UpdateInfo {
    std::wstring version;  // "1.5"
    std::wstring url;      // the zip of this architecture
    unsigned long long size = 0;
    std::string sha256;    // lower-case hex, when GitHub gives it
};

enum class UpdateCheck { UpToDate, Available, Failed };

/* Asks GitHub for the latest release. Blocks for a moment: network. */
UpdateCheck CheckForUpdate(UpdateInfo* info);

/* Downloads the zip to a file. progress(done, total) is called along the way,
   from the calling thread. False on failure, or if cancel was set. */
bool DownloadUpdate(const UpdateInfo& info, const std::wstring& file,
                    const std::atomic<bool>& cancel,
                    const std::function<void(unsigned long long, unsigned long long)>& progress);

/* Verifies the zip and puts its files in place of the current ones. */
bool InstallUpdate(const std::wstring& zip, const UpdateInfo& info, std::wstring* error);

/* Starts the new version, which waits for this one to close. */
bool StartUpdatedProgram();

/* At startup: after an update, waits for the old version to close; then
   removes the files it left behind. */
void FinishUpdate(const wchar_t* command_line);
