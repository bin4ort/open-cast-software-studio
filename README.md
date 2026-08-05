# voice_anonymizer

A C++20 voice anonymization toolkit: disguises the speaker in audio files and
in real time via VAD-gated pitch/formant shifting, and can redact personally
identifiable information (names, emails, phones, URLs) from transcripts.

**License: GPL-3.0-only** (see [LICENSE](LICENSE)).

## Features

- **Batch anonymization** — shift the pitch and formants of all speech in an
  audio file (VAD-gated, so non-speech stays untouched).
- **Selective re-voicing** — when a transcription is available, only the
  segments containing names/PII are re-synthesized; the rest stays natural.
- **Transcript extraction & redaction** — optional speech-to-text via an
  external `whisper` binary (`whisper.cpp` or `openai/whisper`), with regex +
  heuristic redaction of emails, phone numbers, URLs and personal names.
- **Real-time live masking** — mask your own voice through the microphone
  while you talk, with a native Qt6 GUI or the CLI.
- **Native Qt6 GUI** (`voice_anon_gui`) and a full-featured CLI
  (`voice_anon`).
- **CLI install wizard** (`install/install.py`) with license acceptance,
  dependency installation (auto or manual) and uninstall.

## Requirements

- CMake ≥ 3.20, C++20 compiler, `make`
- [PortAudio](http://portaudio.com/), [libsndfile](http://www.mega-nerd.com/libsndfile/),
  [libsamplerate](http://www.mega-nerd.com/SRC/)
- Optional: `ffmpeg` (extra codecs, e.g. MP3/M4A), Qt6 Widgets (GUI),
  a `whisper` binary (transcription)

The GUI is built automatically when Qt6 is found; pass `-DVA_GUI=OFF` to
disable it.

## Build

```sh
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
# binaries: build/voice_anon, build/voice_anon_gui
# tests:    ctest --test-dir build
```

## Install

Run the interactive wizard:

```sh
python3 install/install.py          # build + install
python3 install/install.py uninstall  # remove installed files
```

The wizard will:

1. ask you to accept the GPL-3.0 license;
2. detect your distro and check all dependencies;
3. let you **auto-install** missing packages via your package manager (with a
   preview of the exact `sudo` commands) or **install them manually** — if you
   choose manual and the packages are not present yet, it tells you what's
   missing and exits cleanly (rerun the wizard after installing);
4. build and install to `/usr/local` (system, sudo) or `~/.local` (user);
5. optionally create a desktop menu entry for the GUI.

## CLI usage

```sh
# Anonymize an audio file (default: shift all speech +2 st, formants follow)
voice_anon process interview.wav -o anonymized.wav --preset strong

# Formant-preserving pitch shift (natural voice, new pitch)
voice_anon process interview.wav --preset subtle

# Only re-voice the segments containing names (needs whisper + --transcribe)
voice_anon process interview.wav --mode selective --transcribe --pitch 4

# Real-time live masking (default mic/speaker)
voice_anon live --pitch 5
voice_anon live --preset chipmunk --mute-silence

# Transcript extraction + redaction
voice_anon transcribe interview.wav -o transcript.txt --redact
voice_anon anonymize transcript.txt -o transcript_redacted.txt

# Helpers
voice_anon presets
voice_anon devices
voice_anon info
voice_anon gui
```

### Presets

| name      | pitch   | formant | description |
|-----------|---------|---------|-------------|
| subtle    | +2 st   | 0       | Slight shift, formants preserved |
| strong    | +6 st   | +6 st   | Larger shift, formants follow |
| deep      | -5 st   | -3 st   | Lower, heavier voice |
| chipmunk  | +12 st  | +12 st  | High and bright |
| robotic   | +4 st   | +2 st   | Synthetic timbre + denoise |

`--formant 0` preserves formants (pitch shift only, most natural);
`--formant N` matches pitch for a stronger disguise.

## GUI usage

`voice_anon_gui` opens a window with three tabs:

- **Process file** — pick an audio file, choose a preset or set pitch/formant
  manually, select global vs selective mode, and process. Input and output
  waveforms are shown.
- **Live masking** — choose mic/speaker devices and pitch, then Start. Your
  voice is masked in real time (optional VAD-based muting and dry run).
- **Transcript** — paste a transcript and redact names / emails / phones.

## Transcription

Transcription is optional and requires a `whisper` binary on your `PATH`
(`whisper-cli` from whisper.cpp, or `whisper` from openai/whisper). Use
`--whisper <path>` to point at a specific executable. When no whisper binary
is available, commands that need it fail with a clear message.

## Architecture

```
src/
├── main.cpp / cli.cpp        # CLI dispatch + subcommands
├── audio/                    # PortAudio device mgmt, sndfile+ffmpeg file I/O, ring buffer
├── dsp/                      # FFT, STFT/OLA, phase vocoder (pitch shift),
│                             #   cepstral formant shifter, spectral denoise, VAD, pipeline
├── transcribe/               # whisper subprocess wrapper (JSON segments)
├── anonymize/                # regex + heuristic redactor
├── processing/               # batch job orchestrator
├── util/                     # tiny JSON parser (no external deps)
└── ui/                       # Qt6 main window, waveform widget, live engine
```

All DSP is implemented in-house on top of system libraries; there are no
vendored third-party sources. The pitch shifter uses a phase-vocoder
time-stretch plus sample-rate conversion (libsamplerate) for quality, with a
streaming variant for the live path.

## Tests

```sh
ctest --test-dir build --output-on-failure
```

Covers FFT/STFT round trips, pitch-shift accuracy (440 Hz -> expected),
the streaming live shifter, and transcript redaction.
