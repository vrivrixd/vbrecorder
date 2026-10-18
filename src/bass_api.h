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
 * The BASS functions used by the program. The DLLs are loaded at startup from
 * the lib folder next to the executable (or from beside it) instead of being
 * linked to it, so the program decides where to look for them.
 */
#pragma once

#include <windows.h>

typedef unsigned long long QWORD;

const DWORD BASS_DEVICE_ENABLED = 1;
const DWORD BASS_DEVICE_LOOPBACK = 8;
const DWORD BASS_ENCODE_FP_8BIT = 2;
const DWORD BASS_ENCODE_FP_16BIT = 4;
const DWORD BASS_ENCODE_FP_24BIT = 6;
const DWORD BASS_ENCODE_FP_32BIT = 8;
const DWORD BASS_ENCODE_PCM = 64;
const DWORD BASS_ENCODE_QUEUE = 0x200;
const DWORD BASS_ENCODE_AIFF = 0x4000;
const DWORD BASS_ENCODE_AUTOFREE = 0x40000;
const DWORD BASS_ENCODE_STOP_WAIT = 2;
const int BASS_ERROR_ALREADY = 14;
const DWORD BASS_STREAM_DECODE = 0x200000;
const DWORD BASS_SAMPLE_FLOAT = 0x100;
const DWORD BASS_RECORD_PAUSE = 0x8000;
const DWORD BASS_CONFIG_BUFFER = 0;
const DWORD BASS_CONFIG_REC_BUFFER = 19;
const DWORD BASS_CONFIG_UNICODE = 42;
const DWORD BASS_ATTRIB_FREQ = 1;
const DWORD BASS_ATTRIB_VOL = 2;
const DWORD BASS_UNICODE = 0x80000000;
const DWORD BASS_ACTIVE_STOPPED = 0;
const DWORD BASS_ACTIVE_PLAYING = 1;
const DWORD BASS_STREAMPROC_END = 0x80000000;
const DWORD BASS_ENCODE_COUNT_QUEUE = 3;
const DWORD BASS_MIXER_NONSTOP = 0x20000;
const DWORD BASS_MIXER_CHAN_DOWNMIX = 0x400000;
const DWORD BASS_MIXER_CHAN_NORAMPIN = 0x800000;

struct BASS_DEVICEINFO {
    const char* name;
    const char* driver;
    DWORD flags;
};

/* The BASS_FX equalizer and its parameters, from bass_fx.h. */
const DWORD BASS_FX_BFX_PEAKEQ = 0x10004;
const int BASS_BFX_CHANALL = -1;

struct BASS_BFX_PEAKEQ {
    int lBand;
    float fBandwidth, fQ, fCenter, fGain;
    int lChannel;
};

typedef BOOL(CALLBACK* RECORDPROC)(DWORD handle, const void* buffer, DWORD length, void* user);
/* STREAMPROC_PUSH from bass.h: the value -1 in place of the function. */
#define STREAMPROC_PUSH ((void*)(INT_PTR)-1)

struct BassApi {
    BOOL(WINAPI* SetConfig)(DWORD, DWORD);
    DWORD(WINAPI* GetConfig)(DWORD);
    int(WINAPI* ErrorGetCode)();
    BOOL(WINAPI* Init)(int, DWORD, DWORD, HWND, const void*);
    BOOL(WINAPI* Free)();
    BOOL(WINAPI* RecordInit)(int);
    BOOL(WINAPI* RecordFree)();
    BOOL(WINAPI* RecordSetDevice)(DWORD);
    BOOL(WINAPI* RecordGetDeviceInfo)(DWORD, BASS_DEVICEINFO*);
    DWORD(WINAPI* RecordStart)(DWORD, DWORD, DWORD, RECORDPROC, void*);
    DWORD(WINAPI* StreamCreate)(DWORD, DWORD, DWORD, void*, void*);
    DWORD(WINAPI* StreamCreateFile)(BOOL, const void*, QWORD, QWORD, DWORD);
    BOOL(WINAPI* StreamFree)(DWORD);
    DWORD(WINAPI* StreamPutData)(DWORD, const void*, DWORD);
    BOOL(WINAPI* ChannelPlay)(DWORD, BOOL);
    BOOL(WINAPI* ChannelStop)(DWORD);
    DWORD(WINAPI* ChannelGetData)(DWORD, void*, DWORD);
    BOOL(WINAPI* ChannelSetAttribute)(DWORD, DWORD, float);
    DWORD(WINAPI* ChannelIsActive)(DWORD);
    DWORD(WINAPI* PluginLoad)(const void*, DWORD);
    DWORD(WINAPI* ChannelSetFX)(DWORD, DWORD, int);
    BOOL(WINAPI* ChannelRemoveFX)(DWORD, DWORD);
    BOOL(WINAPI* FXSetParameters)(DWORD, const void*);

    DWORD(WINAPI* EncodeStart)(DWORD, const void*, DWORD, void*, void*);
    DWORD(WINAPI* EncodeStartPCMFile)(DWORD, DWORD, const void*);
    BOOL(WINAPI* EncodeStop)(DWORD);
    BOOL(WINAPI* EncodeStopEx)(DWORD, DWORD);  // may be missing in old versions
    QWORD(WINAPI* EncodeGetCount)(DWORD, DWORD);  // may be missing in old versions
    DWORD(WINAPI* EncodeMP3StartFile)(DWORD, const void*, DWORD, const void*);
    DWORD(WINAPI* EncodeOGGStartFile)(DWORD, const void*, DWORD, const void*);
    DWORD(WINAPI* EncodeFLACStartFile)(DWORD, const void*, DWORD, const void*);
    DWORD(WINAPI* EncodeOPUSStartFile)(DWORD, const void*, DWORD, const void*);

    DWORD(WINAPI* MixerStreamCreate)(DWORD, DWORD, DWORD);
    BOOL(WINAPI* MixerStreamAddChannel)(DWORD, DWORD, DWORD);
    BOOL(WINAPI* MixerChannelRemove)(DWORD);
};
