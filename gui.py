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

import math
import wx
import wx.adv
import os
import sys
import ctypes
from datetime import datetime
from i18n import (_, available_languages, language_name, ngettext, set_language)
from config import (BUFFER_MAXIMO, BUFFER_MINIMO, BUFFER_PADRAO, VERSION,
                    ConfigManager)
from audio_engine import AudioEngine
from process_audio import get_audio_sessions, get_process_name, PROCESS_LOOPBACK_AVAILABLE

MB_YESNO           = 0x00000004
MB_ICONEXCLAMATION = 0x00000030
MB_DEFBUTTON1      = 0x00000000
MB_SETFOREGROUND   = 0x00010000
IDYES              = 6

_MessageBoxW = ctypes.windll.user32.MessageBoxW
_MessageBoxW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_uint]
_MessageBoxW.restype  = ctypes.c_int


def confirmar_sim_nao(parent_hwnd, mensagem, titulo):
    """Pergunta Sim/Não nativa: ícone de aviso (com o som do Windows) e foco no Sim.

    O ícone de interrogação não é usado porque o som SystemQuestion vem sem
    atribuição no Windows 10/11 e a caixa sairia muda. MB_SETFOREGROUND garante
    que ela venha à frente quando é chamada pelo menu da bandeja.
    """
    return _MessageBoxW(parent_hwnd, mensagem, titulo,
                        MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON1
                        | MB_SETFOREGROUND) == IDYES


class HotkeyInputCtrl(wx.TextCtrl):
    def __init__(self, parent, initial_value=''):
        super().__init__(parent, style=wx.TE_CENTER)
        self.SetValue(initial_value)
        self.Bind(wx.EVT_KEY_DOWN, self.on_key_down)
        self.Bind(wx.EVT_CHAR, self.on_char)

    def on_char(self, event):
        pass

    def on_key_down(self, event):
        keycode = event.GetKeyCode()
        if keycode in (wx.WXK_TAB, wx.WXK_ESCAPE, wx.WXK_RETURN, wx.WXK_NUMPAD_ENTER,
                       wx.WXK_UP, wx.WXK_DOWN, wx.WXK_LEFT, wx.WXK_RIGHT):
            event.Skip(); return
        if keycode in (wx.WXK_BACK, wx.WXK_DELETE):
            self.SetValue(""); return
        if keycode in (wx.WXK_CONTROL, wx.WXK_ALT, wx.WXK_SHIFT,
                       wx.WXK_WINDOWS_LEFT, wx.WXK_WINDOWS_RIGHT, wx.WXK_COMMAND):
            event.Skip(); return
        mods = []
        if event.ControlDown(): mods.append("Ctrl")
        if event.AltDown():     mods.append("Alt")
        if event.ShiftDown():   mods.append("Shift")
        if event.MetaDown() or wx.GetKeyState(wx.WXK_WINDOWS_LEFT) or wx.GetKeyState(wx.WXK_WINDOWS_RIGHT):
            mods.append("Win")
        key_name = ""
        if 32 <= keycode <= 126:
            key_name = chr(keycode).upper()
        else:
            for attr in dir(wx):
                if attr.startswith("WXK_") and getattr(wx, attr) == keycode:
                    key_name = attr[4:]; break
        if key_name:
            self.SetValue("+".join(mods) + "+" + key_name if mods else key_name)


class HotkeysDialog(wx.Dialog):
    def __init__(self, parent, config):
        super().__init__(parent, title=_("Configurar Teclas"))
        self.config = config
        self.setup_ui()

    def setup_ui(self):
        vbox = wx.BoxSizer(wx.VERTICAL)

        help_txt = wx.StaticText(self, label=_(
            "Pressione a combinação de teclas (ex: Ctrl+Alt+R) nos campos abaixo.\n"
            "Para limpar, pressione Backspace ou Delete.\nPressione Esc para cancelar."
        ))
        help_txt.Wrap(400)
        vbox.Add(help_txt, 0, wx.ALL, 10)

        grid = wx.FlexGridSizer(cols=2, vgap=8, hgap=8)
        grid.AddGrowableCol(1, 1)
        self.ctrls = {}
        for key, label in [('gravar', _('Gravar:')), ('pausar', _('Pausar / Continuar:')),
                            ('parar', _('Parar Gravação:')), ('mostrar', _('Mostrar Janela Principal:')),
                            ('fechar', _('Fechar Programa:'))]:
            lbl  = wx.StaticText(self, label=label)
            ctrl = HotkeyInputCtrl(self, self.config.get_key(key) or '')
            ctrl.SetToolTip(label.replace(':', '').strip())
            grid.Add(lbl,  0, wx.ALIGN_CENTER_VERTICAL)
            grid.Add(ctrl, 1, wx.EXPAND)
            self.ctrls[key] = ctrl

        vbox.Add(grid, 0, wx.ALL | wx.EXPAND, 10)

        hb = wx.BoxSizer(wx.HORIZONTAL)
        btn_s = wx.Button(self, label=_("&Ok"))
        btn_s.Bind(wx.EVT_BUTTON, self.on_save)
        btn_c = wx.Button(self, label=_("&Cancelar"))
        btn_c.Bind(wx.EVT_BUTTON, lambda e: self.EndModal(wx.ID_CANCEL))
        hb.Add(btn_s, 0, wx.RIGHT, 10); hb.Add(btn_c)
        vbox.Add(hb, 0, wx.ALIGN_CENTER | wx.ALL, 15)
        self.SetSizerAndFit(vbox)

    def on_save(self, event):
        for key, ctrl in self.ctrls.items():
            self.config.set_key(key, ctrl.GetValue())
        self.config.save()
        self.EndModal(wx.ID_OK)


class SettingsDialog(wx.Dialog):
    """Configurações do programa. As teclas de atalho ficam num diálogo à parte,
    aberto pelo botão daqui."""

    def __init__(self, parent, config):
        super().__init__(parent, title=_("Configurações"))
        self.config = config
        self.setup_ui()

    def setup_ui(self):
        vbox = wx.BoxSizer(wx.VERTICAL)

        # A lista traz os idiomas escritos neles mesmos, e o primeiro item deixa
        # o programa seguir o Windows, como antes de existir esta opção.
        self._codigos = [''] + available_languages()
        escolhas = [_("Padrão do sistema")] + [language_name(c) for c in self._codigos[1:]]
        grid = wx.FlexGridSizer(cols=2, vgap=8, hgap=8)
        grid.AddGrowableCol(1, 1)
        grid.Add(wx.StaticText(self, label=_("&Idioma:")), 0, wx.ALIGN_CENTER_VERTICAL)
        self.cmb_lang = wx.ComboBox(self, choices=escolhas, style=wx.CB_READONLY)
        atual = self.config.get('idioma') or ''
        self.cmb_lang.SetSelection(self._codigos.index(atual) if atual in self._codigos else 0)
        grid.Add(self.cmb_lang, 1, wx.EXPAND)
        vbox.Add(grid, 0, wx.ALL | wx.EXPAND, 10)

        self.chk_start_min = wx.CheckBox(self, label=_("Iniciar &Minimizado"))
        self.chk_start_min.SetValue(self.config.getboolean('iniciar_minimizado'))
        vbox.Add(self.chk_start_min, 0, wx.LEFT | wx.RIGHT | wx.BOTTOM, 10)

        self.chk_tray = wx.CheckBox(self, label=_("Minimizar Para a &Bandeja do Sistema"))
        self.chk_tray.SetValue(self.config.getboolean('minimizar_bandeja'))
        vbox.Add(self.chk_tray, 0, wx.LEFT | wx.RIGHT | wx.BOTTOM, 10)

        self.chk_on_top = wx.CheckBox(self, label=_("Janela Sempre no &Topo"))
        self.chk_on_top.SetValue(self.config.getboolean('sempre_no_topo', True))
        vbox.Add(self.chk_on_top, 0, wx.LEFT | wx.RIGHT | wx.BOTTOM, 10)

        hb_buf = wx.BoxSizer(wx.HORIZONTAL)
        hb_buf.Add(wx.StaticText(self, label=_("Tamanho do B&uffer (ms):")),
                   0, wx.ALIGN_CENTER_VERTICAL | wx.RIGHT, 5)
        self.txt_buffer = wx.TextCtrl(self, value=str(self.config.get_buffer()),
                                      size=(80, -1))
        # A explicação vai na dica do campo, e não num texto solto no diálogo:
        # texto solto é lido pelo leitor de tela na abertura, antes de tudo.
        self.txt_buffer.SetToolTip(_(
            "Aumente o buffer se o áudio estalar em um computador ocupado. "
            "De {minimo} a {maximo} ms; o padrão é {padrao}.").format(
                minimo=BUFFER_MINIMO, maximo=BUFFER_MAXIMO, padrao=BUFFER_PADRAO))
        hb_buf.Add(self.txt_buffer, 0, wx.ALIGN_CENTER_VERTICAL)
        vbox.Add(hb_buf, 0, wx.LEFT | wx.RIGHT | wx.BOTTOM, 10)

        self.btn_keys = wx.Button(self, label=_("Configurar Teclas de &Atalho..."))
        self.btn_keys.Bind(wx.EVT_BUTTON, self.on_config_keys)
        vbox.Add(self.btn_keys, 0, wx.LEFT | wx.RIGHT | wx.BOTTOM, 10)

        hb = wx.BoxSizer(wx.HORIZONTAL)
        self.btn_ok = wx.Button(self, wx.ID_OK, label=_("&Ok"))
        self.btn_ok.Bind(wx.EVT_BUTTON, self.on_ok)
        self.btn_cancel = wx.Button(self, wx.ID_CANCEL, label=_("&Cancelar"))
        hb.Add(self.btn_ok, 0, wx.RIGHT, 10)
        hb.Add(self.btn_cancel)
        vbox.Add(hb, 0, wx.ALIGN_CENTER | wx.ALL, 15)

        self.Bind(wx.EVT_CHAR_HOOK, self._on_char_hook)
        self.SetSizerAndFit(vbox)

    def _on_char_hook(self, event):
        """Enter confirma, venha de onde vier — menos de cima de um botão, onde
        ele faz o que aquele botão faz."""
        if (event.GetKeyCode() in (wx.WXK_RETURN, wx.WXK_NUMPAD_ENTER)
                and self.FindFocus() not in (self.btn_ok, self.btn_cancel,
                                             self.btn_keys)):
            self.on_ok(None)
        else:
            event.Skip()

    def on_config_keys(self, event):
        dlg = HotkeysDialog(self, self.config)
        salvou = dlg.ShowModal() == wx.ID_OK
        dlg.Destroy()
        if salvou:
            self.GetParent().register_all_hotkeys()

    def buffer_value(self):
        """Buffer digitado, ou None se não for um número válido na faixa."""
        try:
            valor = int(self.txt_buffer.GetValue().strip())
        except ValueError:
            return None
        return valor if BUFFER_MINIMO <= valor <= BUFFER_MAXIMO else None

    def language_code(self):
        sel = self.cmb_lang.GetSelection()
        return self._codigos[sel] if sel != wx.NOT_FOUND else ''

    def on_ok(self, event):
        if self.buffer_value() is None:
            wx.MessageBox(
                _("O tamanho do buffer deve ser um número entre {minimo} e "
                  "{maximo} milissegundos.").format(minimo=BUFFER_MINIMO,
                                                    maximo=BUFFER_MAXIMO),
                _("Aviso"), wx.ICON_WARNING | wx.OK, self)
            self.txt_buffer.SetFocus()
            self.txt_buffer.SelectAll()
            return
        self.EndModal(wx.ID_OK)


class AddSourceDialog(wx.Dialog):
    def __init__(self, parent, audio_engine, existing_sources, edit_source=None):
        title = (_("Editar Fonte de Gravação") if edit_source
                 else _("Adicionar Fonte de Gravação"))
        super().__init__(parent, title=title)
        self.selected = None
        self.audio    = audio_engine
        self._testing = False

        all_devices = audio_engine.get_input_devices()
        processes   = get_audio_sessions()

        existing_ids = {(s['type'], s['id']) for s in existing_sources}
        if edit_source:
            existing_ids.discard((edit_source['type'], edit_source['id']))

        self._device_items  = []
        self._process_items = []
        for dev_idx, dev_name in all_devices:
            if ('device', dev_idx) not in existing_ids:
                self._device_items.append(('device', dev_idx, dev_name))
        if PROCESS_LOOPBACK_AVAILABLE:
            for pid, proc_name in processes:
                if ('process', pid) not in existing_ids:
                    self._process_items.append(('process', pid, f"{proc_name}  (PID {pid})"))

        vbox = wx.BoxSizer(wx.VERTICAL)

        self._rb = wx.RadioBox(self, label=_("Filtrar Fontes Por:"),
                               choices=[_("Dispositivos de Áudio"), _("Processos em Execução")],
                               majorDimension=1, style=wx.RA_SPECIFY_ROWS)
        if not PROCESS_LOOPBACK_AVAILABLE:
            self._rb.EnableItem(1, False)
        self._rb.Bind(wx.EVT_RADIOBOX, self._on_filter_change)
        vbox.Add(self._rb, 0, wx.ALL | wx.EXPAND, 10)

        lbl_src = wx.StaticText(self, label=_("Selecionar Fonte:"))
        self._cmb = wx.ComboBox(self, choices=[], style=wx.CB_READONLY)
        vbox.Add(lbl_src,   0, wx.LEFT | wx.RIGHT, 10)
        vbox.Add(self._cmb, 0, wx.LEFT | wx.RIGHT | wx.BOTTOM | wx.EXPAND, 10)
        self._cmb.Bind(wx.EVT_COMBOBOX, self._on_source_change)

        vol_box = wx.BoxSizer(wx.HORIZONTAL)
        lbl_vol      = wx.StaticText(self, label=_("Volume:"))
        self._sld    = wx.Slider(self, value=50, minValue=0, maxValue=100,
                                 style=wx.SL_HORIZONTAL)
        self._lbl_db = wx.StaticText(self, label="  0.0 dB", size=(70, -1))
        vol_box.Add(lbl_vol,       0, wx.ALIGN_CENTER_VERTICAL | wx.RIGHT, 5)
        vol_box.Add(self._sld,     1, wx.EXPAND | wx.RIGHT, 5)
        vol_box.Add(self._lbl_db,  0, wx.ALIGN_CENTER_VERTICAL)
        vbox.Add(vol_box, 0, wx.LEFT | wx.RIGHT | wx.BOTTOM | wx.EXPAND, 10)
        self._sld.Bind(wx.EVT_SLIDER,   self._on_slider)
        self._sld.Bind(wx.EVT_KEY_DOWN, self._on_slider_key)

        self.btn_test = wx.Button(self, label=_("&Testar"))
        self.btn_test.SetToolTip(_("Ouvir o dispositivo selecionado com o volume ajustado"))
        self.btn_test.Bind(wx.EVT_BUTTON, self._on_test)
        vbox.Add(self.btn_test, 0, wx.LEFT | wx.RIGHT | wx.BOTTOM, 10)

        self.Bind(wx.EVT_CHAR_HOOK, self._on_char_hook)

        if not PROCESS_LOOPBACK_AVAILABLE:
            note = wx.StaticText(self, label=_(
                "Nota: captura por processo requer Windows 10 v2004 ou superior."))
            note.SetForegroundColour(wx.Colour(120, 120, 120))
            vbox.Add(note, 0, wx.LEFT | wx.BOTTOM, 10)

        hb = wx.BoxSizer(wx.HORIZONTAL)
        self.btn_ok  = wx.Button(self, wx.ID_OK,     label=_("&Ok"))
        btn_cancel   = wx.Button(self, wx.ID_CANCEL, label=_("&Cancelar"))
        self.btn_ok.Bind(wx.EVT_BUTTON, self.on_ok)
        hb.Add(self.btn_ok, 0, wx.RIGHT, 5); hb.Add(btn_cancel)
        vbox.Add(hb, 0, wx.ALIGN_CENTER | wx.ALL, 15)
        self.SetSizerAndFit(vbox)

        if edit_source and edit_source['type'] == 'process':
            self._rb.SetSelection(1)
            self._populate_filter(1)
        else:
            self._populate_filter(0)

        if edit_source:
            for i, item in enumerate(self._current_items):
                if item[0] == edit_source['type'] and item[1] == edit_source['id']:
                    self._cmb.SetSelection(i)
                    break
            vol = edit_source.get('volume', 1.0)
            db  = 20 * math.log10(vol) if vol > 0 else -20.0
            slider_val = min(100, max(0, int(round(db / 0.4 + 50))))
            self._sld.SetValue(slider_val)

        self._update_db_label()

    def _on_char_hook(self, event):
        if (event.GetKeyCode() in (wx.WXK_RETURN, wx.WXK_NUMPAD_ENTER)
                and self.FindFocus() is not self.btn_test):
            self.on_ok(None)
        else:
            event.Skip()

    def _populate_filter(self, sel):
        self._current_items = self._device_items if sel == 0 else self._process_items
        choices = [item[2] for item in self._current_items]
        self._cmb.SetItems(choices)
        if choices:
            self._cmb.SetSelection(0)
            self.btn_ok.Enable()
        else:
            self.btn_ok.Disable()
        self._update_test_state()

    def _on_filter_change(self, event):
        self._stop_test()
        self._populate_filter(self._rb.GetSelection())

    def _on_source_change(self, event):
        self._stop_test()
        self._update_test_state()

    def _selected_device(self):
        """Índice do dispositivo selecionado, ou None se a fonte não for um dispositivo."""
        sel = self._cmb.GetSelection()
        if sel == wx.NOT_FOUND or self._current_items is not self._device_items:
            return None
        return self._current_items[sel][1]

    def _update_test_state(self):
        self.btn_test.Enable(self._selected_device() is not None)

    def _on_test(self, event):
        if self._testing:
            self._stop_test()
            return
        dev_idx = self._selected_device()
        if dev_idx is None:
            return
        rate, channels = self._output_format()
        ok, err = self.audio.start_monitor(dev_idx, self._volume(), rate, channels)
        if not ok:
            wx.MessageBox(
                _("Não foi possível testar este dispositivo (erro BASS {erro}).").format(erro=err),
                _("Erro"), wx.ICON_ERROR | wx.OK, self)
            return
        self._testing = True
        self.btn_test.SetLabel(_("&Parar"))

    def _stop_test(self):
        if not self._testing:
            return
        self.audio.stop_monitor()
        self._testing = False
        self.btn_test.SetLabel(_("&Testar"))

    def _output_format(self):
        """Taxa e canais da janela principal, para o teste soar como a gravação."""
        parent = self.GetParent()
        try:
            rate     = int(parent.cmb_rate.GetValue())
            channels = 1 if parent.cmb_channels.GetValue() == 'mono' else 2
            return rate, channels
        except (AttributeError, ValueError):
            return 44100, 2

    def _volume(self):
        return 10 ** (((self._sld.GetValue() - 50) * 0.4) / 20.0)

    def _on_slider(self, event):
        self._update_db_label()

    def _on_slider_key(self, event):
        key = event.GetKeyCode()
        val = self._sld.GetValue()
        new_val = None
        if key == wx.WXK_HOME:
            new_val = 0
        elif key == wx.WXK_END:
            new_val = 100
        elif key in (wx.WXK_UP, wx.WXK_RIGHT):
            new_val = min(100, val + 1)
        elif key in (wx.WXK_DOWN, wx.WXK_LEFT):
            new_val = max(0, val - 1)
        elif key == wx.WXK_PAGEUP:
            new_val = min(100, (val // 5 + 1) * 5)
        elif key == wx.WXK_PAGEDOWN:
            new_val = max(0, (val - 1) // 5 * 5)
        if new_val is not None:
            self._sld.SetValue(new_val)
            self._update_db_label()
        else:
            event.Skip()

    def _update_db_label(self):
        db = (self._sld.GetValue() - 50) * 0.4
        self._lbl_db.SetLabel(f"  {db:+.1f} dB")
        if self._testing:
            self.audio.set_monitor_volume(self._volume())

    def on_ok(self, event):
        self._stop_test()
        sel = self._cmb.GetSelection()
        if sel == wx.NOT_FOUND or not self._current_items:
            self.EndModal(wx.ID_CANCEL); return
        kind, uid, name = self._current_items[sel]
        self.selected = {
            'type': kind, 'id': uid, 'name': name,
            'volume': self._volume()
        }
        self.EndModal(wx.ID_OK)


class VBTrayIcon(wx.adv.TaskBarIcon):
    def __init__(self, frame):
        super().__init__()
        self.frame = frame
        icon = wx.ArtProvider.GetIcon(wx.ART_INFORMATION, wx.ART_OTHER, (16, 16))
        self.SetIcon(icon, "VB Recorder")
        self.Bind(wx.adv.EVT_TASKBAR_LEFT_DCLICK, self.on_show)

    def CreatePopupMenu(self):
        menu = wx.Menu()
        if not self.frame.audio.is_recording:
            item_rec = menu.Append(wx.ID_ANY, _("Gravar"))
            self.Bind(wx.EVT_MENU, self.frame.on_record_toggle, item_rec)
            if self.frame.has_recording():
                label_play = (_("Parar Reprodução") if self.frame.audio.is_playing
                              else _("Reproduzir Gravação"))
                item_play = menu.Append(wx.ID_ANY, label_play)
                self.Bind(wx.EVT_MENU, self.frame.on_play_toggle, item_play)
                item_del = menu.Append(wx.ID_ANY, _("Excluir Gravação"))
                self.Bind(wx.EVT_MENU, self.frame.on_delete_recording, item_del)
            menu.AppendSeparator()
            self._add_submenu(menu, _("Formato de Saída"), self.frame.cmb_format)
            if self.frame.cmb_format.GetValue() in ['mp3', 'ogg', 'opus']:
                self._add_submenu(menu, _("Bitrate"), self.frame.cmb_bitrate)
            if self.frame.cmb_format.GetValue() == 'flac':
                self._add_submenu(menu, _("Compressão FLAC"), self.frame.cmb_flac_level)
            self._add_submenu(menu, _("Taxa de Amostragem"), self.frame.cmb_rate)
            self._add_submenu(menu, _("Canais"), self.frame.cmb_channels)
            if len(self.frame.sources) >= 2:
                item_sep = menu.AppendCheckItem(wx.ID_ANY, _("Gravar fontes em arquivos separados"))
                item_sep.Check(self.frame.chk_separate.GetValue())
                self.Bind(wx.EVT_MENU, self.on_toggle_separate, item_sep)
            item_tones = menu.AppendCheckItem(wx.ID_ANY, _("Reproduzir tons ao iniciar ou parar gravação"))
            item_tones.Check(self.frame.chk_tones.GetValue())
            self.Bind(wx.EVT_MENU, self.on_toggle_tones, item_tones)
        else:
            label_pause = _("Continuar") if self.frame.audio.is_paused else _("Pausar")
            item_pause = menu.Append(wx.ID_ANY, label_pause)
            self.Bind(wx.EVT_MENU, self.frame.on_pause_toggle, item_pause)
            item_stop = menu.Append(wx.ID_ANY, _("Parar"))
            self.Bind(wx.EVT_MENU, self.frame.on_record_toggle, item_stop)
        menu.AppendSeparator()
        item_show = menu.Append(wx.ID_ANY, _("Mostrar janela principal"))
        self.Bind(wx.EVT_MENU, self.on_show, item_show)
        item_exit = menu.Append(wx.ID_ANY, _("Sair"))
        self.Bind(wx.EVT_MENU, self.on_exit, item_exit)
        return menu

    def _add_submenu(self, parent_menu, label, combo):
        submenu = wx.Menu()
        for item in combo.GetItems():
            menu_item = submenu.AppendRadioItem(wx.ID_ANY, item)
            if item == combo.GetValue(): menu_item.Check(True)
            self.Bind(wx.EVT_MENU, lambda e, c=combo, v=item: self.on_combo_change(c, v), menu_item)
        parent_menu.AppendSubMenu(submenu, label)

    def on_combo_change(self, combo, val):
        combo.SetValue(val)
        self.frame.save_settings()
        if combo == self.frame.cmb_format: self.frame.on_format_change()

    def on_toggle_separate(self, event):
        self.frame.chk_separate.SetValue(event.IsChecked())
        self.frame.save_settings()

    def on_toggle_tones(self, event):
        self.frame.chk_tones.SetValue(event.IsChecked())
        self.frame.save_settings()

    def on_show(self, event):
        self.frame.Show()
        self.frame.Restore()

    def on_exit(self, event): self.frame.Close()


class MainFrame(wx.Frame):
    def __init__(self):
        # A configuração vem antes de tudo: é dela que sai o idioma das
        # mensagens, inclusive a de que o programa já está aberto.
        self.config = ConfigManager()
        set_language(self.config.get('idioma'))

        self.mutex = ctypes.windll.kernel32.CreateMutexW(None, False, "VBRecorder_Mutex")
        if ctypes.windll.kernel32.GetLastError() == 183:
            wx.MessageBox(_("O VB Recorder já está em execução."), _("Aviso"),
                          wx.ICON_WARNING | wx.OK)
            sys.exit(0)

        super().__init__(parent=None, title='VB Recorder', size=(600, 850))
        self.panel   = wx.Panel(self)
        self.audio   = AudioEngine()
        self.audio.set_buffer(self.config.get_buffer())
        self.sources = []

        self.last_recording = []
        self.play_timer     = wx.Timer(self)
        self.Bind(wx.EVT_TIMER, self._on_play_timer, self.play_timer)

        self.setup_hotkey_ids()
        self.setup_ui()
        self.load_settings()

        self.apply_always_on_top()

        self.tray_icon = VBTrayIcon(self)
        self.Bind(wx.EVT_ICONIZE,   self.on_minimize)
        self.Bind(wx.EVT_CLOSE,     self.on_close)
        self.Bind(wx.EVT_HOTKEY,    self.on_hotkey)
        self.Bind(wx.EVT_CHAR_HOOK, self.on_char_hook)
        self.register_all_hotkeys()

    def minimiza_para_bandeja(self):
        return self.config.getboolean('minimizar_bandeja')

    def apply_always_on_top(self):
        no_topo = self.config.getboolean('sempre_no_topo', True)
        estilo  = self.GetWindowStyleFlag()
        self.SetWindowStyleFlag(estilo | wx.STAY_ON_TOP if no_topo
                                else estilo & ~wx.STAY_ON_TOP)

    def on_char_hook(self, event):
        """Esc minimiza, ou fecha o programa quando a bandeja está desativada."""
        if event.GetKeyCode() == wx.WXK_ESCAPE:
            if self.minimiza_para_bandeja():
                self.Iconize(True)
            else:
                self.Close()
        else:
            event.Skip()

    def setup_hotkey_ids(self):
        self.HK_REC   = wx.NewIdRef().GetId() if hasattr(wx, 'NewIdRef') else wx.NewId()
        self.HK_PAUSE = wx.NewIdRef().GetId() if hasattr(wx, 'NewIdRef') else wx.NewId()
        self.HK_STOP  = wx.NewIdRef().GetId() if hasattr(wx, 'NewIdRef') else wx.NewId()
        self.HK_SHOW  = wx.NewIdRef().GetId() if hasattr(wx, 'NewIdRef') else wx.NewId()
        self.HK_EXIT  = wx.NewIdRef().GetId() if hasattr(wx, 'NewIdRef') else wx.NewId()

    def parse_hotkey_str(self, hk_str):
        if not hk_str: return 0, 0
        mods, parts = 0, hk_str.upper().split('+')
        for p in parts[:-1]:
            if p == 'CTRL':  mods |= wx.MOD_CONTROL
            elif p == 'ALT':   mods |= wx.MOD_ALT
            elif p == 'SHIFT': mods |= wx.MOD_SHIFT
            elif p == 'WIN':   mods |= wx.MOD_WIN
        key = parts[-1].strip()
        code = ord(key) if len(key) == 1 else getattr(wx, f"WXK_{key}", 0)
        return mods, code

    def register_all_hotkeys(self):
        for hid in [self.HK_REC, self.HK_PAUSE, self.HK_STOP, self.HK_SHOW, self.HK_EXIT]:
            self.UnregisterHotKey(hid)
        keys_map = {
            self.HK_REC: 'gravar', self.HK_PAUSE: 'pausar',
            self.HK_STOP: 'parar', self.HK_SHOW: 'mostrar', self.HK_EXIT: 'fechar'
        }
        for hid, key_name in keys_map.items():
            mods, code = self.parse_hotkey_str(self.config.get_key(key_name))
            if code: self.RegisterHotKey(hid, mods, code)

    def on_hotkey(self, event):
        hid = event.GetId()
        if hid == self.HK_REC   and not self.audio.is_recording: self.on_record_toggle(None)
        elif hid == self.HK_PAUSE and self.audio.is_recording:     self.on_pause_toggle(None)
        elif hid == self.HK_STOP  and self.audio.is_recording:     self.on_record_toggle(None)
        elif hid == self.HK_SHOW: self.Show(); self.Restore()
        elif hid == self.HK_EXIT: self.Close()

    def setup_ui(self):
        """Monta os controles. Os textos ficam todos em _retranslate()."""
        self.vbox = wx.BoxSizer(wx.VERTICAL)

        hbox_ctrl = wx.BoxSizer(wx.HORIZONTAL)
        self.btn_record = wx.Button(self.panel)
        self.btn_record.Bind(wx.EVT_BUTTON, self.on_record_toggle)
        self.btn_pause = wx.Button(self.panel)
        self.btn_pause.Bind(wx.EVT_BUTTON, self.on_pause_toggle)
        self.btn_pause.Disable()
        self.btn_play = wx.Button(self.panel)
        self.btn_play.Bind(wx.EVT_BUTTON, self.on_play_toggle)
        self.btn_play.Disable()
        self.btn_delete = wx.Button(self.panel)
        self.btn_delete.Bind(wx.EVT_BUTTON, self.on_delete_recording)
        self.btn_delete.Disable()
        hbox_ctrl.Add(self.btn_record, 0, wx.ALL, 5)
        hbox_ctrl.Add(self.btn_pause,  0, wx.ALL, 5)
        hbox_ctrl.Add(self.btn_play,   0, wx.ALL, 5)
        hbox_ctrl.Add(self.btn_delete, 0, wx.ALL, 5)
        self.vbox.Add(hbox_ctrl, 0, wx.ALIGN_CENTER | wx.ALL, 10)

        self.lbl_format, self.cmb_format = self.create_labeled_combo(
            self.vbox, ['mp3', 'wav', 'aiff', 'ogg', 'opus', 'flac'])
        self.cmb_format.Bind(wx.EVT_COMBOBOX, self.on_format_change)

        self.lbl_bitrate = wx.StaticText(self.panel)
        self.cmb_bitrate = wx.ComboBox(self.panel, style=wx.CB_READONLY,
            choices=['8 KBPS', '16 KBPS', '32 KBPS', '64 KBPS', '96 KBPS',
                     '128 KBPS', '160 KBPS', '192 KBPS', '256 KBPS', '320 KBPS'])
        hbox_bitrate = wx.BoxSizer(wx.HORIZONTAL)
        hbox_bitrate.Add(self.lbl_bitrate, 0, wx.ALIGN_CENTER_VERTICAL | wx.RIGHT, 5)
        hbox_bitrate.Add(self.cmb_bitrate, 1, wx.EXPAND)
        self.vbox.Add(hbox_bitrate, 0, wx.ALL | wx.EXPAND, 5)

        self.lbl_flac_level = wx.StaticText(self.panel)
        self.cmb_flac_level = wx.ComboBox(self.panel, style=wx.CB_READONLY,
            choices=[str(i) for i in range(9)])
        hbox_flac = wx.BoxSizer(wx.HORIZONTAL)
        hbox_flac.Add(self.lbl_flac_level, 0, wx.ALIGN_CENTER_VERTICAL | wx.RIGHT, 5)
        hbox_flac.Add(self.cmb_flac_level, 1, wx.EXPAND)
        self.vbox.Add(hbox_flac, 0, wx.ALL | wx.EXPAND, 5)

        self.lbl_rate, self.cmb_rate = self.create_labeled_combo(
            self.vbox, ['8000', '11025', '22050', '32000', '44100', '48000'])
        self.lbl_channels, self.cmb_channels = self.create_labeled_combo(
            self.vbox, ['mono', 'stereo'])

        self.lbl_sources = wx.StaticText(self.panel)
        self.vbox.Add(self.lbl_sources, 0, wx.LEFT | wx.TOP, 10)
        self.lst_fontes = wx.ListCtrl(self.panel,
                                      style=wx.LC_REPORT | wx.LC_NO_HEADER | wx.LC_HRULES)
        self.lst_fontes.AppendColumn('', width=wx.LIST_AUTOSIZE_USEHEADER)
        self.lst_fontes.Bind(wx.EVT_KEY_DOWN, self._on_list_key)
        self.vbox.Add(self.lst_fontes, 1, wx.ALL | wx.EXPAND, 5)

        hb_src = wx.BoxSizer(wx.HORIZONTAL)
        self.btn_add  = wx.Button(self.panel)
        self.btn_edit = wx.Button(self.panel)
        self.btn_rem  = wx.Button(self.panel)
        self.btn_add.Bind(wx.EVT_BUTTON,  self.on_add_source)
        self.btn_edit.Bind(wx.EVT_BUTTON, self.on_edit_source)
        self.btn_rem.Bind(wx.EVT_BUTTON,  self.on_remove_source)
        hb_src.Add(self.btn_add,  0, wx.RIGHT, 5)
        hb_src.Add(self.btn_edit, 0, wx.RIGHT, 5)
        hb_src.Add(self.btn_rem)
        self.vbox.Add(hb_src, 0, wx.LEFT | wx.BOTTOM, 10)

        self.chk_separate = wx.CheckBox(self.panel)
        self.vbox.Add(self.chk_separate, 0, wx.ALL, 10)

        self.chk_tones = wx.CheckBox(self.panel)
        self.vbox.Add(self.chk_tones, 0, wx.ALL, 10)

        self.lbl_filename, self.txt_filename = self.create_labeled_input(self.vbox)

        hbox_dir = wx.BoxSizer(wx.HORIZONTAL)
        self.lbl_dir    = wx.StaticText(self.panel)
        self.txt_dir    = wx.TextCtrl(self.panel)
        self.btn_browse = wx.Button(self.panel)
        self.btn_browse.Bind(wx.EVT_BUTTON, self.on_browse)
        self.btn_open_dir = wx.Button(self.panel)
        self.btn_open_dir.Bind(wx.EVT_BUTTON, self.on_open_dir)
        hbox_dir.Add(self.lbl_dir, 0, wx.ALIGN_CENTER_VERTICAL | wx.RIGHT, 5)
        hbox_dir.Add(self.txt_dir, 1, wx.EXPAND | wx.RIGHT, 5)
        hbox_dir.Add(self.btn_browse, 0, wx.RIGHT, 5)
        hbox_dir.Add(self.btn_open_dir, 0)
        self.vbox.Add(hbox_dir, 0, wx.ALL | wx.EXPAND, 10)

        hbox_bottom = wx.BoxSizer(wx.HORIZONTAL)
        self.btn_settings = wx.Button(self.panel)
        self.btn_settings.Bind(wx.EVT_BUTTON, self.on_settings)
        self.btn_about = wx.Button(self.panel)
        self.btn_about.Bind(wx.EVT_BUTTON, self.on_about)
        self.btn_min = wx.Button(self.panel)
        self.btn_min.Bind(wx.EVT_BUTTON, lambda e: self.Iconize(True))
        self.btn_close = wx.Button(self.panel)
        self.btn_close.Bind(wx.EVT_BUTTON, self.on_close)
        hbox_bottom.Add(self.btn_settings, 0, wx.RIGHT, 5)
        hbox_bottom.Add(self.btn_about,    0, wx.RIGHT, 5)
        hbox_bottom.Add(self.btn_min,      0, wx.RIGHT, 5)
        hbox_bottom.Add(self.btn_close,    0)
        self.vbox.Add(hbox_bottom, 0, wx.ALIGN_RIGHT | wx.ALL, 10)

        self.panel.SetSizer(self.vbox)
        self._retranslate()

    def _retranslate(self):
        """Escreve todos os textos da janela no idioma atual.

        É o único lugar onde eles aparecem, então trocar de idioma não deixa
        nada para trás. Os botões que mudam de nome conforme o que o programa
        está fazendo continuam refletindo o estado.
        """
        self.lbl_format.SetLabel(_('&Formato de Saída:'))
        self.lbl_bitrate.SetLabel(_('&Bitrate:'))
        self.lbl_flac_level.SetLabel(_('Compressão F&LAC:'))
        self.lbl_rate.SetLabel(_('&Taxa de Amostragem:'))
        self.lbl_channels.SetLabel(_('&Canais:'))
        self.lbl_sources.SetLabel(_('Fontes de &Gravação:'))
        self.lbl_filename.SetLabel(_('&Nome do Arquivo:'))
        self.lbl_dir.SetLabel(_('&Diretório de Saída:'))

        self.btn_add.SetLabel(_('&Adicionar'))
        self.btn_edit.SetLabel(_('&Editar'))
        self.btn_rem.SetLabel(_('&Remover'))
        self.btn_browse.SetLabel(_('P&rocurar...'))
        self.btn_open_dir.SetLabel(_('Abr&ir Pasta de Gravações'))
        self.btn_settings.SetLabel(_('Config&urações...'))
        self.btn_about.SetLabel(_('S&obre'))
        self.btn_min.SetLabel(_('&Minimizar'))
        self.btn_close.SetLabel(_('&Fechar'))
        self.btn_delete.SetLabel(_('E&xcluir Gravação'))

        self.chk_separate.SetLabel(_('Gravar fontes em arquivos se&parados'))
        self.chk_tones.SetLabel(_('Reproduzir &tons ao iniciar ou parar gravação'))

        self.btn_record.SetLabel(_('Para&r') if self.audio.is_recording else _('&Gravar'))
        self.btn_pause.SetLabel(_('C&ontinuar') if self.audio.is_paused else _('&Pausar'))
        self._update_play_button()

        self._refresh_device_names()
        self.panel.Layout()

    def _refresh_device_names(self):
        """Renomeia as fontes que são dispositivos, para o caso do idioma ter
        mudado: o nome do dispositivo padrão vem traduzido."""
        if not self.sources:
            return
        nomes = dict(self.audio.get_input_devices())
        mudou = False
        for src in self.sources:
            if src['type'] == 'device' and src['id'] in nomes:
                if src['name'] != nomes[src['id']]:
                    src['name'] = nomes[src['id']]
                    mudou = True
        if mudou:
            self._refresh_list()

    def create_labeled_combo(self, sizer, choices):
        hb  = wx.BoxSizer(wx.HORIZONTAL)
        lbl = wx.StaticText(self.panel)
        cmb = wx.ComboBox(self.panel, choices=choices, style=wx.CB_READONLY)
        hb.Add(lbl, 0, wx.ALIGN_CENTER_VERTICAL | wx.RIGHT, 5)
        hb.Add(cmb, 1, wx.EXPAND)
        sizer.Add(hb, 0, wx.ALL | wx.EXPAND, 5)
        return lbl, cmb

    def create_labeled_input(self, sizer):
        hb  = wx.BoxSizer(wx.HORIZONTAL)
        lbl = wx.StaticText(self.panel)
        txt = wx.TextCtrl(self.panel)
        hb.Add(lbl, 0, wx.ALIGN_CENTER_VERTICAL | wx.RIGHT, 5)
        hb.Add(txt, 1, wx.EXPAND)
        sizer.Add(hb, 0, wx.ALL | wx.EXPAND, 5)
        return lbl, txt

    def _refresh_list(self):
        self.lst_fontes.DeleteAllItems()
        for s in self.sources:
            self.lst_fontes.InsertItem(self.lst_fontes.GetItemCount(), s['name'])
        self.lst_fontes.SetColumnWidth(0, wx.LIST_AUTOSIZE)
        has_multi = len(self.sources) >= 2
        self.chk_separate.Show(has_multi)
        self.panel.Layout()

    def on_add_source(self, event):
        dlg = AddSourceDialog(self, self.audio, self.sources)
        result = dlg.ShowModal()
        self.audio.stop_monitor()
        if result == wx.ID_OK and dlg.selected:
            src = dlg.selected
            self.sources.append(src)
            if src['type'] == 'device':
                self.audio.init_device(src['id'])
            self._refresh_list()
            self.save_settings()
        dlg.Destroy()

    def on_edit_source(self, event):
        sel = self.lst_fontes.GetFirstSelected()
        if sel == -1 or self.lst_fontes.GetNextSelected(sel) != -1:
            return
        src = self.sources[sel]
        dlg = AddSourceDialog(self, self.audio, self.sources, edit_source=src)
        result = dlg.ShowModal()
        self.audio.stop_monitor()
        if result == wx.ID_OK and dlg.selected:
            self.sources[sel] = dlg.selected
            if dlg.selected['type'] == 'device':
                self.audio.init_device(dlg.selected['id'])
            self._refresh_list()
            self.save_settings()
        dlg.Destroy()

    def on_remove_source(self, event):
        to_remove = []
        idx = self.lst_fontes.GetFirstSelected()
        while idx != -1:
            to_remove.append(idx)
            idx = self.lst_fontes.GetNextSelected(idx)
        if not to_remove:
            return
        for i in sorted(to_remove, reverse=True):
            del self.sources[i]
        self._refresh_list()
        self.save_settings()

    def _on_list_key(self, event):
        key     = event.GetKeyCode()
        count   = self.lst_fontes.GetItemCount()
        focused = self.lst_fontes.GetFocusedItem()
        if count > 0 and focused != -1:
            if key == wx.WXK_DOWN and focused >= count - 1:
                return
            if key == wx.WXK_UP and focused <= 0:
                return
        event.Skip()

    def on_settings(self, event):
        dlg = SettingsDialog(self, self.config)
        if dlg.ShowModal() == wx.ID_OK:
            idioma_antes = self.config.get('idioma') or ''
            idioma_agora = dlg.language_code()
            self.config.set_config('idioma',             idioma_agora)
            self.config.set_config('iniciar_minimizado', dlg.chk_start_min.GetValue())
            self.config.set_config('minimizar_bandeja',  dlg.chk_tray.GetValue())
            self.config.set_config('sempre_no_topo',     dlg.chk_on_top.GetValue())
            self.config.set_config('buffer',             dlg.buffer_value())
            self.config.save()

            self.audio.set_buffer(dlg.buffer_value())
            self.apply_always_on_top()
            if idioma_agora != idioma_antes:
                set_language(idioma_agora)
                self._retranslate()
        dlg.Destroy()
        dlg.Destroy()

    def on_format_change(self, event=None):
        fmt = self.cmb_format.GetValue()
        needs_br = fmt in ['mp3', 'ogg', 'opus']
        self.lbl_bitrate.Show(needs_br)
        self.cmb_bitrate.Show(needs_br)
        self.lbl_flac_level.Show(fmt == 'flac')
        self.cmb_flac_level.Show(fmt == 'flac')
        if needs_br:
            cur  = self.cmb_bitrate.GetValue()
            base = ['8 KBPS', '16 KBPS', '32 KBPS', '64 KBPS', '96 KBPS',
                    '128 KBPS', '160 KBPS', '192 KBPS', '256 KBPS', '320 KBPS']
            if fmt in ['ogg', 'opus']: base.append('500 KBPS')
            self.cmb_bitrate.SetItems(base)
            self.cmb_bitrate.SetValue(cur if cur in base else '320 KBPS')
        self.panel.Layout()

    def load_settings(self):
        self.cmb_format.SetValue(self.config.get('formato') or 'mp3')
        self.on_format_change()
        self.cmb_bitrate.SetValue(self.config.get('bitrate') or '128 KBPS')
        self.cmb_flac_level.SetValue(self.config.get('flac_nivel') or '8')
        self.cmb_rate.SetValue(self.config.get('taxa') or '44100')
        self.cmb_channels.SetValue(self.config.get('canais') or 'stereo')
        self.chk_tones.SetValue(self.config.getboolean('reproduzir_tons'))
        self.chk_separate.SetValue(self.config.getboolean('arquivos_separados'))
        self.txt_filename.SetValue(self.config.get('arquivo') or '')
        self.txt_dir.SetValue(self.config.get('diretorio') or '')

        all_devices = {idx: name for idx, name in self.audio.get_input_devices()}
        self.sources = []
        for entry in self.config.get_fontes():
            if entry['type'] == 'device':
                name = all_devices.get(entry['id'])
                if name is not None:
                    self.sources.append({
                        'type': 'device', 'id': entry['id'],
                        'name': name, 'volume': entry.get('volume', 1.0)
                    })
                    self.audio.init_device(entry['id'])
            else:
                resolved = get_process_name(entry['id'])
                if resolved is not None:
                    self.sources.append({
                        'type': 'process', 'id': entry['id'],
                        'name': f"{resolved}  (PID {entry['id']})",
                        'volume': entry.get('volume', 1.0)
                    })

        if not self.sources and all_devices:
            first_idx, first_name = next(iter(all_devices.items()))
            self.sources.append({'type': 'device', 'id': first_idx, 'name': first_name, 'volume': 1.0})
            self.audio.init_device(first_idx)

        self._refresh_list()

    def save_settings(self):
        self.config.set_config('formato',            self.cmb_format.GetValue())
        self.config.set_config('bitrate',            self.cmb_bitrate.GetValue())
        self.config.set_config('flac_nivel',         self.cmb_flac_level.GetValue())
        self.config.set_config('taxa',               self.cmb_rate.GetValue())
        self.config.set_config('canais',             self.cmb_channels.GetValue())
        self.config.set_config('reproduzir_tons',    self.chk_tones.GetValue())
        self.config.set_config('arquivos_separados', self.chk_separate.GetValue())
        self.config.set_config('arquivo',            self.txt_filename.GetValue())
        self.config.set_config('diretorio',          self.txt_dir.GetValue())
        self.config.set_fontes(self.sources)
        self.config.save()

    def on_record_toggle(self, event):
        if not self.audio.is_recording:
            self.audio.stop_monitor()
            if not self.sources:
                wx.MessageBox(_("Adicione ao menos uma fonte de gravação antes de iniciar."),
                              _("Aviso"), wx.ICON_WARNING | wx.OK)
                return
            self.audio.stop_playback()
            self._update_play_button()
            if self.chk_tones.GetValue(): self.audio.play_tone(880, 150)
            out_dir = self.current_output_dir()
            if not os.path.exists(out_dir): os.makedirs(out_dir)
            ext      = self.cmb_format.GetValue()
            filename = self.txt_filename.GetValue().strip() or datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
            full_path = os.path.join(out_dir, f"{filename}.{ext}")
            base, counter = filename, 1
            while os.path.exists(full_path):
                filename  = f"{base} {counter}"
                full_path = os.path.join(out_dir, f"{filename}.{ext}")
                counter  += 1
            bitrate_val = self.cmb_bitrate.GetValue().split(' ')[0]
            ok, err = self.audio.start_recording(
                full_path, ext, bitrate_val,
                int(self.cmb_rate.GetValue()), self.cmb_channels.GetValue(),
                self.sources, self.chk_separate.GetValue(),
                int(self.cmb_flac_level.GetValue())
            )
            if ok:
                self.btn_record.SetLabel(_('Para&r'))
                self.btn_pause.Enable()
                self._toggle_ui(False)
                self._update_recording_buttons()
            else:
                wx.MessageBox(_("Erro BASS {erro} ao iniciar a gravação.").format(erro=err),
                              _("Erro"), wx.ICON_ERROR)
        else:
            self.audio.stop_recording()
            if self.chk_tones.GetValue(): self.audio.play_tone(440, 150)
            self.btn_record.SetLabel(_('&Gravar'))
            self.btn_pause.SetLabel(_('&Pausar'))
            self.btn_pause.Disable()
            self._toggle_ui(True)
            self.last_recording = [p for p in self.audio.output_files if os.path.exists(p)]
            self._update_recording_buttons()

    def on_pause_toggle(self, event):
        if not self.audio.is_paused:
            self.audio.pause_recording()
            if self.chk_tones.GetValue(): self.audio.play_tone(659, 150)
            self.btn_pause.SetLabel(_('C&ontinuar'))
        else:
            self.audio.resume_recording()
            if self.chk_tones.GetValue(): self.audio.play_tone(880, 150)
            self.btn_pause.SetLabel(_('&Pausar'))

    def on_browse(self, event):
        dlg = wx.DirDialog(self, _("Selecionar Pasta"), style=wx.DD_DEFAULT_STYLE)
        if dlg.ShowModal() == wx.ID_OK: self.txt_dir.SetValue(dlg.GetPath())
        dlg.Destroy()

    def current_output_dir(self):
        """Pasta em uso: a do campo de edição, ou a padrão se ele estiver vazio."""
        return self.txt_dir.GetValue().strip() or os.path.join(os.getcwd(), _('Gravações'))

    def on_open_dir(self, event):
        out_dir = self.current_output_dir()
        try:
            if not os.path.exists(out_dir):
                os.makedirs(out_dir)
            os.startfile(out_dir)
        except OSError as e:
            wx.MessageBox(
                _("Não foi possível abrir a pasta:\n{pasta}\n\n{erro}").format(pasta=out_dir, erro=e),
                _("Erro"), wx.ICON_ERROR | wx.OK)

    def _existing_recording_files(self):
        return [p for p in self.last_recording if os.path.exists(p)]

    def has_recording(self):
        return bool(self._existing_recording_files()) and not self.audio.is_recording

    def _dialog_parent(self):
        """Sem pai quando a janela está na bandeja: assim o diálogo aparece à frente."""
        return self if self.IsShown() else None

    def _dialog_hwnd(self):
        return self.GetHandle() if self.IsShown() else None

    def _aviso(self, mensagem, titulo, icone=wx.ICON_WARNING):
        wx.MessageBox(mensagem, titulo, icone | wx.OK, self._dialog_parent())

    def _update_recording_buttons(self):
        available = self.has_recording()
        self.btn_play.Enable(available)
        self.btn_delete.Enable(available)
        self._update_play_button()

    def _update_play_button(self):
        playing = self.audio.is_playing
        self.btn_play.SetLabel(_('Parar Reprod&ução') if playing else _('Reprod&uzir Gravação'))
        if playing and not self.play_timer.IsRunning():
            self.play_timer.Start(300)
        elif not playing and self.play_timer.IsRunning():
            self.play_timer.Stop()

    def _on_play_timer(self, event):
        if not self.audio.is_playing:
            self._update_play_button()

    def on_play_toggle(self, event):
        if self.audio.is_playing:
            self.audio.stop_playback()
            self._update_play_button()
            return
        files = self._existing_recording_files()
        if not files:
            self.last_recording = []
            self._update_recording_buttons()
            return
        path = files[0]
        if len(files) > 1:
            parent = self._dialog_parent()
            estilo = wx.CHOICEDLG_STYLE | (wx.STAY_ON_TOP if parent is None else 0)
            dlg = wx.SingleChoiceDialog(parent, _("Escolha o arquivo a reproduzir:"),
                                        _("Reproduzir Gravação"),
                                        [os.path.basename(p) for p in files], estilo)
            dlg.Raise()
            escolhido = dlg.ShowModal() == wx.ID_OK
            if escolhido: path = files[dlg.GetSelection()]
            dlg.Destroy()
            if not escolhido: return
        dll = self.audio.missing_playback_plugin(path)
        if dll:
            self._aviso(
                _("Para reproduzir arquivos {extensao} dentro do programa, "
                  "coloque {dll} na pasta lib do VB Recorder.").format(
                      extensao=os.path.splitext(path)[1], dll=dll), _("Aviso"))
            return
        ok, err = self.audio.play_file(path)
        if not ok:
            self._aviso(_("Erro BASS {erro} ao reproduzir:\n{arquivo}").format(erro=err, arquivo=path),
                        _("Erro"), wx.ICON_ERROR)
            return
        self._update_play_button()

    def on_delete_recording(self, event):
        files = self._existing_recording_files()
        if not files:
            self.last_recording = []
            self._update_recording_buttons()
            return
        lista = "\n".join(os.path.basename(p) for p in files)
        msg = ngettext("Excluir a gravação?\n\n{lista}",
                       "Excluir os {n} arquivos da última gravação?\n\n{lista}",
                       len(files)).format(n=len(files), lista=lista)
        if not confirmar_sim_nao(self._dialog_hwnd(), msg, _("Confirmar Exclusão")):
            return
        self.audio.stop_playback()
        falhas = []
        for path in files:
            try:
                os.remove(path)
            except OSError as e:
                falhas.append(f"{os.path.basename(path)}: {e}")
        self.last_recording = []
        self._update_recording_buttons()
        if falhas:
            self._aviso(_("Não foi possível excluir:\n") + "\n".join(falhas),
                        _("Erro"), wx.ICON_ERROR)

    def on_about(self, event):
        wx.MessageBox(_("VB Recorder v{versao}\nAutor: Vitor Bruski").format(versao=VERSION),
                      _("Sobre"), wx.OK)

    def _toggle_ui(self, enable):
        for ctrl in [self.cmb_format, self.cmb_bitrate, self.cmb_flac_level, self.cmb_rate,
                     self.cmb_channels, self.lst_fontes, self.btn_add, self.btn_edit, self.btn_rem,
                     self.chk_separate, self.txt_dir, self.btn_browse, self.txt_filename,
                     self.btn_settings]:
            ctrl.Enable(enable)

    def on_minimize(self, event):
        """Some para a bandeja só se a opção estiver ligada; senão, minimiza
        para a barra de tarefas como qualquer outra janela."""
        if self.IsIconized() and self.minimiza_para_bandeja():
            self.Hide()
        else:
            event.Skip()

    def on_close(self, event):
        if self.audio.is_recording and not confirmar_sim_nao(
                self._dialog_hwnd(),
                _("Há uma gravação em andamento.\n\nDeseja mesmo fechar o VB Recorder?"),
                _("Confirmar Saída")):
            if hasattr(event, 'Veto') and event.CanVeto():
                event.Veto()
            return
        self.play_timer.Stop()
        self.audio.stop_recording()
        self.save_settings()
        self.audio.cleanup()
        self.tray_icon.Destroy()
        self.Destroy()
