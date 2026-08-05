# VB Recorder - Gravador de áudio do Windows
# Copyright (C) 2026 Vitor Bruski
#
# Este programa é software livre: você pode redistribuí-lo e/ou modificá-lo sob
# os termos da GNU General Public License, publicada pela Free Software
# Foundation, na versão 3 da licença ou, a seu critério, em qualquer versão
# posterior.
#
# Este programa é distribuído na esperança de que seja útil, mas SEM NENHUMA
# GARANTIA. Veja a GNU General Public License para mais detalhes.
#
# Uma cópia da licença acompanha este programa, no arquivo LICENSE. Se não,
# veja <https://www.gnu.org/licenses/>.

"""Tradução do programa.

O idioma é o da interface do Windows, decidido uma vez ao iniciar. Os textos
originais estão em português do Brasil: quando não há tradução para o idioma
do sistema, é ele que aparece.

Uso nos outros módulos:

    from i18n import _, ngettext
"""

import ctypes
import gettext
import locale
import os
import sys

DOMAIN     = 'vbrecorder'
LOCALE_DIR = 'locale'

# Idioma em que o programa foi escrito: não tem catálogo, é o texto original.
SOURCE_LANGUAGE = 'pt_BR'

# Cada idioma aparece na lista escrito nele mesmo, para que a pessoa reconheça
# o próprio idioma mesmo com o programa aberto numa língua que ela não lê. Um
# idioma novo que não esteja aqui aparece pelo código da pasta, e funciona igual.
NATIVE_NAMES = {
    'de':    'Deutsch',
    'en':    'English',
    'es':    'Español',
    'fr':    'Français',
    'it':    'Italiano',
    'pt_BR': 'Português (Brasil)',
    'pt_PT': 'Português (Portugal)',
    'ru':    'Русский',
    'vi':    'Tiếng Việt',
}


def _locale_path():
    """Pasta locale: a de dentro do executável, ou a que está ao lado dele."""
    base = getattr(sys, '_MEIPASS', os.path.dirname(os.path.abspath(__file__)))
    path = os.path.join(base, LOCALE_DIR)
    if not os.path.isdir(path):
        path = os.path.join(os.path.dirname(os.path.abspath(sys.argv[0])), LOCALE_DIR)
    return path


def _windows_ui_language():
    """Idioma da interface do Windows, no formato do gettext (ex: pt_BR)."""
    try:
        lang_id = ctypes.windll.kernel32.GetUserDefaultUILanguage()
    except (AttributeError, OSError):
        return None
    buf = ctypes.create_unicode_buffer(85)
    try:
        if ctypes.windll.kernel32.LCIDToLocaleName(lang_id, buf, 85, 0) and buf.value:
            return buf.value.replace('-', '_')
    except (AttributeError, OSError):
        pass
    return locale.windows_locale.get(lang_id)


def system_languages():
    """Idiomas a tentar, do mais específico para o mais genérico.

    A variável de ambiente LANGUAGE vem antes, como manda o gettext; é por ela
    que dá para testar um idioma sem mexer nas configurações do Windows.
    """
    langs = []
    for var in ('LANGUAGE', 'LC_ALL', 'LC_MESSAGES', 'LANG'):
        value = os.environ.get(var)
        if value:
            langs.extend(p.split('.')[0] for p in value.split(':') if p)
            break
    if not langs:
        name = _windows_ui_language()
        if name:
            langs.append(name)

    # pt_PT também serve para pt, en_US para en, e assim por diante.
    for name in list(langs):
        root = name.split('_')[0]
        if root and root not in langs:
            langs.append(root)
    return langs


def available_languages():
    """Códigos com catálogo instalado, mais o idioma original do programa.

    Vêm em ordem alfabética pelo nome que aparece na tela, com o idioma
    original no meio dos outros, e não em primeiro lugar: quem escolhe procura
    pelo nome. Lê a pasta locale, então um idioma novo entra sem mexer no código.
    """
    encontrados = [SOURCE_LANGUAGE]
    localedir = _locale_path()
    if os.path.isdir(localedir):
        for nome in os.listdir(localedir):
            if os.path.isfile(os.path.join(localedir, nome, 'LC_MESSAGES', DOMAIN + '.mo')):
                encontrados.append(nome)
    return sorted(encontrados, key=lambda c: language_name(c).lower())


def language_name(code):
    """Nome do idioma escrito nele mesmo; sem cadastro, mostra o próprio código."""
    return NATIVE_NAMES.get(code, code)


def _load_catalog(languages):
    """Procura o catálogo, do nome mais específico para o mais genérico.

    A busca é feita à mão, e não com gettext.translation, porque o gettext
    normaliza os nomes: ele transforma 'pt' em 'pt_PT' e, com isso, entregaria
    o português de Portugal a quem usa o Windows em português do Brasil. Aqui,
    quem não tem catálogo próprio simplesmente vê os textos originais.
    """
    localedir = _locale_path()
    for lang in languages:
        caminho = os.path.join(localedir, lang, 'LC_MESSAGES', DOMAIN + '.mo')
        if os.path.isfile(caminho):
            with open(caminho, 'rb') as f:
                return gettext.GNUTranslations(f), lang
    return gettext.NullTranslations(), None


def set_language(code=None):
    """Passa a usar outro idioma. Sem código, segue o idioma do Windows.

    Vale imediatamente para todo mundo: os módulos importam as funções _ e
    ngettext daqui, e elas consultam o catálogo atual a cada chamada.
    """
    global translation, language
    if code:
        candidatos = [code]
        raiz = code.split('_')[0]
        if raiz != code:
            candidatos.append(raiz)
    else:
        candidatos = system_languages()
    translation, language = _load_catalog(candidatos)
    return language


def _(message):
    return translation.gettext(message)


def ngettext(singular, plural, n):
    return translation.ngettext(singular, plural, n)


translation, language = _load_catalog(system_languages())
