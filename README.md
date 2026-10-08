# VB Recorder

Record any audio on Windows, from several sources at once, without virtual
cables, extra drivers or complicated routing setups.

VB Recorder captures audio from microphones, keyboards, mixers and sound cards,
and also from running processes, such as browsers, music players and calling
apps. All together, with individual volume control for each source, and with
very little latency: you can record your performance over a playback.

The program is light, portable and runs in the system tray.

Starting with version 1.4, VB Recorder is written in C++: the program got much
smaller and records without stutters. The windows, dialogs, hotkeys and
`VBRecorder.ini` are the same as before. The Python versions remain available
in the earlier releases and in the repository history.

## Features

- **Multiple simultaneous sources.** Add as many sources as you like to the
  recording list, mixing audio devices and processes.
- **Per-process capture.** Using the Windows per-process capture API (WASAPI),
  the audio of a specific program is captured directly, without a virtual
  cable.
- **Per-source volume.** Adjust each source from -20 dB to +20 dB, to balance
  weak microphones without affecting the others.
- **Test before recording.** The Test button lets you hear the device with the
  volume already set.
- **Separate files.** With two or more sources, each one can go to its own
  file, named after the source: `recording (Microphone).wav`,
  `recording (chrome.exe).wav`.
- **Play and delete.** Listen to the last recording and delete it if it did not
  turn out well, from the program itself or from the tray menu.
- **Global hotkeys**, configurable, that work with the window minimized or
  hidden.
- **Settings window** with language, start minimized, minimize to tray, window
  always on top and buffer size, besides the hotkeys.

## Formats

| Format | Notes                                          |
|--------|------------------------------------------------|
| MP3    | bitrate from 8 to 320 kbps                     |
| WAV    | uncompressed, 16-bit                           |
| AIFF   | uncompressed, 16-bit                           |
| OGG    | adjustable bitrate up to 500 kbps              |
| OPUS   | modern, highly efficient codec, ideal for voice |
| FLAC   | lossless, compression level from 0 to 8        |

Sample rate from 8000 Hz to 48000 Hz, mono or stereo.

## Stutter-free audio

Recording goes through a real buffer: each source delivers its audio into a
queue of its own, and mixing and encoding happen on a separate thread, at the
pace of the computer clock, with a delay equal to the buffer size chosen in the
settings. A source that is late by less than the buffer loses nothing, and a
source that is later than that is waited for instead of leaving a hole in the
recording. Differences between sound card clocks are compensated gradually,
without cuts, and encoding and disk writes never hold up the capture.

While recording, the program asks Windows not to slow it down (EcoQoS), runs
its audio threads in the system audio scheduler (MMCSS) and keeps the computer
from going to sleep. When you stop, everything that was still in the buffer
goes to the file.

## Languages

VB Recorder starts in the language of the Windows interface, and you can choose
another one in Settings. Available languages:

Brazilian Portuguese, European Portuguese, English, Spanish, French, German,
Italian, Russian and Vietnamese.

If Windows is in a language without a translation, the program shows up in
Brazilian Portuguese, its original language.

**Want to translate it into your language?** Copy `locale/vbrecorder.pot` to
`locale/<code>/LC_MESSAGES/vbrecorder.po`, fill in the texts and run
`tools/language_compiler.py` (it needs `polib`: `pip install polib`). Anything
left untranslated keeps showing in Portuguese, so you can translate a little at
a time. Then add a line for the new language to `res/locale.rc`, so it is
embedded in the executable. Contributions are welcome through [Pull
Request](https://github.com/vrivrixd/vbrecorder/pulls).

To test a translation without compiling, put the `locale` folder next to
`VBRecorder.exe`: what is in it takes precedence over the embedded catalogs.

## Requirements

- Windows 10 or later, 64-bit.
- For process capture, Windows 10 version 2004 (May 2020) or later.
- No additional drivers or virtual cables required.

## Installation

Download `VBRecorder.zip` from the
[Releases](https://github.com/vrivrixd/vbrecorder/releases) page, extract it
wherever you like and run `VBRecorder.exe`. There is no installer: the program
creates `VBRecorder.ini` with your settings in the folder it is started from.

The `lib` folder holds the BASS libraries and must stay next to the executable.

## Building from source

The program builds with the GCC from [WinLibs](https://winlibs.com/):

```
winget install BrechtSanders.WinLibs.POSIX.UCRT
```

Then, from the project folder:

```
mkdir build
windres -I res -i res\app.rc -o build\app.o
windres -i res\locale.rc -o build\locale.o
type nul > build\empty.c
gcc -c build\empty.c -o build\default-manifest.o
g++ -std=c++17 -O2 -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_WIN32_WINNT=0x0A00 -municode -mwindows src\*.cpp build\app.o build\locale.o -lole32 -lcomctl32 -lshell32 -luser32 -lgdi32 -ladvapi32 -luuid -static -s -Bbuild\ -o build\VBRecorder.exe
```

The empty `default-manifest.o` takes the place of the manifest GCC adds to
every executable, which would clash with the program's own manifest
(`res/app.manifest`).

To run it, put the `lib` folder next to `VBRecorder.exe`.

Structure:

- `src/main.cpp` — main window, system tray and global hotkeys.
- `src/dialogs.cpp` — settings, hotkeys and recording sources.
- `src/audio_engine.cpp` — recording, mixing, encoding and playback through BASS.
- `src/process_audio.cpp` — per-process audio capture (WASAPI).
- `src/config.cpp` — reading and writing `VBRecorder.ini`.
- `src/i18n.cpp` — language choice and reading of the translation catalogs.
- `src/hotkeys.cpp` — hotkeys written as text.
- `src/ui.cpp` — interface foundation: native controls and window layout.
- `res/` — manifest, version information and the list of embedded translations.
- `locale/` — translation catalogs, one per language.
- `lib/` — BASS libraries.
- `tools/language_compiler.py` — compiles the `.po` files into `.mo`.

## License

The VB Recorder source code is distributed under the
[GNU General Public License v3.0](LICENSE).

The libraries in the `lib` folder (BASS and its add-ons) are **not** covered by
that license. They belong to [un4seen Developments](https://www.un4seen.com/)
and follow the BASS terms, which make it free for non-commercial use.

## Author

VB Recorder was developed by **Vitor Bruski**.

### Translations

- Vietnamese: **nguyenninhhoang**

Questions, suggestions and bug reports are welcome in the
[Issues](https://github.com/vrivrixd/vbrecorder/issues) tab.
