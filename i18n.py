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


def _load_translation():
    """Procura o catálogo do idioma, do mais específico para o mais genérico.

    A busca é feita à mão, e não com gettext.translation, porque o gettext
    normaliza os nomes: ele transforma 'pt' em 'pt_PT' e, com isso, entregaria
    o português de Portugal a quem usa o Windows em português do Brasil. Aqui,
    quem não tem catálogo próprio simplesmente vê os textos originais.
    """
    localedir = _locale_path()
    for lang in system_languages():
        caminho = os.path.join(localedir, lang, 'LC_MESSAGES', DOMAIN + '.mo')
        if os.path.isfile(caminho):
            with open(caminho, 'rb') as f:
                return gettext.GNUTranslations(f), lang
    return gettext.NullTranslations(), None


translation, language = _load_translation()

_        = translation.gettext
ngettext = translation.ngettext
