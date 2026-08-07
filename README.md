# OCS Studio — Open Cast Software Studio

A C++20 voice transformation toolkit: change your voice to sound male, female,
or like a character — live or on recordings — for voice acting, voice-over,
streaming, or privacy. Includes VAD-gated pitch/formant shifting, gender
conversion, AI-assisted hybrid re-voicing, transcript extraction & redaction, a
virtual microphone for routing the anonymized voice to other apps, and a native
Qt6 GUI.

**License: GPL-3.0-only** (see [LICENSE](LICENSE)).

## Features

- **Batch transformation** — shift the pitch and formants of all speech in an
  audio file (VAD-gated, so non-speech stays untouched).
- **Gender voices** — Generic / Male / Female presets plus custom
  pitch/formant control, and an AI path that maps a natural-language voice
  description ("young woman from London, slightly deep") onto the settings via
  a local LLM (llama.cpp / Ollama) with offline keyword matching as fallback.
- **Hybrid mode** — transcribes the recording, rewrites gendered
  self-references toward the target gender (rules engine, or a local LLM when
  available), globally shifts the voice, and re-voices the rewritten segments
  with TTS in the target voice.
- **Selective re-voicing** — when a transcription is available, only the
  segments containing names/PII are re-synthesized; the rest stays natural.
- **Transcript extraction & redaction** — optional speech-to-text via
  `whisper-cpp`, with regex + heuristic redaction of emails, phone numbers,
  URLs and personal names.
- **Voice preview** — hear the configured voice speaking an example text via
  TTS (espeak-ng) run through the same shift, or record a mic test.
- **Mic test** — record raw and anonymized samples and replay the last one.
- **Real-time live masking** — mask your own voice through the microphone
  while you talk, via the Qt6 GUI or the CLI.
- **Virtual microphone** — route the anonymized voice to Discord, streaming,
  or recording apps as a selectable input device. If the app closes or
  crashes, the virtual mic goes silent (like the mic died — no
  deanonymization).
- **Native Qt6 GUI** (`ocs_studio_gui`) and a full-featured CLI
  (`ocs_studio`).
- **CLI install wizard** (`install/install.py`) with license acceptance,
  dependency installation (auto or manual), optional-feature tools and
  uninstall.

## Requirements

- CMake ≥ 3.20, C++20 compiler, `make`
- [PortAudio](http://portaudio.com/), [libsndfile](http://www.mega-nerd.com/libsndfile/),
  [libsamplerate](http://www.mega-nerd.com/SRC/)
- Optional: `ffmpeg` (extra codecs, e.g. MP3/M4A), Qt6 Widgets (GUI),
  `whisper-cpp` (transcription), `espeak-ng` (voice preview),
  `onnxruntime` (experimental neural voice conversion, off by default)

The GUI is built automatically when Qt6 is found; pass `-DOCS_GUI=OFF` to
disable it. The install wizard offers to install the optional tools.

## Build

```sh
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
# binaries: build/ocs_studio, build/ocs_studio_gui
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
4. offer to install optional feature tools (whisper-cpp, espeak-ng,
   onnxruntime);
5. build and install to `/usr/local` (system, sudo) or `~/.local` (user);
6. optionally create a desktop menu entry for the GUI.

## CLI usage

```sh
# Anonymize an audio file (default: shift all speech +2 st, formants follow)
ocs_studio process interview.wav -o anonymized.wav --preset strong

# Formant-preserving pitch shift (natural voice, new pitch)
ocs_studio process interview.wav --preset subtle

# Gender conversion
ocs_studio process interview.wav --gender female
ocs_studio process interview.wav --gender male --pitch -5

# Hybrid: global gender shift + re-voiced behavior phrases
# (needs whisper-cpp + espeak-ng)
ocs_studio process interview.wav --mode hybrid --gender male

# Only re-voice the segments containing names (needs whisper + --transcribe)
ocs_studio process interview.wav --mode selective --transcribe --pitch 4

# Real-time live masking (default mic/speaker)
ocs_studio live --pitch 5
ocs_studio live --preset chipmunk --mute-silence

# Transcript extraction + redaction
ocs_studio transcribe interview.wav -o transcript.txt --redact
ocs_studio anonymize transcript.txt -o transcript_redacted.txt

# Helpers
ocs_studio presets
ocs_studio devices
ocs_studio info
ocs_studio gui
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

`ocs_studio_gui` opens a window with three tabs:

- **Process file** — pick an audio file, set the target voice (gender presets,
  DSP sliders, or an AI prompt), choose Global / Selective / Hybrid mode, and
  process. Input and output waveforms are shown, plus a voice preview that
  reads an example text in the configured voice.
- **Live masking** — choose mic/speaker devices, set the voice, then Start.
  Your voice is masked in real time (optional VAD-based muting and dry run).
  Route the output to the speaker, a **virtual mic** (select it in Discord,
  streaming, etc.), or both. A mic test records raw and anonymized samples and
  lets you replay them.
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
├── audio/                    # PortAudio device mgmt, sndfile+ffmpeg I/O, ring
│                             #   buffer, playback/record helpers, virtual mic
├── dsp/                      # FFT, STFT/OLA, phase vocoder (pitch shift),
│                             #   cepstral formant shifter, gender profiles,
│                             #   spectral denoise, VAD, pipeline
├── ai/                       # LLM wrapper (llama.cpp/Ollama), gendered-phrase
│                             #   rewriter, experimental neural VC interface
├── transcribe/               # whisper subprocess wrapper (JSON segments)
├── anonymize/                # regex + heuristic redactor
├── tts.cpp                   # espeak-ng text-to-speech wrapper
├── processing/               # batch job orchestrator (global/selective/hybrid)
├── util/                     # tiny JSON parser (no external deps)
└── ui/                       # Qt6 main window, waveform, live engine,
                              #   voice panel
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
the streaming live shifter, gender profiles, phrase rewriting and transcript
redaction.
