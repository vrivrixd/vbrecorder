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
 * Program translation.
 *
 * The language is the one of the Windows interface, or the one chosen in the
 * settings. The original texts are in Brazilian Portuguese; when Windows is in
 * a language without a translation, the program shows up in English.
 *
 * The catalogs are the same gettext .mo files as in the Python version. They
 * are embedded in the executable, and a locale folder next to it, if present,
 * takes precedence: that is how a new translation can be tested without
 * recompiling.
 */
#pragma once

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace i18n {

/* Switches to another language. With an empty code, follows the Windows language. */
std::wstring SetLanguage(const std::wstring& code);

/* The language in use: "pt_BR", "vi"...; English when nothing was found. */
std::wstring CurrentLanguage();

/* Codes with a catalog, plus the original language, sorted by display name. */
std::vector<std::wstring> AvailableLanguages();

/* Language name written in itself; unknown languages show their code. */
std::wstring LanguageName(const std::wstring& code);

}  // namespace i18n

/* Translated text. The message is the original Portuguese text, in UTF-8. */
std::wstring tr(const char* message);
std::wstring ntr(const char* singular, const char* plural, unsigned long n);

/* Replaces {name} fields with values, like Python's str.format. */
std::wstring Format(const std::wstring& text,
                    std::initializer_list<std::pair<const wchar_t*, std::wstring>> args);
