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

#include "schedule.h"

#include <cwchar>
#include <vector>

#include "i18n.h"
#include "util.h"

namespace {

/* The Windows locale of the program's language ("pt-BR" for pt_BR), so dates
   look like the rest of the program; the user's own if Windows has none. */
std::wstring Locale() {
    std::wstring name = i18n::CurrentLanguage();
    for (auto& c : name)
        if (c == L'_') c = L'-';
    return IsValidLocaleName(name.c_str()) ? name : std::wstring();
}

const wchar_t* LocaleArg(const std::wstring& locale) {
    return locale.empty() ? LOCALE_NAME_USER_DEFAULT : locale.c_str();
}

bool ToSystemTime(const std::wstring& iso, SYSTEMTIME* st) {
    int y = 0, m = 0, d = 0;
    if (swscanf(iso.c_str(), L"%d-%d-%d", &y, &m, &d) != 3) return false;
    *st = SYSTEMTIME{};
    st->wYear = (WORD)y;
    st->wMonth = (WORD)m;
    st->wDay = (WORD)d;
    FILETIME ft;
    return y >= 1601 && y <= 9999 && SystemTimeToFileTime(st, &ft);  // also checks the day
}

std::wstring FormatDate(const std::wstring& iso, DWORD flags) {
    SYSTEMTIME st;
    if (!ToSystemTime(iso, &st)) return iso;
    std::wstring locale = Locale();
    wchar_t buf[128];
    if (!GetDateFormatEx(LocaleArg(locale), flags, &st, nullptr, buf, 128, nullptr)) return iso;
    return buf;
}

std::wstring Iso(int y, int m, int d) {
    wchar_t buf[16];
    swprintf(buf, 16, L"%04d-%02d-%02d", y, m, d);
    return buf;
}

long long Seconds(const SYSTEMTIME& st) {
    FILETIME ft;
    SystemTimeToFileTime(&st, &ft);
    return (((long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000000LL;
}

}  // namespace

bool ParseDuration(const std::wstring& text, int* seconds) {
    std::wstring t = Trim(text);
    *seconds = 0;
    if (t.empty()) return true;
    long long total = 0;
    int parts = 0;
    size_t pos = 0;
    while (pos <= t.size()) {
        size_t end = t.find(L':', pos);
        if (end == std::wstring::npos) end = t.size();
        std::wstring part = Trim(t.substr(pos, end - pos));
        if (part.empty() || part.size() > 6 || part.find_first_not_of(L"0123456789") != std::wstring::npos)
            return false;
        total = total * 60 + _wtoi(part.c_str());
        ++parts;
        pos = end + 1;
    }
    if (parts > 3 || total <= 0 || total > 0x7fffffff) return false;
    *seconds = (int)total;
    return true;
}

std::wstring FormatDuration(int seconds) {
    wchar_t buf[32];
    swprintf(buf, 32, L"%d:%02d:%02d", seconds / 3600, seconds / 60 % 60, seconds % 60);
    return buf;
}

bool ParseScheduleTime(const std::wstring& text, int* hour, int* minute, int* second) {
    std::wstring t = Trim(text);
    int h = -1, m = -1, s = 0;
    wchar_t rest = 0;
    int n = swscanf(t.c_str(), L"%d:%d%lc", &h, &m, &rest);
    if (n == 3 && rest == L':') n = swscanf(t.c_str(), L"%d:%d:%d%lc", &h, &m, &s, &rest) == 3 ? 2 : 0;
    if (n != 2) return false;
    if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59) return false;
    *hour = h;
    *minute = m;
    *second = s;
    return true;
}

bool ParseScheduleDate(const std::wstring& text, std::wstring* iso) {
    // Three numbers, in the order of the language's short date: day, month
    // and year in Brazil, month first in the United States.
    std::vector<int> nums;
    std::wstring t = Trim(text), digits;
    for (size_t i = 0; i <= t.size(); ++i) {
        if (i < t.size() && iswdigit(t[i])) {
            digits += t[i];
        } else if (!digits.empty()) {
            nums.push_back(_wtoi(digits.c_str()));
            digits.clear();
        } else if (i < t.size() && iswalpha(t[i])) {
            return false;
        }
    }
    if (nums.size() != 3) return false;
    std::wstring locale = Locale();
    wchar_t pattern[80] = L"dd/MM/yyyy";
    GetLocaleInfoEx(LocaleArg(locale), LOCALE_SSHORTDATE, pattern, 80);
    std::wstring p = pattern;
    size_t pd = p.find(L'd'), pm = p.find(L'M'), py = p.find(L'y');
    if (pd == std::wstring::npos || pm == std::wstring::npos || py == std::wstring::npos) {
        pd = 0;
        pm = 1;
        py = 2;
    }
    auto rank = [&](size_t pos) { return (int)(pos > pd) + (int)(pos > pm) + (int)(pos > py); };
    int d = nums[rank(pd)], m = nums[rank(pm)], y = nums[rank(py)];
    if (y < 100) y += 2000;
    std::wstring candidate = Iso(y, m, d);
    SYSTEMTIME st;
    if (!ToSystemTime(candidate, &st)) return false;
    *iso = candidate;
    return true;
}

std::wstring ScheduleDateForEdit(const std::wstring& iso) {
    return iso.empty() ? iso : FormatDate(iso, DATE_SHORTDATE);
}

std::wstring ScheduleTimeForEdit(const Schedule& s) {
    wchar_t buf[16];
    if (s.second)
        swprintf(buf, 16, L"%02d:%02d:%02d", s.hour, s.minute, s.second);
    else
        swprintf(buf, 16, L"%02d:%02d", s.hour, s.minute);
    return buf;
}

std::wstring ScheduleDateExample() {
    SYSTEMTIME now;
    GetLocalTime(&now);
    return FormatDate(Iso(now.wYear, now.wMonth, now.wDay), DATE_SHORTDATE);
}

std::wstring ScheduleText(const Schedule& s) {
    std::wstring time = ScheduleTimeForEdit(s);
    std::wstring text =
        s.date.empty()
            ? Format(tr("Todos os dias às {hora}"), {{L"hora", time}})
            : Format(tr("{data} às {hora}"), {{L"data", FormatDate(s.date, DATE_LONGDATE)}, {L"hora", time}});
    if (s.duration > 0)
        text = Format(tr("{agendamento}, por {duracao}"),
                      {{L"agendamento", text}, {L"duracao", FormatDuration(s.duration)}});
    if (s.remove_after) text = Format(tr("{agendamento}, remover após concluído"), {{L"agendamento", text}});
    return text;
}

bool SchedulePassed(const Schedule& s) {
    SYSTEMTIME st, now;
    if (s.date.empty() || !ToSystemTime(s.date, &st)) return false;
    st.wHour = (WORD)s.hour;
    st.wMinute = (WORD)s.minute;
    st.wSecond = (WORD)s.second;
    GetLocalTime(&now);
    now.wMilliseconds = 0;
    return Seconds(st) < Seconds(now);
}

std::wstring ScheduleDueKey(const Schedule& s, const SYSTEMTIME& now) {
    std::wstring today = Iso(now.wYear, now.wMonth, now.wDay);
    if (!s.date.empty() && s.date != today) return std::wstring();
    int late = (now.wHour * 3600 + now.wMinute * 60 + now.wSecond) - (s.hour * 3600 + s.minute * 60 + s.second);
    if (late < 0 || late >= 60) return std::wstring();
    return today + L" " + ScheduleTimeForEdit(s);
}
