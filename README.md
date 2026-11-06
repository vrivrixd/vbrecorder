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
- **Background audio.** A file, or a folder whose audio files play in name
  order, goes into the recording as one more source, such as music under a
  voice. "Repeat When Finished" starts it over at the end; unchecked, it plays
  once. With "Shuffle", a folder plays in a random order, a new one each time
  it repeats. With "Include Subfolders", the audio files of the folders inside
  it play too, each folder after the files of the one above it. It has its own volume, equalizer, effects and stereo setting.
  It is only heard in the recording, unless "Play While Recording" is checked:
  then it is also played, as it is recorded. Pausing
  pauses it too, and continuing the last recording goes on from where it
  stopped.
- **Stream URL.** An internet radio goes into the recording as one more
  source: paste its link, or the path of an M3U, PLS or M3U8 playlist on the
  computer (Browse File finds it). Links to playlists on the internet work too,
  HLS (M3U8) included, and radios in MP3, AAC and AAC+. It has its own volume, balance, equalizer, effects,
  side chain and stereo setting, and Test plays it with all of them. It
  connects on its own, without holding up the recording: until it does, and
  whenever the connection drops, it is silent, and it connects again by itself.
  With "Play While Recording", it is also heard while it is recorded.
- **Channels to record.** Each source can record both of its channels, or only
  the left or only the right one: a microphone that an interface puts on the
  left channel alone, for example. "Stereo" still decides where it goes: with
  it checked, the channel stays on its side; unchecked, it goes to both, at its
  own level.
- **Per-source volume.** Adjust each source from -30 dB to +30 dB, to balance
  weak microphones without affecting the others.
- **Per-source balance.** Place each source between left and right in a
  stereo recording: the slider goes from 0, all to the left, to 100, all to
  the right, with 50 in the center, and the screen reader says the position.
  A mono source, such as a microphone, is placed too. Editing a source while
  recording changes its balance right away.
- **Stereo or mono per source.** A source with "Stereo" unchecked is mixed
  down to mono, while the others stay in stereo.
- **Test before recording.** The Test button lets you hear the device with the
  volume and balance already set, and with its equalizer, effects, noise
  remover and stereo setting.
- **Equalizer per source.** Ten bands, from 31 Hz to 16 kHz, each with its
  own gain, a preamp, and presets: the classic Winamp ones, plus curves for
  voice, podcast, telephone and more. The sliders say the gain in dB to the
  screen reader: the arrows move 0.5 dB, Page Up and Page Down 3 dB.
- **Effects per source.** Compressor, pitch shift, chorus, flanger, delay,
  reverb and convolution reverb, each with its controls and
  presets (the reverb ones based on Audacity's). They are applied in that
  order, after the noise remover and the equalizer. The convolution reverb
  takes an impulse response of your own, a wav or flac of up to 10 seconds,
  and gives the source the sound of that place. The delay
  has a real ping-pong mode: the echoes take turns between left and right,
  even with a microphone, which has the same sound on both sides. The reverb
  has a pre-delay. Each effect starts with a Flat preset, which changes
  nothing, and its sliders say the value itself (ms, dB, semitones): the
  arrows move one step, Page Up and Page Down a larger one.
- **Noise remover** for audio devices, with RNNoise: it cleans up fans, hiss
  and background noise from a microphone, with an adjustable intensity, shown
  only while the remover is on.
- **Side chain ducking.** A source can turn the others down while it has
  sound: with the microphone and Discord set to reduce the others, the music
  and the game go down while someone speaks, and come back after. A source
  takes no part until it is set to. Each source that reduces the others has
  its threshold and hold time; each one that is reduced has how much it goes
  down (down to silence) and how fast it goes down and comes back. It applies
  to the recording only, separate files included, not to what is heard.
- **Separate files.** With two or more sources, each one can go to its own
  file, named after the source: `recording (Microphone).wav`,
  `recording (chrome.exe).wav`. With "Record in a Subfolder", checked by
  default, each recording goes into a folder of its own, named after the
  file, so the files of different recordings do not mix; deleting the
  recording removes the folder too, and renaming it can rename the folder.
- **Tags.** Configure Tags, in the main window, sets the title, contributing
  artists, album artist, album, year, genre, track and comment written into the
  recordings, in every format.
  After each field, Variables opens a menu of what can go into it, put where
  the cursor is: the file name (`$name`), the format (`$format`), the part
  number (`$part`), the date (`$date`, `$year`, `$month`, `$day`), the time
  (`$time`), the user name (`$user`) and the computer name (`$computer`).
  Until the tags are set, the title is the file name and both artists are VB
  Recorder; an empty field writes no tag. In MP3, OGG, OPUS and FLAC a double
  quote in a tag becomes an apostrophe.
- **Split recording.** In the settings, "Split Recording Every" goes on in a
  new file every so often, typed as the recording duration is: `30:00` is
  half an hour, `1:00:00` an hour, and a single number is seconds. No audio
  is lost or repeated between the files: `recording 001.mp3`, `recording 002.mp3`... Each part
  has its own tags, with its number in `$part`. Separate files are split at the
  same moments, and continuing the last recording goes on in its last part.
  Empty, the default, does not split.
- **Play, rename and delete.** Listen to the last recording, rename it, and
  delete it if it did not turn out well, from the program itself or from the
  tray menu. Rename Recording opens a field for each file (and one for its
  folder), with the name without the extension; Continue Last Recording goes
  on in the renamed files.
- **Continue the last recording.** After stopping, the files are ready to be
  played or opened in any editor, and Continue Last Recording goes on
  recording in the same files, as if the recording had only been paused. The
  Pause / Continue hotkey does the same when nothing is recording. With MP3,
  OGG and OPUS, the part already recorded is encoded again.
- **Global hotkeys**, configurable, that work with the window minimized or
  hidden: record, pause and continue, stop, play, rename, delete, open the
  recordings folder, show the window and close the program.
- **Tray menu.** With the window minimized or hidden, the tray icon's menu
  records, pauses, stops, continues the last recording, plays, renames and
  deletes it, changes the format and what to do when the recording ends, and
  opens Configure Tags, Recording Schedule, the recordings folder and Settings. A window opened from there closes back to
  where you were, and the main window stays minimized.
- **Countdown**, optional: three low beeps, one per second, and the recording
  starts on the high one, as at the start of a race. Stop cancels it.
- **Pre-recording.** In the settings, "Pre-Recording (seconds)", up to 60,
  records what happened before Record was pressed: with the program open, the
  sources are captured all the time and their last seconds are kept, so the
  recording starts that much earlier. It also applies to scheduled recordings
  and to Continue Last Recording, which then goes on with what was said while
  you were getting to the button. Background audio starts when Record is
  pressed. 0, the default, turns it off, and then nothing is captured until
  you record.
- **Silence detection.** Silence Detection, in the main window, acts once
  the recording has started, by Record, a schedule or Continue Last
  Recording: after the silence time with every source quiet (background audio
  aside), it pauses until any source reaches the sensitivity, in dB, again, or
  stops for good, as chosen. Each return keeps a little of what came just
  before the sound (the detection's own pre-recording, 150 ms by default), so
  the first syllable is never cut; the silence before a pause stays in the
  file, and nothing of the time paused. Separate files pause together, so they
  stay in sync. Tones for what it does by itself are optional.
- **Recording duration**, optional: type how long to record (`1:30:00` is an
  hour and a half), and the recording stops by itself when it gets there.
  Pausing also pauses the count.
- **When the recording ends**, in the main window: do nothing (the default),
  close the program, hibernate, sleep, shut down or restart the computer, as
  soon as the recording stops, by Stop or by itself. After doing it, the
  choice goes back to doing nothing. Hibernate and sleep are listed only where
  the computer has them; with Modern Standby, sleep turns the screen off, which
  is how those computers go to sleep.
- **Start with Windows**, optional. The program stays portable: if its folder
  moves, it points Windows to the new place the next time it opens.
- **Scheduled recordings.** Recording Schedule, in the main window, keeps a
  list of times to start recording by itself, as if Record were pressed:
  on a given date, or every day when the date is left empty. The time is
  typed as `14:30`, or with seconds, as `14:30:15`. Each one may
  have its own recording duration; without one, the duration field of the
  main window counts. "Remove When Done" takes a schedule out of the list
  once its recording starts. The program does this itself, without the
  Windows Task Scheduler, so it must be open (in the tray is enough) and the
  computer awake.
- **More than one instance.** With "Allow Multiple Instances", in the
  settings, VB Recorder can be opened more than once at the same time. Off by
  default. Only the first one open looks for updates by itself.
- **Settings window** with language, automatic update checks, start with
  Windows, start minimized, minimize to tray, window always on top, multiple
  instances, countdown, pre-recording, split recording and buffer size,
  besides the hotkeys. The pre-recording and the buffer are spin boxes: the
  arrows change them by one, and they can still be typed.
- **Automatic updates.** When it starts, the program checks GitHub for a new
  version and, if you want, downloads the right one for your Windows (32 or 64
  bits), installs it and opens it again. What is new in the version shows
  first, in the program's language. The check can also be done from the
  settings, and turned off.

## Formats

| Format | Notes                                                        |
|--------|--------------------------------------------------------------|
| MP3    | bitrate from 8 to 320 kbps                                   |
| WAV    | uncompressed, 8, 16, 24 or 32-bit, or 32-bit floating point  |
| AIFF   | uncompressed, 8, 16, 24 or 32-bit, or 32-bit floating point  |
| OGG    | bitrate from 8 to 500 kbps                                   |
| OPUS   | modern, highly efficient codec, ideal for voice; 6 to 510 kbps |
| FLAC   | lossless, 8, 16, 24 or 32-bit, compression 0 to 8            |
| WMA    | through the codecs of Windows: WMA, or WMA Pro at 24-bit     |
| M4A    | AAC through the encoder of Windows; 96 to 192 kbps            |

Sample rate from 8000 Hz to 384000 Hz in WAV, AIFF and FLAC, and up to 48000 Hz
in MP3, OGG and OPUS; mono or stereo. Each list offers only what the format
takes with the other choices: the bitrates follow the sample rate and the
channels (OGG and MP3 take fewer at low sample rates, and WMA has the ones
Windows offers), so a recording never fails to start over a combination its
encoder refuses. A WAV file goes past 4 GB as RF64. WMA needs the Windows
Media codecs, which the N editions of Windows only have with the Media Feature
Pack; without them, it is not offered. M4A likewise: it uses the AAC encoder
that comes with Windows, whose patents Windows covers, at 44100 or 48000 Hz,
mono or stereo, at 96, 128, 160 or 192 kbps.

## Stutter-free audio

Recording goes through a real buffer: each source delivers its audio into a
queue of its own, and mixing and encoding happen on a separate thread, at the
pace of the computer clock, with a delay equal to the buffer size chosen in the
settings, 2000 ms by default. A source that is late by less than the buffer loses nothing, and a
source that is later than that is waited for instead of leaving a hole in the
recording. Audio is placed by the moment it was captured, not by when it
arrived, so a capture that was held up and then hands over what it kept is
never taken for a gap. When a source really stops delivering (a loopback with nothing
playing, a sound card that went to sleep, a program that went quiet), the time
it was gone becomes silence and its audio comes back exactly in time. A device
or process capture that drops out is opened again by itself. Differences
between sound card clocks are compensated gradually, without cuts, and encoding
and disk writes never hold up the capture.

While recording, the program asks Windows not to slow it down (EcoQoS), runs
its audio threads, the BASS capture of each sound card included, in the system
audio scheduler (MMCSS) and keeps the computer
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
g++ -std=c++17 -O2 -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_WIN32_WINNT=0x0A00 -Ithird_party\rnnoise\include -municode -mwindows src\*.cpp build\rnn_*.o build\app.o build\locale.o -lole32 -loleacc -lcomctl32 -lshell32 -luser32 -lgdi32 -ladvapi32 -luuid -lwinhttp -lbcrypt -static -s -Bbuild\ -o build\VBRecorder.exe
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
- `src/fx.cpp` — equalizer, effects, noise remover and side chain settings, and their presets.
- `src/fx_chain.cpp` — the processing of each source (BASS_FX, RNNoise and the delay).
- `src/convolver.cpp` — the convolution reverb.
- `src/background.cpp` — background audio: the files of a source, played in turn.
- `src/net_source.cpp` — a stream URL as a source, connected and connected again on its own thread.
- `src/m4a_writer.cpp` — M4A files, with the AAC encoder of Windows (Media Foundation).
- `src/tags.cpp` — the tags of the recordings, their variables, and how each format takes them.
- `src/schedule.cpp` — scheduled recordings: their dates and times.
- `src/fx_dialogs.cpp` — the equalizer, effects and noise remover dialogs.
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

`bass_aac.dll`, in the `lib` folder, is BASS_AAC, by Sebastian Andersson,
with portions by Nero AG (FAAD2), free to use and distribute under the GNU
GPL. It lets stream URLs in AAC and AAC+, which many radios use, start at once.

## Author

VB Recorder was developed by **Vitor Bruski**.

### Translations

- Vietnamese: **nguyenninhhoang**

Questions, suggestions and bug reports are welcome in the
[Issues](https://github.com/vrivrixd/vbrecorder/issues) tab.
