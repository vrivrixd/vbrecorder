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
import ctypes.wintypes
import os
import struct
import threading
import uuid as _uuidlib

HRESULT = ctypes.c_long
REFERENCE_TIME = ctypes.c_longlong

# O próprio gravador nunca deve aparecer como fonte de áudio. O filtro é pelo
# PID, e não pelo nome do executável, para valer qualquer que seja esse nome.
_OWN_PID = os.getpid()

_ole32    = ctypes.windll.ole32
_kernel32 = ctypes.windll.kernel32

_ole32.CoInitializeEx.argtypes  = [ctypes.c_void_p, ctypes.c_uint32]
_ole32.CoInitializeEx.restype   = HRESULT
_ole32.CoUninitialize.argtypes  = []
_ole32.CoUninitialize.restype   = None
_ole32.CoCreateInstance.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint32,
                                     ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
_ole32.CoCreateInstance.restype = HRESULT
_ole32.CoTaskMemFree.argtypes   = [ctypes.c_void_p]
_ole32.CoTaskMemFree.restype    = None

try:
    _activate_async = ctypes.windll.mmdevapi.ActivateAudioInterfaceAsync
    _activate_async.argtypes = [ctypes.c_wchar_p, ctypes.c_void_p, ctypes.c_void_p,
                                 ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
    _activate_async.restype = HRESULT
    PROCESS_LOOPBACK_AVAILABLE = True
except Exception:
    PROCESS_LOOPBACK_AVAILABLE = False


class GUID(ctypes.Structure):
    _fields_ = [('Data1', ctypes.c_ulong), ('Data2', ctypes.c_ushort),
                 ('Data3', ctypes.c_ushort), ('Data4', ctypes.c_ubyte * 8)]

    @classmethod
    def from_str(cls, s):
        u = _uuidlib.UUID(s)
        b = u.bytes_le
        return cls(struct.unpack_from('<I', b, 0)[0], struct.unpack_from('<H', b, 4)[0],
                   struct.unpack_from('<H', b, 6)[0], (ctypes.c_ubyte * 8)(*b[8:]))


_CLSID_MMDeviceEnumerator  = GUID.from_str('{BCDE0395-E52F-467C-8E3D-C4579291692E}')
_IID_IMMDeviceEnumerator   = GUID.from_str('{A95664D2-9614-4F35-A746-DE8DB63617E6}')
_IID_IAudioSessionManager2 = GUID.from_str('{77AA99A0-1BD6-484F-8BC7-2C654C9A9B6F}')
_IID_IAudioSessionControl2 = GUID.from_str('{BFB7FF88-7239-4FC9-8FA2-07C950BE9C6D}')
_IID_IAudioClient          = GUID.from_str('{1CB9AD4C-DBFA-4C32-B178-C2F568A703B2}')
_IID_IAudioCaptureClient   = GUID.from_str('{C8ADBD64-E71E-48A0-A4DE-185C395CD317}')


def _ptr_at(address, index):
    return ctypes.c_size_t.from_address(address + index * ctypes.sizeof(ctypes.c_size_t)).value


def _com(ptr, idx, restype, argtypes, args):
    addr = ptr.value if isinstance(ptr, ctypes.c_void_p) else ptr
    vtbl_addr = _ptr_at(addr, 0)
    fn_addr   = _ptr_at(vtbl_addr, idx)
    fn = ctypes.WINFUNCTYPE(restype, ctypes.c_void_p, *argtypes)(fn_addr)
    return fn(addr, *args)


def _release(ptr):
    if ptr:
        _com(ptr, 2, ctypes.c_ulong, [], [])


def _qi(ptr, iid, out):
    return _com(ptr, 0, HRESULT,
                [ctypes.POINTER(GUID), ctypes.POINTER(ctypes.c_void_p)],
                [ctypes.byref(iid), ctypes.byref(out)])


class WAVEFORMATEX(ctypes.Structure):
    _fields_ = [('wFormatTag', ctypes.c_uint16), ('nChannels', ctypes.c_uint16),
                 ('nSamplesPerSec', ctypes.c_uint32), ('nAvgBytesPerSec', ctypes.c_uint32),
                 ('nBlockAlign', ctypes.c_uint16), ('wBitsPerSample', ctypes.c_uint16),
                 ('cbSize', ctypes.c_uint16)]


def get_audio_sessions():
    result = []
    _ole32.CoInitializeEx(None, 0)
    try:
        enumerator = ctypes.c_void_p()
        hr = _ole32.CoCreateInstance(ctypes.byref(_CLSID_MMDeviceEnumerator), None, 0x17,
                                     ctypes.byref(_IID_IMMDeviceEnumerator),
                                     ctypes.byref(enumerator))
        if hr != 0 or not enumerator.value: return []
        try:
            device = ctypes.c_void_p()
            hr = _com(enumerator, 4, HRESULT, [ctypes.c_int, ctypes.c_int,
                      ctypes.POINTER(ctypes.c_void_p)], [0, 0, ctypes.byref(device)])
            if hr != 0 or not device.value: return []
            try:
                mgr2 = ctypes.c_void_p()
                iid = _IID_IAudioSessionManager2
                hr = _com(device, 3, HRESULT,
                          [ctypes.POINTER(GUID), ctypes.c_uint32, ctypes.c_void_p,
                           ctypes.POINTER(ctypes.c_void_p)],
                          [ctypes.byref(iid), 0x17, None, ctypes.byref(mgr2)])
                if hr != 0 or not mgr2.value: return []
                try:
                    sess_enum = ctypes.c_void_p()
                    hr = _com(mgr2, 5, HRESULT, [ctypes.POINTER(ctypes.c_void_p)],
                              [ctypes.byref(sess_enum)])
                    if hr != 0 or not sess_enum.value: return []
                    try:
                        count = ctypes.c_int(0)
                        _com(sess_enum, 3, HRESULT, [ctypes.POINTER(ctypes.c_int)],
                             [ctypes.byref(count)])
                        seen = set()
                        for i in range(count.value):
                            ctrl = ctypes.c_void_p()
                            hr = _com(sess_enum, 4, HRESULT,
                                      [ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)],
                                      [i, ctypes.byref(ctrl)])
                            if hr != 0 or not ctrl.value: continue
                            try:
                                ctrl2 = ctypes.c_void_p()
                                iid2 = _IID_IAudioSessionControl2
                                _qi(ctrl, iid2, ctrl2)
                                if not ctrl2.value: continue
                                try:
                                    pid = ctypes.c_uint32(0)
                                    hr2 = _com(ctrl2, 14, HRESULT,
                                               [ctypes.POINTER(ctypes.c_uint32)],
                                               [ctypes.byref(pid)])
                                    if (hr2 == 0 and pid.value and pid.value != _OWN_PID
                                            and pid.value not in seen):
                                        seen.add(pid.value)
                                        name = _process_name(pid.value)
                                        if name:
                                            result.append((pid.value, name))
                                finally:
                                    _release(ctrl2)
                            finally:
                                _release(ctrl)
                    finally:
                        _release(sess_enum)
                finally:
                    _release(mgr2)
            finally:
                _release(device)
        finally:
            _release(enumerator)
    except Exception:
        pass
    finally:
        _ole32.CoUninitialize()
    return result


def _process_name(pid):
    handle = _kernel32.OpenProcess(0x1000, False, pid)
    if not handle:
        return None
    try:
        buf  = ctypes.create_unicode_buffer(260)
        size = ctypes.c_uint32(260)
        if _kernel32.QueryFullProcessImageNameW(handle, 0, buf, ctypes.byref(size)):
            return os.path.basename(buf.value)
        return None
    finally:
        _kernel32.CloseHandle(handle)


def get_process_name(pid):
    # Também cobre o caso raro do Windows reciclar um PID salvo no .ini e ele
    # calhar de ser o nosso: a fonte é descartada ao carregar as configurações.
    if pid == _OWN_PID:
        return None
    return _process_name(pid)


class _CompletionHandler:
    def __init__(self):
        self._event       = threading.Event()
        self.audio_client = ctypes.c_void_p(0)
        self._act_hr      = ctypes.c_long(-1)

        QI = ctypes.WINFUNCTYPE(HRESULT, ctypes.c_size_t, ctypes.c_size_t, ctypes.c_size_t)
        RF = ctypes.WINFUNCTYPE(ctypes.c_ulong, ctypes.c_size_t)
        AC = ctypes.WINFUNCTYPE(HRESULT, ctypes.c_size_t, ctypes.c_size_t)

        self._cb_qi  = QI(self._qi)
        self._cb_ar  = RF(self._ar)
        self._cb_rel = RF(self._rel)
        self._cb_ac  = AC(self._ac)

        self._vtbl = (ctypes.c_size_t * 4)(
            self._fn_ptr(self._cb_qi),
            self._fn_ptr(self._cb_ar),
            self._fn_ptr(self._cb_rel),
            self._fn_ptr(self._cb_ac),
        )

        self._obj = (ctypes.c_size_t * 1)(ctypes.addressof(self._vtbl))

    @staticmethod
    def _fn_ptr(cb):
        return ctypes.cast(cb, ctypes.c_void_p).value or 0

    def ptr(self):
        return ctypes.addressof(self._obj)

    def wait(self, timeout=5.0):
        return self._event.wait(timeout)

    def _qi(self, this, riid, ppv):
        if ppv:
            ctypes.c_size_t.from_address(ppv).value = this
        return 0

    def _ar(self, this):
        return 1

    def _rel(self, this):
        return 1

    def _ac(self, this, op):
        try:
            act_hr  = ctypes.c_long(-1)
            act_unk = ctypes.c_void_p(0)
            _com(op, 3, HRESULT,
                 [ctypes.POINTER(ctypes.c_long), ctypes.POINTER(ctypes.c_void_p)],
                 [ctypes.byref(act_hr), ctypes.byref(act_unk)])
            self._act_hr = act_hr
            if act_hr.value == 0 and act_unk.value:
                iid = _IID_IAudioClient
                _qi(act_unk, iid, self.audio_client)
                _release(act_unk)
        except Exception:
            pass
        finally:
            self._event.set()
        return 0


class _LoopbackParams(ctypes.Structure):
    _fields_ = [('TargetProcessId', ctypes.c_uint32), ('ProcessLoopbackMode', ctypes.c_uint32)]


class _ActivationParams(ctypes.Structure):
    _fields_ = [('ActivationType', ctypes.c_uint32), ('Params', _LoopbackParams)]


class _BlobData(ctypes.Structure):
    _fields_ = [('cbSize', ctypes.c_uint32), ('pBlobData', ctypes.c_void_p)]


class _PropVariant(ctypes.Structure):
    _fields_ = [('vt', ctypes.c_uint16), ('r1', ctypes.c_uint16),
                 ('r2', ctypes.c_uint16), ('r3', ctypes.c_uint16), ('blob', _BlobData)]


_VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK = "VAD\\Process_Loopback"
_AUDCLNT_SHAREMODE_SHARED              = 0
_AUDCLNT_STREAMFLAGS_LOOPBACK          = 0x00020000


class ProcessLoopbackCapture:
    def __init__(self, pid, callback):
        self.pid         = pid
        self._callback   = callback
        self.sample_rate = 0
        self.channels    = 0
        self._ready      = threading.Event()
        self._stop       = threading.Event()
        self._thread     = threading.Thread(target=self._run, daemon=True)

    def start(self):
        if not PROCESS_LOOPBACK_AVAILABLE:
            return False
        self._thread.start()
        return True

    def set_callback(self, cb):
        self._callback = cb

    def wait_ready(self, timeout=3.0):
        return self._ready.wait(timeout)

    def stop(self):
        self._stop.set()
        self._thread.join(timeout=3.0)

    def _run(self):
        _ole32.CoInitializeEx(None, 2)
        try:
            self._capture()
        except Exception:
            pass
        finally:
            self._ready.set()
            _ole32.CoUninitialize()

    def _capture(self):
        act_params = _ActivationParams()
        act_params.ActivationType             = 1
        act_params.Params.TargetProcessId     = self.pid
        act_params.Params.ProcessLoopbackMode = 0

        prop = _PropVariant()
        prop.vt             = 0x41
        prop.blob.cbSize    = ctypes.sizeof(act_params)
        prop.blob.pBlobData = ctypes.addressof(act_params)

        handler  = _CompletionHandler()
        async_op = ctypes.c_void_p(0)
        iid      = _IID_IAudioClient

        hr = _activate_async(
            _VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
            ctypes.byref(iid),
            ctypes.byref(prop),
            handler.ptr(),
            ctypes.byref(async_op)
        )
        if hr != 0:
            return

        if not handler.wait(5.0):
            return

        audio_client = handler.audio_client
        if not audio_client.value:
            if async_op.value:
                _release(async_op.value)
            return

        try:
            if async_op.value:
                _release(async_op.value)

            fmt_ptr      = ctypes.c_void_p(0)
            free_fmt_ptr = False
            hr = _com(audio_client, 8, HRESULT, [ctypes.POINTER(ctypes.c_void_p)],
                      [ctypes.byref(fmt_ptr)])

            if hr == 0 and fmt_ptr.value:
                fmt = ctypes.cast(fmt_ptr, ctypes.POINTER(WAVEFORMATEX)).contents
                self.sample_rate = fmt.nSamplesPerSec
                self.channels    = fmt.nChannels
                block_align      = fmt.nBlockAlign
                free_fmt_ptr     = True
            else:
                self.sample_rate = 48000
                self.channels    = 2
                _fallback        = WAVEFORMATEX()
                _fallback.wFormatTag      = 3
                _fallback.nChannels       = self.channels
                _fallback.nSamplesPerSec  = self.sample_rate
                _fallback.wBitsPerSample  = 32
                _fallback.nBlockAlign     = self.channels * 4
                _fallback.nAvgBytesPerSec = self.sample_rate * self.channels * 4
                _fallback.cbSize          = 0
                block_align   = _fallback.nBlockAlign
                fmt_ptr.value = ctypes.addressof(_fallback)

            REFTIMES_100NS = REFERENCE_TIME(10_000_000 // 10)
            hr = _com(audio_client, 3, HRESULT,
                      [ctypes.c_int, ctypes.c_uint32, REFERENCE_TIME, REFERENCE_TIME,
                       ctypes.c_void_p, ctypes.c_void_p],
                      [_AUDCLNT_SHAREMODE_SHARED, _AUDCLNT_STREAMFLAGS_LOOPBACK,
                       REFTIMES_100NS, 0, fmt_ptr.value, None])
            if free_fmt_ptr:
                _ole32.CoTaskMemFree(fmt_ptr)
            if hr != 0:
                return

            cap_client = ctypes.c_void_p(0)
            iid_cap    = _IID_IAudioCaptureClient
            hr = _com(audio_client, 14, HRESULT,
                      [ctypes.POINTER(GUID), ctypes.POINTER(ctypes.c_void_p)],
                      [ctypes.byref(iid_cap), ctypes.byref(cap_client)])
            if hr != 0 or not cap_client.value:
                return

            try:
                hr = _com(audio_client, 10, HRESULT, [], [])
                if hr != 0:
                    return

                self._ready.set()

                while not self._stop.is_set():
                    pkt = ctypes.c_uint32(0)
                    _com(cap_client, 5, HRESULT, [ctypes.POINTER(ctypes.c_uint32)],
                         [ctypes.byref(pkt)])
                    if pkt.value == 0:
                        self._stop.wait(0.01)
                        continue

                    data_ptr   = ctypes.c_void_p(0)
                    num_frames = ctypes.c_uint32(0)
                    flags      = ctypes.c_uint32(0)
                    dev_pos    = ctypes.c_uint64(0)
                    qpc_pos    = ctypes.c_int64(0)

                    hr = _com(cap_client, 3, HRESULT,
                              [ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_uint32),
                               ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint64),
                               ctypes.POINTER(ctypes.c_int64)],
                              [ctypes.byref(data_ptr), ctypes.byref(num_frames),
                               ctypes.byref(flags), ctypes.byref(dev_pos),
                               ctypes.byref(qpc_pos)])

                    if hr == 0 and data_ptr.value and num_frames.value:
                        byte_count = num_frames.value * block_align
                        self._callback(data_ptr.value, byte_count,
                                       self.channels, self.sample_rate)

                    _com(cap_client, 4, HRESULT, [ctypes.c_uint32], [num_frames.value])

                _com(audio_client, 11, HRESULT, [], [])
            finally:
                _release(cap_client)
        finally:
            _release(audio_client)
