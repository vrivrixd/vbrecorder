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

import wx
from gui import MainFrame

if __name__ == '__main__':
    app = wx.App(False)
    frame = MainFrame()
    frame.Show()
    app.MainLoop()