# VB Recorder

Record any audio on Windows, from several sources at once, without virtual
cables, extra drivers or complicated routing setups.

VB Recorder captures audio from microphones, keyboards, mixers and sound cards,
and also from running processes, such as browsers, music players and calling
apps. All together, with individual volume control for each source, and with
very little latency: you can record your performance over a playback.

The program is light, runs in the system tray, and comes both portable and
with an installer.

Starting with version 1.4, VB Recorder is written in C++: the program got much
smaller, runs on 32-bit and 64-bit Windows and records without stutters. The
windows, dialogs, hotkeys and `VBRecorder.ini` are the same as before. The
Python versions remain available in the earlier releases and in the repository
history.

## Features

- **Multiple simultaneous sources.** Add as many sources as you like to the
  recording list, mixing audio devices and processes.
- **Change sources while recording.** Add, edit or remove sources without
  stopping: a new source joins in sync, a removed one first records what it
  already had, and editing a source changes its volume right away. With
  separate files, a source added midway gets a file that starts with silence
  from the beginning, and the file of a removed source goes on in silence (and
  takes the source back if you add it again), so every file stays in sync.
- **Per-process capture.** Using the Windows per-process capture API (WASAPI),
  the audio of a specific program is captured directly, without a virtual
  cable. The list shows the programs playing sound; "Show All Processes" lists
  the others too.
- **Capture by executable.** Add a program by its executable name (`game.exe`),
  even before it is open: it is recorded whenever it runs, however many times
  it is opened and closed, and however many copies are open. With separate
  files, it goes to `recording (game.exe).wav`. The Browse button finds the
  program on disk and keeps only its file name.
- **Per-source volume.** Adjust each source from -30 dB to +30 dB, to balance
  weak microphones without affecting the others.
- **Stereo or mono per source.** A source with "Stereo" unchecked is mixed
  down to mono, while the others stay in stereo.
- **Test before recording.** The Test button lets you hear the device with the
  volume already set, and with its equalizer, noise remover and stereo setting.
- **Equalizer per source.** Ten bands, from 31 Hz to 16 kHz, each with its
  own gain, a preamp, and presets: the classic Winamp ones, plus curves for
  voice, podcast, telephone and more. Every slider goes from 0 to 100, as the
  volume does, which is what the screen reader says.
- **Noise remover** for audio devices, with RNNoise: it cleans up fans, hiss
  and background noise from a microphone, with an adjustable intensity.
- **Separate files.** With two or more sources, each one can go to its own
  file, named after the source: `recording (Microphone).wav`,
  `recording (chrome.exe).wav`.
- **Play and delete.** Listen to the last recording and delete it if it did not
  turn out well, from the program itself or from the tray menu.
- **Continue the last recording.** After stopping, the files are ready to be
  played or opened in any editor, and Continue Last Recording goes on
  recording in the same files, as if the recording had only been paused. The
  Pause / Continue hotkey does the same when nothing is recording. With MP3,
  OGG and OPUS, the part already recorded is encoded again.
- **Global hotkeys**, configurable, that work with the window minimized or
  hidden: record, pause and continue, stop, play, delete, show the window and
  close the program.
- **Countdown**, optional: three low beeps, one per second, and the recording
  starts on the high one, as at the start of a race. Stop cancels it.
- **Recording duration**, optional: type how long to record (`1:30:00` is an
  hour and a half), and the recording stops by itself when it gets there.
  Pausing also pauses the count.
- **Start with Windows**, optional. The program stays portable: if its folder
  moves, it points Windows to the new place the next time it opens.
- **Settings window** with language, automatic update checks, start with
  Windows, start minimized, minimize to tray, window always on top, countdown
  and buffer size, besides the hotkeys.
- **Automatic updates.** When it starts, the program checks GitHub for a new
  version and, if you want, downloads the right one for your Windows (32 or 64
  bits), installs it and opens it again. The check can also be done from the
  settings, and turned off.

## Formats

| Format | Notes                                          |
|--------|------------------------------------------------|
| MP3    | bitrate from 8 to 320 kbps                     |
| WAV    | uncompressed, 8, 16, 24 or 32-bit              |
| AIFF   | uncompressed, 8, 16, 24 or 32-bit              |
| OGG    | adjustable bitrate up to 500 kbps              |
| OPUS   | modern, highly efficient codec, ideal for voice |
| FLAC   | lossless, 8, 16 or 24-bit, compression 0 to 8  |

Sample rate from 8000 Hz to 384000 Hz in WAV, AIFF and FLAC, and up to 48000 Hz
in MP3, OGG and OPUS; mono or stereo.

## Stutter-free audio

Recording goes through a real buffer: each source delivers its audio into a
queue of its own, and mixing and encoding happen on a separate thread, at the
pace of the computer clock, with a delay equal to the buffer size chosen in the
settings. A source that is late by less than the buffer loses nothing, and a
source that is later than that is waited for instead of leaving a hole in the
recording. When a source really stops delivering (a loopback with nothing
playing, a sound card that went to sleep, a program that went quiet), the time
it was gone becomes silence and its audio comes back exactly in time. A device
or process capture that drops out is opened again by itself. Differences
between sound card clocks are compensated gradually, without cuts, and encoding
and disk writes never hold up the capture.

While recording, the program asks Windows not to slow it down (EcoQoS), runs
its audio threads in the system audio scheduler (MMCSS) and keeps the computer
from going to sleep, also on computers with Modern Standby. When you stop, everything that was still in the buffer
goes to the file.

## Languages

VB Recorder starts in the language of the Windows interface, and you can choose
another one in Settings. Available languages:

Arabic, Bengali, Brazilian Portuguese, Bulgarian, Chinese (Simplified),
Chinese (Traditional), Czech, Danish, English, Esperanto, European Portuguese,
Finnish, French, Galician, German, Hindi, Indonesian, Italian, Japanese,
Korean, Polish, Russian, Slovak, Slovenian, Spanish, Thai, Turkish and
Vietnamese.

If Windows is in a language without a translation, the program shows up in
English. Brazilian Portuguese is the original language of the program.

**Want to translate it into your language?** Copy `locale/vbrecorder.pot` to
`locale/<code>/LC_MESSAGES/vbrecorder.po`, fill in the texts and run
`tools/language_compiler.py` (it needs `polib`: `pip install polib`). Anything
left untranslated shows up in English, so you can translate a little at a
time. Then add a line for the new language to `res/locale.rc`, so it is
embedded in the executable. Contributions are welcome through [Pull
Request](https://github.com/vrivrixd/vbrecorder/pulls).

To test a translation without compiling, put the `locale` folder next to
`VBRecorder.exe`: what is in it takes precedence over the embedded catalogs.

## Requirements

- Windows 10 or later, 32-bit or 64-bit.
- For process capture, Windows 10 version 2004 (May 2020) or later.
- No additional drivers or virtual cables required.

## Installation

From the [Releases](https://github.com/vrivrixd/vbrecorder/releases) page,
download either the installer or the portable version.

**Installer:** `VBRecorder-Setup.exe`, for 64-bit and 32-bit Windows alike. It
needs no administrator rights and installs into `C:\VBRecorder` by default (the
folder can be changed), with Start menu and desktop shortcuts if you want them.
It shows in "Installed apps", from where it can be uninstalled. The installed
program keeps `VBRecorder.ini` in its folder and records into
`Documents\VB Recorder` by default.

**Portable:** `VBRecorder-x64.zip` for 64-bit Windows (most computers) or
`VBRecorder-x86.zip` for 32-bit Windows (it also runs on 64-bit Windows).
Extract it wherever you like and run `VBRecorder.exe`. The program creates
`VBRecorder.ini` with your settings in the folder it is started from (when
Windows starts it, in its own folder), and records into the `Recordings`
folder next to it by default. The `lib` folder holds the BASS libraries and must
stay next to the executable.

Both update themselves in the same way, when a new version comes out.

## Building from source

The release zips are built by GitHub Actions
(`.github/workflows/build.yml`), for both architectures: pushing a version tag
builds them and publishes them in that tag's release.

To build on your own computer, use the GCC from [WinLibs](https://winlibs.com/):

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
for %f in (third_party\rnnoise\src\*.c) do gcc -O2 -msse2 -w -Ithird_party\rnnoise\include -c %f -o build\rnn_%~nf.o
g++ -std=c++17 -O2 -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_WIN32_WINNT=0x0A00 -Ithird_party\rnnoise\include -municode -mwindows src\*.cpp build\rnn_*.o build\app.o build\locale.o -lole32 -lcomctl32 -lshell32 -luser32 -lgdi32 -ladvapi32 -luuid -lwinhttp -lbcrypt -static -s -Bbuild\ -o build\VBRecorder.exe
```

The empty `default-manifest.o` takes the place of the manifest GCC adds to
every executable, which would clash with the program's own manifest
(`res/app.manifest`). The noise remover (RNNoise) is C code: it is compiled
with `gcc` first and linked with the rest. In a `.bat` file, write `%%f` and
`%%~nf` instead of `%f` and `%~nf`.

This builds the 64-bit program. For the 32-bit one, use the i686 build of
WinLibs (from the [WinLibs](https://winlibs.com/) page) with the same commands.
To run it, copy the DLLs of the matching architecture, from `lib/x64` or
`lib/x86`, to a `lib` folder next to `VBRecorder.exe`.

The installer is made with [Inno Setup](https://jrsoftware.org/isinfo.php) 7,
from the folders of the two release zips (their contents, extracted):

```
ISCC /DX64=<64-bit folder> /DX86=<32-bit folder> installer\VBRecorder.iss
```

It writes `dist\VBRecorder-Setup.exe`. The installer comes in the same
languages as the program; the ones Inno Setup does not ship are in
`installer/Languages`.

Structure:

- `src/main.cpp` — main window, system tray and global hotkeys.
- `src/dialogs.cpp` — settings, hotkeys and recording sources.
- `src/audio_engine.cpp` — recording, mixing, encoding and playback through BASS.
- `src/process_audio.cpp` — per-process audio capture (WASAPI).
- `src/config.cpp` — reading and writing `VBRecorder.ini`.
- `src/i18n.cpp` — language choice and reading of the translation catalogs.
- `src/hotkeys.cpp` — hotkeys written as text.
- `src/startup.cpp` — starting with Windows.
- `src/install.cpp` — what changes in the installed copy.
- `src/fx.cpp` — equalizer and noise remover settings, and the equalizer presets.
- `src/fx_chain.cpp` — the processing of each source (BASS_FX and RNNoise).
- `src/fx_dialogs.cpp` — the equalizer and noise remover dialogs.
- `src/ui.cpp` — interface foundation: native controls and window layout.
- `res/` — manifest, version information and the list of embedded translations.
- `locale/` — translation catalogs, one per language.
- `lib/x64`, `lib/x86` — BASS libraries for each architecture.
- `third_party/rnnoise` — RNNoise, the noise remover.
- `.github/workflows/build.yml` — builds and publishes the release zips.
- `installer/` — the Inno Setup script and its extra languages.
- `tools/language_compiler.py` — compiles the `.po` files into `.mo`.

## License

The VB Recorder source code is distributed under the
[GNU General Public License v3.0](LICENSE).

The libraries in the `lib` folder (BASS and its add-ons, BASS_FX among them)
are **not** covered by that license. They belong to
[un4seen Developments](https://www.un4seen.com/) and follow the BASS terms,
which make it free for non-commercial use.

[RNNoise](https://github.com/xiph/rnnoise), in `third_party/rnnoise`, is by
Jean-Marc Valin and contributors (Xiph.Org, Mozilla, Amazon), under the BSD
license in `third_party/rnnoise/COPYING`.

## Author

VB Recorder was developed by **Vitor Bruski**.

### Translations

- Vietnamese: **nguyenninhhoang**

Questions, suggestions and bug reports are welcome in the
[Issues](https://github.com/vrivrixd/vbrecorder/issues) tab.
