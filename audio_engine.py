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

import ctypes
import os
import re
import sys
import threading

from config import BUFFER_MAXIMO, BUFFER_MINIMO, BUFFER_PADRAO
from i18n import _

from process_audio import ProcessLoopbackCapture

BASS_DEVICE_ENABLED    = 1
BASS_DEVICE_LOOPBACK   = 8
BASS_ENCODE_PCM        = 64
BASS_ENCODE_AIFF       = 16384
BASS_ENCODE_FP_16BIT   = 4
BASS_ENCODE_AUTOFREE   = 0x40000
BASS_ERROR_ALREADY     = 14
BASS_STREAM_DECODE     = 0x200000
BASS_SAMPLE_FLOAT      = 256
BASS_RECORD_PAUSE      = 0x8000
BASS_CONFIG_BUFFER     = 0
BASS_CONFIG_REC_BUFFER = 19
BASS_ATTRIB_VOL        = 2
BASS_UNICODE           = 0x80000000
BASS_ACTIVE_STOPPED    = 0

# Subpasta onde ficam as DLLs do BASS.
LIB_DIR = 'lib'

# Formatos que o BASS só reproduz com um plugin extra na pasta do programa.
PLAYBACK_PLUGINS = {'flac': 'bassflac.dll', 'opus': 'bassopus.dll'}


class BASS_DEVICEINFO(ctypes.Structure):
    _fields_ = [("name", ctypes.c_char_p), ("driver", ctypes.c_char_p),
                 ("flags", ctypes.c_uint32)]


RECORDPROC = ctypes.WINFUNCTYPE(ctypes.c_int, ctypes.c_uint32, ctypes.c_void_p,
                                 ctypes.c_uint32, ctypes.c_void_p)


def _source_safe_name(name):
    name = re.sub(r'\s*\(PID \d+\)', '', name).strip()
    name = re.sub(r'[\\/:*?"<>|]', '', name)
    return name.strip() or _('fonte')


class AudioEngine:
    def __init__(self):
        self.is_recording = False
        self.is_paused    = False

        self.mixer_handle   = None
        self.record_handles = []
        self.push_streams   = {}
        self._sep_pushes    = []
        self._proc_captures = []
        self._rec_volumes   = {}
        self._drain_stop    = threading.Event()
        self._drain_thread  = None
        self._mix_lock      = threading.Lock()
        self._pending_devs  = 0
        self._total_devs    = 0

        self.monitor_rec    = None
        self.monitor_stream = None
        self.play_stream    = None
        self.output_files   = []
        self._plugins       = {}

        self._record_cb  = RECORDPROC(self._record_callback)
        self._monitor_cb = RECORDPROC(self._monitor_callback)
        base_path = getattr(sys, '_MEIPASS', os.path.dirname(os.path.abspath(__file__)))
        lib_path  = os.path.join(base_path, LIB_DIR)
        # As DLLs ficam em lib/; se não estiverem lá, procura ao lado do programa.
        if not os.path.isfile(os.path.join(lib_path, 'bass.dll')):
            lib_path = base_path
        self._lib_path = lib_path

        try:
            self.bass         = ctypes.WinDLL(os.path.join(lib_path, 'bass.dll'))
            self.bassenc      = ctypes.WinDLL(os.path.join(lib_path, 'bassenc.dll'))
            self.bassenc_mp3  = ctypes.WinDLL(os.path.join(lib_path, 'bassenc_mp3.dll'))
            self.bassenc_ogg  = ctypes.WinDLL(os.path.join(lib_path, 'bassenc_ogg.dll'))
            self.bassenc_flac = ctypes.WinDLL(os.path.join(lib_path, 'bassenc_flac.dll'))
            self.bassmix      = ctypes.WinDLL(os.path.join(lib_path, 'bassmix.dll'))
            self.bassenc_opus = ctypes.WinDLL(os.path.join(lib_path, 'bassenc_opus.dll'))
        except Exception:
            sys.exit(1)

        self._setup_ctypes()
        # Guarda o buffer de reprodução de fábrica do BASS antes de mexer nele:
        # ele é o piso usado em set_buffer.
        self._buffer_reproducao_padrao = self.bass.BASS_GetConfig(BASS_CONFIG_BUFFER)
        self.set_buffer(BUFFER_PADRAO)
        self.bass.BASS_Init(-1, 44100, 0, 0, None)
        self._load_playback_plugins()

    def set_buffer(self, milissegundos):
        """Tamanho do buffer, em milissegundos.

        Vale para a captura e para a reprodução ao vivo do botão Testar. O BASS
        lê esses valores ao criar cada canal, então a mudança pega na próxima
        gravação ou no próximo teste, não no que já está em andamento.

        Não é um atraso: o BASS mantém esses buffers o mais vazios possível e
        toca o áudio assim que ele chega. O tamanho é a folga que existe para
        absorver um engasgo do computador, e não uma fila a ser preenchida.
        """
        ms = max(BUFFER_MINIMO, min(BUFFER_MAXIMO, int(milissegundos)))
        self.buffer_ms = ms
        self.bass.BASS_SetConfig(BASS_CONFIG_REC_BUFFER, ms)
        # O buffer de reprodução nunca desce abaixo do padrão do BASS. Ele tem
        # de ser maior que o período de atualização (100 ms), senão o som do
        # teste falha, que é justamente o contrário do que se quer aqui.
        self.bass.BASS_SetConfig(BASS_CONFIG_BUFFER,
                                 max(ms, self._buffer_reproducao_padrao))
        return ms

    def _load_playback_plugins(self):
        """Carrega os plugins opcionais de reprodução (flac/opus), se estiverem na pasta."""
        for fmt, dll in PLAYBACK_PLUGINS.items():
            path = os.path.join(self._lib_path, dll)
            if os.path.exists(path):
                handle = self.bass.BASS_PluginLoad(path, BASS_UNICODE)
                if handle:
                    self._plugins[fmt] = handle

    def _setup_ctypes(self):
        self.bass.BASS_SetConfig.argtypes = [ctypes.c_uint32, ctypes.c_uint32]
        self.bass.BASS_SetConfig.restype  = ctypes.c_int
        self.bass.BASS_GetConfig.argtypes = [ctypes.c_uint32]
        self.bass.BASS_GetConfig.restype  = ctypes.c_uint32
        self.bass.BASS_ErrorGetCode.restype = ctypes.c_int
        self.bass.BASS_RecordInit.argtypes  = [ctypes.c_int]
        self.bass.BASS_RecordInit.restype   = ctypes.c_int
        self.bass.BASS_RecordStart.argtypes = [ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32,
                                               RECORDPROC, ctypes.c_void_p]
        self.bass.BASS_RecordStart.restype  = ctypes.c_uint32
        self.bassenc.BASS_Encode_Start.argtypes = [ctypes.c_uint32, ctypes.c_char_p,
                                                    ctypes.c_uint32, ctypes.c_void_p, ctypes.c_void_p]
        self.bassenc.BASS_Encode_StartPCMFile.argtypes = [ctypes.c_uint32, ctypes.c_uint32,
                                                          ctypes.c_char_p]
        self.bassenc.BASS_Encode_StartPCMFile.restype  = ctypes.c_uint32
        self.bassenc_mp3.BASS_Encode_MP3_StartFile.argtypes = [ctypes.c_uint32, ctypes.c_char_p,
                                                                ctypes.c_uint32, ctypes.c_char_p]
        self.bassenc_ogg.BASS_Encode_OGG_StartFile.argtypes = [ctypes.c_uint32, ctypes.c_char_p,
                                                                ctypes.c_uint32, ctypes.c_char_p]
        self.bassenc_flac.BASS_Encode_FLAC_StartFile.argtypes = [ctypes.c_uint32, ctypes.c_char_p,
                                                                  ctypes.c_uint32, ctypes.c_char_p]
        self.bassenc_opus.BASS_Encode_OPUS_StartFile.argtypes = [ctypes.c_uint32, ctypes.c_char_p,
                                                                  ctypes.c_uint32, ctypes.c_char_p]
        self.bassenc.BASS_Encode_Start.restype           = ctypes.c_uint32
        self.bassenc_mp3.BASS_Encode_MP3_StartFile.restype  = ctypes.c_uint32
        self.bassenc_ogg.BASS_Encode_OGG_StartFile.restype  = ctypes.c_uint32
        self.bassenc_flac.BASS_Encode_FLAC_StartFile.restype = ctypes.c_uint32
        self.bassenc_opus.BASS_Encode_OPUS_StartFile.restype = ctypes.c_uint32
        self.bassenc.BASS_Encode_SetPaused.argtypes = [ctypes.c_uint32, ctypes.c_int]
        self.bassenc.BASS_Encode_SetPaused.restype  = ctypes.c_int
        self.bassmix.BASS_Mixer_StreamCreate.argtypes = [ctypes.c_uint32, ctypes.c_uint32,
                                                          ctypes.c_uint32]
        self.bassmix.BASS_Mixer_StreamCreate.restype  = ctypes.c_uint32
        self.bassmix.BASS_Mixer_StreamAddChannel.argtypes = [ctypes.c_uint32, ctypes.c_uint32,
                                                              ctypes.c_uint32]
        self.bassmix.BASS_Mixer_StreamAddChannel.restype  = ctypes.c_int
        self.bass.BASS_StreamCreate.argtypes = [ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32,
                                                ctypes.c_void_p, ctypes.c_void_p]
        self.bass.BASS_StreamCreate.restype  = ctypes.c_uint32
        self.bass.BASS_StreamPutData.argtypes = [ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32]
        self.bass.BASS_ChannelPlay.argtypes  = [ctypes.c_uint32, ctypes.c_int]
        self.bass.BASS_ChannelPlay.restype   = ctypes.c_int
        self.bass.BASS_ChannelStop.argtypes  = [ctypes.c_uint32]
        self.bass.BASS_ChannelStop.restype   = ctypes.c_int
        self.bass.BASS_ChannelGetData.argtypes = [ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32]
        self.bass.BASS_ChannelGetData.restype  = ctypes.c_int
        self.bass.BASS_ChannelSetAttribute.argtypes = [ctypes.c_uint32, ctypes.c_uint32, ctypes.c_float]
        self.bass.BASS_ChannelSetAttribute.restype  = ctypes.c_int
        self.bass.BASS_ChannelIsActive.argtypes = [ctypes.c_uint32]
        self.bass.BASS_ChannelIsActive.restype  = ctypes.c_uint32
        self.bass.BASS_StreamCreateFile.argtypes = [ctypes.c_int, ctypes.c_wchar_p, ctypes.c_uint64,
                                                    ctypes.c_uint64, ctypes.c_uint32]
        self.bass.BASS_StreamCreateFile.restype  = ctypes.c_uint32
        self.bass.BASS_PluginLoad.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32]
        self.bass.BASS_PluginLoad.restype  = ctypes.c_uint32

    def _record_callback(self, handle, buffer, length, user):
        if self.is_paused:
            return 1
        push = self.push_streams.get(handle)
        if push:
            volume = self._rec_volumes.get(handle, 1.0)
            if volume != 1.0 and buffer and length > 0:
                n = length // 4
                arr = (ctypes.c_float * n).from_address(buffer)
                for i in range(n):
                    arr[i] *= volume
            self.bass.BASS_StreamPutData(push, buffer, length)
            if self.mixer_handle and self._total_devs > 0:
                do_pull = False
                with self._mix_lock:
                    self._pending_devs += 1
                    if self._pending_devs >= self._total_devs:
                        self._pending_devs = 0
                        do_pull = True
                if do_pull:
                    buf = (ctypes.c_byte * length)()
                    self.bass.BASS_ChannelGetData(self.mixer_handle, buf, length)
        return 1

    def _monitor_callback(self, handle, buffer, length, user):
        stream = self.monitor_stream
        if stream and buffer and length > 0:
            self.bass.BASS_StreamPutData(stream, buffer, length)
        return 1

    def _drain_loop(self, streams):
        buf_size = 8192
        buf = (ctypes.c_byte * buf_size)()
        while not self._drain_stop.is_set():
            for s in streams:
                self.bass.BASS_ChannelGetData(s, buf, buf_size)
            self._drain_stop.wait(0.02)

    def _start_drain(self, streams):
        self._drain_stop.clear()
        self._drain_thread = threading.Thread(target=self._drain_loop,
                                               args=(streams,), daemon=True)
        self._drain_thread.start()

    def _make_push(self, sample_rate, channels):
        return self.bass.BASS_StreamCreate(
            sample_rate, channels,
            BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT,
            ctypes.c_void_p(-1), None
        )

    def init_device(self, dev_idx):
        if dev_idx < 0: return True
        res = self.bass.BASS_RecordInit(dev_idx)
        if res == 0:
            code = self.bass.BASS_ErrorGetCode()
            if code != BASS_ERROR_ALREADY: return False
        return True

    def get_input_devices(self):
        devices = []
        info = BASS_DEVICEINFO()
        i = 0
        while self.bass.BASS_RecordGetDeviceInfo(i, ctypes.byref(info)):
            if info.flags & BASS_DEVICE_ENABLED:
                name = info.name.decode('mbcs', errors='ignore')
                if name == 'Default': name = _('Dispositivo de Entrada Padrão')
                if info.flags & BASS_DEVICE_LOOPBACK: name += ' (Loopback)'
                devices.append((i, name))
            i += 1
        return devices

    def play_tone(self, frequency=1000, duration=150):
        import winsound
        winsound.Beep(frequency, duration)

    @property
    def is_monitoring(self):
        return self.monitor_rec is not None

    def start_monitor(self, dev_idx, volume=1.0, sample_rate=44100, channels=2):
        """Captura o dispositivo e reproduz na saída padrão, para teste ao vivo."""
        self.stop_monitor()
        if not self.init_device(dev_idx):
            return False, self.bass.BASS_ErrorGetCode()
        self.bass.BASS_RecordSetDevice(dev_idx)

        code = 0
        for rate, ch in [(sample_rate, channels), (44100, 2)]:
            stream = self.bass.BASS_StreamCreate(rate, ch, BASS_SAMPLE_FLOAT,
                                                 ctypes.c_void_p(-1), None)
            if not stream:
                code = self.bass.BASS_ErrorGetCode()
                continue
            self.monitor_stream = stream
            self.set_monitor_volume(volume)
            rec = self.bass.BASS_RecordStart(rate, ch, BASS_SAMPLE_FLOAT,
                                             self._monitor_cb, None)
            if rec:
                self.monitor_rec = rec
                self.bass.BASS_ChannelPlay(stream, 0)
                return True, 0
            code = self.bass.BASS_ErrorGetCode()
            self.monitor_stream = None
            self.bass.BASS_StreamFree(stream)
        return False, code

    def set_monitor_volume(self, volume):
        if self.monitor_stream:
            self.bass.BASS_ChannelSetAttribute(self.monitor_stream, BASS_ATTRIB_VOL, volume)

    def stop_monitor(self):
        if self.monitor_rec:
            self.bass.BASS_ChannelStop(self.monitor_rec)
            self.monitor_rec = None
        if self.monitor_stream:
            stream = self.monitor_stream
            self.monitor_stream = None
            self.bass.BASS_ChannelStop(stream)
            self.bass.BASS_StreamFree(stream)

    @property
    def is_playing(self):
        return bool(self.play_stream) and \
            self.bass.BASS_ChannelIsActive(self.play_stream) != BASS_ACTIVE_STOPPED

    def missing_playback_plugin(self, file_path):
        """Nome da DLL necessária para reproduzir o arquivo, ou None se já dá para tocar."""
        fmt = os.path.splitext(file_path)[1].lstrip('.').lower()
        if fmt in PLAYBACK_PLUGINS and fmt not in self._plugins:
            return PLAYBACK_PLUGINS[fmt]
        return None

    def play_file(self, file_path):
        self.stop_playback()
        stream = self.bass.BASS_StreamCreateFile(0, file_path, 0, 0, BASS_UNICODE)
        if not stream:
            return False, self.bass.BASS_ErrorGetCode()
        self.play_stream = stream
        if not self.bass.BASS_ChannelPlay(stream, 0):
            code = self.bass.BASS_ErrorGetCode()
            self.stop_playback()
            return False, code
        return True, 0

    def stop_playback(self):
        if self.play_stream:
            stream = self.play_stream
            self.play_stream = None
            self.bass.BASS_ChannelStop(stream)
            self.bass.BASS_StreamFree(stream)

    def start_recording(self, file_path, format_type, bitrate_val, sample_rate,
                        channels, sources, separate_files=False, flac_nivel=8):
        if self.is_recording: return False, 0
        if not sources: return False, -1

        self.record_handles = []
        self.push_streams   = {}
        self._sep_pushes    = []
        self._proc_captures = []
        self._rec_volumes   = {}
        self.mixer_handle   = None
        self._total_devs    = 0
        self._pending_devs  = 0
        self.output_files   = []
        ch    = 1 if channels == "mono" else 2
        flags = BASS_ENCODE_AUTOFREE

        options_bytes = b""
        if format_type == 'mp3':   options_bytes = f"-b {bitrate_val}".encode('mbcs')
        elif format_type == 'ogg':  options_bytes = f"-b {bitrate_val}".encode('mbcs')
        elif format_type == 'opus': options_bytes = f"--bitrate {bitrate_val}".encode('mbcs')
        elif format_type == 'flac': options_bytes = f"-{flac_nivel}".encode('mbcs')

        def apply_encoder(handle, path):
            pb = path.encode('mbcs')
            # A captura é em float; FP_16BIT grava wav e aiff em PCM de 16 bits.
            if format_type == 'wav':   return self.bassenc.BASS_Encode_Start(handle, pb, flags | BASS_ENCODE_PCM | BASS_ENCODE_FP_16BIT, None, None)
            elif format_type == 'aiff': return self.bassenc.BASS_Encode_StartPCMFile(handle, flags | BASS_ENCODE_AIFF | BASS_ENCODE_FP_16BIT, pb)
            elif format_type == 'mp3':  return self.bassenc_mp3.BASS_Encode_MP3_StartFile(handle, options_bytes, flags, pb)
            elif format_type == 'ogg':  return self.bassenc_ogg.BASS_Encode_OGG_StartFile(handle, options_bytes, flags, pb)
            elif format_type == 'opus': return self.bassenc_opus.BASS_Encode_OPUS_StartFile(handle, options_bytes, flags, pb)
            elif format_type == 'flac': return self.bassenc_flac.BASS_Encode_FLAC_StartFile(handle, options_bytes, flags, pb)

        proc_caps = {}
        for src in sources:
            if src['type'] == 'process':
                cap = ProcessLoopbackCapture(src['id'], lambda *_: None)
                cap.start()
                proc_caps[src['id']] = cap
                self._proc_captures.append(cap)

        for cap in proc_caps.values():
            cap.wait_ready(timeout=3.0)

        def make_proc_cb(push_handle, volume=1.0):
            def cb(data_ptr, byte_count, _ch, _sr):
                if not self.is_paused:
                    if volume != 1.0 and data_ptr and byte_count > 0:
                        n = byte_count // 4
                        buf = (ctypes.c_float * n)()
                        ctypes.memmove(buf, data_ptr, byte_count)
                        for i in range(n):
                            buf[i] *= volume
                        self.bass.BASS_StreamPutData(push_handle, buf, byte_count)
                    else:
                        self.bass.BASS_StreamPutData(push_handle, ctypes.c_void_p(data_ptr), byte_count)
            return cb

        if separate_files and len(sources) > 1:
            base, ext = os.path.splitext(file_path)
            drain_targets = []
            for src in sources:
                safe   = _source_safe_name(src['name'])
                path_i = f"{base} ({safe}){ext}"
                volume = src.get('volume', 1.0)
                self.output_files.append(path_i)
                if src['type'] == 'device':
                    dev_idx = src['id']
                    self.init_device(dev_idx)
                    self.bass.BASS_RecordSetDevice(dev_idx)
                    push = self._make_push(sample_rate, ch)
                    drain_targets.append(push)
                    apply_encoder(push, path_i)
                    rec = self.bass.BASS_RecordStart(
                        sample_rate, ch, BASS_RECORD_PAUSE | BASS_SAMPLE_FLOAT,
                        self._record_cb, None)
                    if not rec: return False, self.bass.BASS_ErrorGetCode()
                    self.record_handles.append(rec)
                    self.push_streams[rec] = push
                    self._rec_volumes[rec]  = volume
                    self.bass.BASS_ChannelPlay(rec, 0)
                else:
                    cap = proc_caps[src['id']]
                    push = self._make_push(cap.sample_rate or sample_rate, cap.channels or ch)
                    self._sep_pushes.append(push)
                    drain_targets.append(push)
                    apply_encoder(push, path_i)
                    cap.set_callback(make_proc_cb(push, volume))
            if drain_targets:
                self._start_drain(drain_targets)
        else:
            dev_count = sum(1 for s in sources if s['type'] == 'device')
            self._total_devs   = dev_count
            self._pending_devs = 0

            self.mixer_handle = self.bassmix.BASS_Mixer_StreamCreate(
                sample_rate, ch, BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE)
            for src in sources:
                if src['type'] == 'device':
                    push = self._make_push(sample_rate, ch)
                    self.bassmix.BASS_Mixer_StreamAddChannel(self.mixer_handle, push, 0)
                    dev_idx = src['id']
                    self.init_device(dev_idx)
                    self.bass.BASS_RecordSetDevice(dev_idx)
                    rec = self.bass.BASS_RecordStart(
                        sample_rate, ch, BASS_RECORD_PAUSE | BASS_SAMPLE_FLOAT,
                        self._record_cb, None)
                    if rec:
                        self.record_handles.append(rec)
                        self.push_streams[rec] = push
                        self._rec_volumes[rec]  = src.get('volume', 1.0)
                else:
                    cap = proc_caps[src['id']]
                    push = self._make_push(cap.sample_rate or sample_rate, cap.channels or ch)
                    self.bassmix.BASS_Mixer_StreamAddChannel(self.mixer_handle, push, 0)
                    cap.set_callback(make_proc_cb(push, src.get('volume', 1.0)))
            apply_encoder(self.mixer_handle, file_path)
            self.output_files.append(file_path)
            for rec in self.record_handles:
                self.bass.BASS_ChannelPlay(rec, 0)
            if dev_count == 0:
                self._start_drain([self.mixer_handle])

        self.is_recording = True
        self.is_paused    = False
        return True, 0

    def pause_recording(self):
        if self.is_recording:
            self.is_paused = True

    def resume_recording(self):
        if self.is_recording:
            self.is_paused = False

    def stop_recording(self):
        self._drain_stop.set()
        if self._drain_thread:
            self._drain_thread.join(timeout=2.0)
            self._drain_thread = None

        for cap in self._proc_captures:
            cap.stop()
        self._proc_captures = []

        if self.mixer_handle:
            self.bass.BASS_ChannelStop(self.mixer_handle)
            self.bass.BASS_StreamFree(self.mixer_handle)
            self.mixer_handle = None

        for rec in self.record_handles:
            self.bass.BASS_ChannelStop(rec)
        self.record_handles = []

        for push in self.push_streams.values():
            self.bass.BASS_StreamFree(push)
        self.push_streams  = {}
        self._rec_volumes  = {}

        for push in self._sep_pushes:
            self.bass.BASS_StreamFree(push)
        self._sep_pushes = []

        self.is_recording = False
        self.is_paused    = False

    def cleanup(self):
        self.stop_monitor()
        self.stop_playback()
        self.stop_recording()
        self.bass.BASS_Free()
