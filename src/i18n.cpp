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

#include "i18n.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>

#include "util.h"

namespace {

const wchar_t* const DOMAIN_MO = L"vbrecorder.mo";
const wchar_t* const LOCALE_DIR = L"locale";

/* Language the program is written in: it has no catalog, it is the original text. */
const wchar_t* const SOURCE_LANGUAGE = L"pt_BR";

/* Each language is listed in its own language, so people recognize theirs even
   when the program is open in a language they cannot read. */
const std::pair<const wchar_t*, const wchar_t*> NATIVE_NAMES[] = {
    {L"ar", L"العربية"},
    {L"bg", L"Български"},
    {L"bn", L"বাংলা"},
    {L"cs", L"Čeština"},
    {L"da", L"Dansk"},
    {L"de", L"Deutsch"},
    {L"en", L"English"},
    {L"eo", L"Esperanto"},
    {L"es", L"Español"},
    {L"fi", L"Suomi"},
    {L"fr", L"Français"},
    {L"gl", L"Galego"},
    {L"hi", L"हिन्दी"},
    {L"id", L"Bahasa Indonesia"},
    {L"it", L"Italiano"},
    {L"ja", L"日本語"},
    {L"ko", L"한국어"},
    {L"pl", L"Polski"},
    {L"pt_BR", L"Português (Brasil)"},
    {L"pt_PT", L"Português (Portugal)"},
    {L"ru", L"Русский"},
    {L"sk", L"Slovenčina"},
    {L"sl", L"Slovenščina"},
    {L"th", L"ไทย"},
    {L"tr", L"Türkçe"},
    {L"vi", L"Tiếng Việt"},
    {L"zh_CN", L"简体中文"},
    {L"zh_TW", L"繁體中文"},
};

/* Language used when Windows is in a language without a translation. */
const wchar_t* const FALLBACK_LANGUAGE = L"en";

/* Regions that read the same written Chinese as another one. */
const std::pair<const wchar_t*, const wchar_t*> ALIASES[] = {
    {L"zh_HK", L"zh_TW"},
    {L"zh_MO", L"zh_TW"},
    {L"zh_SG", L"zh_CN"},
};

/* ---- gettext plural expression (Plural-Forms), evaluated for each n. ---- */

struct Expr {
    char op = 0;  // 'n', '#' (number), '?' or an operator
    unsigned long value = 0;
    std::unique_ptr<Expr> a, b, c;
};

class PluralParser {
public:
    explicit PluralParser(const char* text) : p_(text) {}

    std::unique_ptr<Expr> Parse() {
        auto e = Ternary();
        Skip();
        return (ok_ && *p_ == '\0') ? std::move(e) : nullptr;
    }

private:
    void Skip() {
        while (*p_ == ' ' || *p_ == '\t') ++p_;
    }
    bool Eat(const char* tok) {
        Skip();
        size_t n = strlen(tok);
        if (strncmp(p_, tok, n) != 0) return false;
        // "<" must not swallow the start of "<=", nor "!" that of "!=".
        if (n == 1 && (tok[0] == '<' || tok[0] == '>' || tok[0] == '!' || tok[0] == '=') &&
            p_[1] == '=')
            return false;
        p_ += n;
        return true;
    }
    static std::unique_ptr<Expr> Node(char op, std::unique_ptr<Expr> a,
                                      std::unique_ptr<Expr> b = nullptr) {
        auto e = std::make_unique<Expr>();
        e->op = op;
        e->a = std::move(a);
        e->b = std::move(b);
        return e;
    }
    std::unique_ptr<Expr> Ternary() {
        auto cond = Or();
        if (!Eat("?")) return cond;
        auto yes = Ternary();
        if (!Eat(":")) ok_ = false;
        auto no = Ternary();
        auto e = Node('?', std::move(cond), std::move(yes));
        e->c = std::move(no);
        return e;
    }
    std::unique_ptr<Expr> Or() {
        auto e = And();
        while (Eat("||")) e = Node('|', std::move(e), And());
        return e;
    }
    std::unique_ptr<Expr> And() {
        auto e = Equality();
        while (Eat("&&")) e = Node('&', std::move(e), Equality());
        return e;
    }
    std::unique_ptr<Expr> Equality() {
        auto e = Relational();
        for (;;) {
            if (Eat("==")) e = Node('=', std::move(e), Relational());
            else if (Eat("!=")) e = Node('!', std::move(e), Relational());
            else return e;
        }
    }
    std::unique_ptr<Expr> Relational() {
        auto e = Additive();
        for (;;) {
            if (Eat("<=")) e = Node('l', std::move(e), Additive());
            else if (Eat(">=")) e = Node('g', std::move(e), Additive());
            else if (Eat("<")) e = Node('<', std::move(e), Additive());
            else if (Eat(">")) e = Node('>', std::move(e), Additive());
            else return e;
        }
    }
    std::unique_ptr<Expr> Additive() {
        auto e = Multiplicative();
        for (;;) {
            if (Eat("+")) e = Node('+', std::move(e), Multiplicative());
            else if (Eat("-")) e = Node('-', std::move(e), Multiplicative());
            else return e;
        }
    }
    std::unique_ptr<Expr> Multiplicative() {
        auto e = Unary();
        for (;;) {
            if (Eat("*")) e = Node('*', std::move(e), Unary());
            else if (Eat("/")) e = Node('/', std::move(e), Unary());
            else if (Eat("%")) e = Node('%', std::move(e), Unary());
            else return e;
        }
    }
    std::unique_ptr<Expr> Unary() {
        if (Eat("!")) return Node('N', Unary());
        return Primary();
    }
    std::unique_ptr<Expr> Primary() {
        Skip();
        if (*p_ == '(') {
            ++p_;
            auto e = Ternary();
            if (!Eat(")")) ok_ = false;
            return e;
        }
        if (*p_ == 'n') {
            ++p_;
            return Node('n', nullptr);
        }
        if (*p_ >= '0' && *p_ <= '9') {
            auto e = Node('#', nullptr);
            e->value = strtoul(p_, const_cast<char**>(&p_), 10);
            return e;
        }
        ok_ = false;
        return Node('#', nullptr);
    }

    const char* p_;
    bool ok_ = true;
};

unsigned long Eval(const Expr* e, unsigned long n) {
    switch (e->op) {
        case 'n': return n;
        case '#': return e->value;
        case '?': return Eval(e->a.get(), n) ? Eval(e->b.get(), n) : Eval(e->c.get(), n);
        case 'N': return !Eval(e->a.get(), n);
        case '|': return Eval(e->a.get(), n) || Eval(e->b.get(), n);
        case '&': return Eval(e->a.get(), n) && Eval(e->b.get(), n);
        default: break;
    }
    unsigned long x = Eval(e->a.get(), n), y = Eval(e->b.get(), n);
    switch (e->op) {
        case '=': return x == y;
        case '!': return x != y;
        case '<': return x < y;
        case '>': return x > y;
        case 'l': return x <= y;
        case 'g': return x >= y;
        case '+': return x + y;
        case '-': return x - y;
        case '*': return x * y;
        case '/': return y ? x / y : 0;
        case '%': return y ? x % y : 0;
    }
    return 0;
}

/* ---- .mo catalog ---- */

struct Catalog {
    std::map<std::string, std::vector<std::string>> messages;
    std::unique_ptr<Expr> plural;

    bool Load(const std::string& data) {
        if (data.size() < 28) return false;
        auto u32 = [&](size_t off, bool swap) -> uint32_t {
            if (off + 4 > data.size()) return 0;
            uint32_t v;
            memcpy(&v, data.data() + off, 4);
            if (swap)
                v = (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24);
            return v;
        };
        bool swap;
        if (u32(0, false) == 0x950412de) swap = false;
        else if (u32(0, true) == 0x950412de) swap = true;
        else return false;
        uint32_t count = u32(8, swap), orig = u32(12, swap), trans = u32(16, swap);
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t olen = u32(orig + i * 8, swap), ooff = u32(orig + i * 8 + 4, swap);
            uint32_t tlen = u32(trans + i * 8, swap), toff = u32(trans + i * 8 + 4, swap);
            if ((size_t)ooff + olen > data.size() || (size_t)toff + tlen > data.size()) continue;
            std::string key(data.data() + ooff, olen);
            std::string value(data.data() + toff, tlen);
            // The msgid of a plural entry comes as "singular\0plural".
            size_t nul = key.find('\0');
            if (nul != std::string::npos) key.resize(nul);
            std::vector<std::string> forms;
            size_t pos = 0;
            for (;;) {
                size_t end = value.find('\0', pos);
                forms.push_back(value.substr(pos, end == std::string::npos ? end : end - pos));
                if (end == std::string::npos) break;
                pos = end + 1;
            }
            if (key.empty()) {
                ParseHeader(forms[0]);
                continue;
            }
            bool empty = true;
            for (auto& f : forms) empty = empty && f.empty();
            if (!empty) messages[key] = forms;
        }
        return true;
    }

    void ParseHeader(const std::string& header) {
        size_t at = header.find("Plural-Forms:");
        if (at == std::string::npos) return;
        size_t eq = header.find("plural=", at);
        if (eq == std::string::npos) return;
        size_t end = header.find_first_of(";\n", eq);
        std::string expr = header.substr(eq + 7, end == std::string::npos ? end : end - eq - 7);
        plural = PluralParser(expr.c_str()).Parse();
    }
};

std::unique_ptr<Catalog> g_catalog;
/* English, for texts a catalog does not have yet; the European Portuguese one
   falls back to the original texts instead. */
std::unique_ptr<Catalog> g_fallback;

/* Catalogs embedded in the executable: one RCDATA resource per language, named
   after its code. The resource compiler stores names in upper case, so the
   code is rebuilt the way gettext writes it: language in lower case, region in
   upper case (pt_PT). */
BOOL CALLBACK CollectLanguage(HMODULE, LPCWSTR, LPWSTR name, LONG_PTR param) {
    if (IS_INTRESOURCE(name)) return TRUE;
    std::wstring code = ToLower(name);
    size_t sep = code.find(L'_');
    if (sep != std::wstring::npos)
        CharUpperBuffW(&code[sep + 1], (DWORD)(code.size() - sep - 1));
    reinterpret_cast<std::vector<std::wstring>*>(param)->push_back(code);
    return TRUE;
}

std::vector<std::wstring> EmbeddedLanguages() {
    std::vector<std::wstring> codes;
    EnumResourceNamesW(nullptr, RT_RCDATA, CollectLanguage, (LONG_PTR)&codes);
    return codes;
}

bool EmbeddedCatalog(const std::wstring& code, std::string* data) {
    HRSRC res = FindResourceW(nullptr, code.c_str(), RT_RCDATA);
    HGLOBAL mem = res ? LoadResource(nullptr, res) : nullptr;
    const char* bytes = mem ? (const char*)LockResource(mem) : nullptr;
    if (!bytes) return false;
    data->assign(bytes, SizeofResource(nullptr, res));
    return true;
}

std::wstring ExternalCatalogPath(const std::wstring& code) {
    return JoinPath(JoinPath(JoinPath(JoinPath(ExeDir(), LOCALE_DIR), code), L"LC_MESSAGES"),
                    DOMAIN_MO);
}

bool ReadWholeFile(const std::wstring& path, std::string* data) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD size = GetFileSize(f, nullptr), read = 0;
    bool ok = size != INVALID_FILE_SIZE && size < (1u << 24);
    if (ok) {
        data->resize(size);
        ok = size == 0 || (ReadFile(f, &(*data)[0], size, &read, nullptr) && read == size);
    }
    CloseHandle(f);
    return ok;
}

bool CatalogBytes(const std::wstring& code, std::string* data) {
    if (ReadWholeFile(ExternalCatalogPath(code), data)) return true;
    return EmbeddedCatalog(code, data);
}

std::wstring WindowsUiLanguage() {
    LANGID id = GetUserDefaultUILanguage();
    wchar_t name[LOCALE_NAME_MAX_LENGTH] = {};
    if (LCIDToLocaleName(MAKELCID(id, SORT_DEFAULT), name, LOCALE_NAME_MAX_LENGTH, 0) &&
        name[0]) {
        std::wstring out = name;
        std::replace(out.begin(), out.end(), L'-', L'_');
        return out;
    }
    return std::wstring();
}

/* Languages to try, from the most specific to the most generic. The LANGUAGE
   environment variable comes first, as gettext does; it allows testing a
   language without changing the Windows settings. */
std::vector<std::wstring> SystemLanguages() {
    std::vector<std::wstring> langs;
    for (const wchar_t* var : {L"LANGUAGE", L"LC_ALL", L"LC_MESSAGES", L"LANG"}) {
        wchar_t buf[512];
        DWORD n = GetEnvironmentVariableW(var, buf, 512);
        if (n == 0 || n >= 512) continue;
        std::wstring value(buf, n);
        size_t pos = 0;
        while (pos <= value.size()) {
            size_t end = value.find(L':', pos);
            if (end == std::wstring::npos) end = value.size();
            std::wstring part = value.substr(pos, end - pos);
            if (!part.empty()) langs.push_back(part.substr(0, part.find(L'.')));
            pos = end + 1;
        }
        break;
    }
    if (langs.empty()) {
        std::wstring name = WindowsUiLanguage();
        if (!name.empty()) langs.push_back(name);
    }
    for (auto& alias : ALIASES)
        if (std::find(langs.begin(), langs.end(), alias.first) != langs.end())
            langs.push_back(alias.second);
    // pt_PT also stands for pt, en_US for en, and so on.
    std::vector<std::wstring> copy = langs;
    for (auto& name : copy) {
        std::wstring root = name.substr(0, name.find(L'_'));
        if (!root.empty() && std::find(langs.begin(), langs.end(), root) == langs.end())
            langs.push_back(root);
    }
    return langs;
}

/* Looks for the catalog, from the most specific name to the most generic. The
   search does not normalize names: someone using Windows in Brazilian
   Portuguese does not get the Portugal catalog, they see the original texts. */
std::wstring LoadCatalog(const std::vector<std::wstring>& languages) {
    for (auto& lang : languages) {
        if (lang == SOURCE_LANGUAGE || lang == L"pt") {
            // The original texts: no catalog.
            g_catalog.reset();
            g_fallback.reset();
            return SOURCE_LANGUAGE;
        }
        std::string data;
        if (!CatalogBytes(lang, &data)) continue;
        auto cat = std::make_unique<Catalog>();
        if (!cat->Load(data)) continue;
        g_catalog = std::move(cat);
        g_fallback.reset();
        std::string english;
        if (lang.compare(0, 2, L"pt") != 0 && lang != FALLBACK_LANGUAGE &&
            CatalogBytes(FALLBACK_LANGUAGE, &english)) {
            auto en = std::make_unique<Catalog>();
            if (en->Load(english)) g_fallback = std::move(en);
        }
        return lang;
    }
    g_catalog.reset();
    g_fallback.reset();
    return std::wstring();
}

}  // namespace

namespace i18n {

std::wstring SetLanguage(const std::wstring& code) {
    std::vector<std::wstring> candidates;
    if (!code.empty()) {
        candidates.push_back(code);
        std::wstring root = code.substr(0, code.find(L'_'));
        if (root != code) candidates.push_back(root);
    } else {
        // Following Windows: a language without a translation gets English.
        candidates = SystemLanguages();
        candidates.push_back(FALLBACK_LANGUAGE);
    }
    return LoadCatalog(candidates);
}

std::vector<std::wstring> AvailableLanguages() {
    std::vector<std::wstring> found = {SOURCE_LANGUAGE};
    auto add = [&](const std::wstring& code) {
        if (std::find(found.begin(), found.end(), code) == found.end())
            found.push_back(code);
    };
    for (auto& code : EmbeddedLanguages()) add(code);
    WIN32_FIND_DATAW fd;
    std::wstring pattern = JoinPath(JoinPath(ExeDir(), LOCALE_DIR), L"*");
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            std::wstring name = fd.cFileName;
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && name != L"." &&
                name != L".." && FileExists(ExternalCatalogPath(name)))
                add(name);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    // Alphabetical by the name shown on screen, not by code: people look for
    // the name when choosing.
    std::sort(found.begin(), found.end(),
              [](const std::wstring& a, const std::wstring& b) {
                  return ToLower(LanguageName(a)) < ToLower(LanguageName(b));
              });
    return found;
}

std::wstring LanguageName(const std::wstring& code) {
    for (auto& p : NATIVE_NAMES)
        if (code == p.first) return p.second;
    return code;
}

}  // namespace i18n

std::wstring tr(const char* message) {
    for (const Catalog* cat : {g_catalog.get(), g_fallback.get()}) {
        if (!cat) continue;
        auto it = cat->messages.find(message);
        if (it != cat->messages.end() && !it->second[0].empty()) return Widen(it->second[0]);
    }
    return Widen(message);
}

std::wstring ntr(const char* singular, const char* plural, unsigned long n) {
    for (const Catalog* cat : {g_catalog.get(), g_fallback.get()}) {
        if (!cat) continue;
        auto it = cat->messages.find(singular);
        if (it == cat->messages.end()) continue;
        unsigned long index = cat->plural ? Eval(cat->plural.get(), n) : (n != 1 ? 1 : 0);
        const auto& forms = it->second;
        if (index < forms.size() && !forms[index].empty()) return Widen(forms[index]);
    }
    return Widen(n == 1 ? singular : plural);
}

std::wstring Format(const std::wstring& text,
                    std::initializer_list<std::pair<const wchar_t*, std::wstring>> args) {
    std::wstring out;
    size_t i = 0;
    while (i < text.size()) {
        wchar_t c = text[i];
        if ((c == L'{' || c == L'}') && i + 1 < text.size() && text[i + 1] == c) {
            out += c;
            i += 2;
            continue;
        }
        if (c == L'{') {
            size_t close = text.find(L'}', i);
            if (close != std::wstring::npos) {
                std::wstring name = text.substr(i + 1, close - i - 1);
                bool matched = false;
                for (auto& a : args) {
                    if (name == a.first) {
                        out += a.second;
                        matched = true;
                        break;
                    }
                }
                if (matched) {
                    i = close + 1;
                    continue;
                }
            }
        }
        out += c;
        ++i;
    }
    return out;
}
