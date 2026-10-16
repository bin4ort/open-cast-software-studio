# OCS Studio — Open Cast Software Studio

A C++20 voice transformation toolkit: change your voice to sound male, female,
or like a character — live or on recordings — for voice acting, voice-over,
streaming, or privacy. Includes VAD-gated pitch/formant shifting, gender
conversion, AI-assisted hybrid re-voicing, transcript extraction & redaction, a
virtual microphone for routing the anonymized voice to other apps, and a native
Qt6 GUI.

**License: GPL-3.0-only** (see [LICENSE](LICENSE)).

See [CHANGELOG.md](CHANGELOG.md) for the release history.

## Features

- **Batch transformation** — shift the pitch and formants of all speech in an
  audio file (VAD-gated, so non-speech stays untouched).
- **Gender voices** — Generic / Male / Female presets plus custom
  pitch/formant control, and an AI path that maps a natural-language voice
  description ("young woman from London, slightly deep") onto the settings via
  a local LLM (llama.cpp / Ollama) with offline keyword matching as fallback.
- **AI connectivity settings** — point the app at any Ollama or llama.cpp
  server (localhost, a container, or a remote machine), test the connection,
  and optionally auto- or manually launch the backend from a chosen working
  directory.
- **Hybrid mode** — transcribes the recording, rewrites gendered
  self-references toward the target gender (rules engine, or a local LLM when
  available), globally shifts the voice, and re-voices the rewritten segments
  with TTS in the target voice.
- **Selective re-voicing** — when a transcription is available, only the
  segments containing names/PII are re-synthesized; the rest stays natural.
- **Waveform compare & AI correction** — the original and processed
  recordings are shown as two graphs; a Compare button overlaps them
  (vivid main curve + grayscale overlay) to spot differences, and
  "Correct with AI" feeds the waveform difference to the LLM, which suggests
  corrected voice settings that are applied automatically.
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
- **Logo** — vector logos live in `assets/` (all square 1:1): a single
  vivid gradient chirp with per-cycle dynamics, a breath pause and a slight
  drop shadow (`logo.svg`); framed variants with round frames
  (`logo-framed*.svg`, used for the window and desktop icons);
  light/dark theme versions (`logo-light.svg`, `logo-dark.svg`, picked
  automatically by the GUI from the system theme); a multicolored version
  (`logo-colorful.svg`); a monochrome mark (`logo-mono.svg`); a
  theme-adaptive web version (`logo-adaptive.svg`); and the original tile
  design (`logo-tile.svg`, kept for a possible mobile port). All variants
  are generated from `assets/generate_logo.py`.
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
  process. Input and output waveforms are shown (with a **Compare** button
  that overlaps both graphs — the main recording stays vivid, the other is
  drawn in grayscale so differences stand out), plus a voice preview that
  reads an example text in the configured voice. A **Correct with AI** button
  analyzes the waveform difference between the two recordings, asks the LLM
  for corrected voice settings and re-processes automatically.
- **Live masking** — choose mic/speaker devices, set the voice, then Start.
  Your voice is masked in real time (optional VAD-based muting and dry run).
  Route the output to the speaker, a **virtual mic** (select it in Discord,
  streaming, etc.), or both. A mic test records raw and anonymized samples and
  lets you replay them.
- **Transcript** — paste a transcript and redact names / emails / phones.
- **Settings** — AI backend connection: Ollama and llama.cpp server addresses
  (so the AI can run on a second computer or a remote server), a "Test
  connection" button, and backend launch controls: an auto-launch option
  (starts the backend automatically when the app needs it and nothing is
  listening), a launch command with a working directory, and a manual
  "Launch now" button. Output from a launched backend is logged to
  `/tmp/ocs_ai_launch.log`.

## AI connectivity

The AI features (voice description translation, phrase rewriting, live AI
regeneration) talk to either an **Ollama** server (default
`http://localhost:11434`) or a **llama.cpp** server (default
`http://localhost:8080`). The GUI's Settings tab sets these addresses — use
any host:port, e.g. `http://192.168.1.50:11434` for another machine on your
LAN. "Test connection" probes both endpoints.

The CLI reads the same settings from the environment:

```sh
export OCS_OLLAMA_URL=http://192.168.1.50:11434   # optional
export OCS_LLAMA_URL=http://192.168.1.50:8080     # optional
export OCS_AI_LAUNCH_CMD="ollama serve"           # optional launcher
export OCS_AI_LAUNCH_DIR=/opt/ai                  # optional working dir
export OCS_AI_AUTO_LAUNCH=1                       # optional auto-start
ocs_studio info        # shows which backend is reachable
```

When no backend is reachable, `--mode hybrid` rewriting and voice-prompt
translation fall back to the built-in rules/keyword engine.

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
│                             #   rewriter, live regeneration, neural VC stub
├── transcribe/               # whisper subprocess wrapper (JSON segments)
├── anonymize/                # regex + heuristic redactor
├── tts.cpp                   # espeak-ng text-to-speech wrapper
├── processing/               # batch job orchestrator (global/selective/hybrid)
├── util/                     # tiny JSON parser (no external deps)
└── ui/                       # Qt6 main window, waveform, live engine, voice
                              #   panel, GUI entry points (gui_main/gui_entry)
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
the streaming live shifter, gender profiles, phrase rewriting, transcript
redaction, VAD (streaming + offline), the ring buffer (including wrap-around
and full-buffer behavior), the JSON parser, waveform diff analysis and AI
correction response parsing. Run the suite under AddressSanitizer/UBsan for
audio/DSP work:

```sh
cmake -B build-asan -S . \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -O1"
cmake --build build-asan -j$(nproc)
ctest --test-dir build-asan --output-on-failure
```
