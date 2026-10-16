# Changelog

All notable changes to OCS Studio are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

- **Phase vocoder infinite loop** — `hop_` could become 0 when `osamp > nfft`,
  causing an infinite loop. A guard clamps `hop_` to 1.
- **JSON surrogate pair handling** — high surrogates were encoded before
  surrogate-pair detection, producing garbage for non-BMP Unicode. Detection
  now runs before UTF-8 encoding; orphan surrogates are escaped individually.
- **JSON control char escaping** — control characters (0x00–0x1F) were not
  always escaped, producing invalid JSON sent to LLM backends.
- **Regen stop_ not reset** — the regen engine's `stop_` flag was not reset
  after regeneration completed, permanently deadening live regeneration
  after a stop/restart cycle.
- **Whisper output path** — transcription output used hardcoded paths without
  basename extraction or shell quoting; now uses deterministic `-od` + basename
  with proper quoting.
- **Virtual mic data race** — read/write I/O on the virtual mic ring buffer
  was not protected by mutex; added mutex protection.

### Added

- **Module documentation** — `README.md`, `standard.md`, and `test/test.sh`
  for every module under `src/*/`: `ai`, `anonymize`, `audio`, `dsp`,
  `processing`, `transcribe`, `ui`, `util`.
- **Version header** — `src/version.h` (hardcoded) and `src/version.h.in`
  (CMake-configurable) with `ocs::VERSION`, `ocs::VERSION_MAJOR/MINOR/PATCH`.
- **Package build files** — `PKGBUILD` (Arch Linux / pacman) and
  `debian/` directory (Debian / apt).
- **QMS module documentation** — 8 module READMEs, 8 standard.md files,
  8 test/test.sh scripts satisfy the `docs-coverage` check.

## [0.2.0] — 2026-08-07

### Fixed

- **Live masking crashed on startup** — the streaming VAD used an empty
  sample-accumulator vector, causing an out-of-bounds write (`frame_acc_[0]`)
  in every live session (UB/crash).
- **Ring buffer could overwrite unread audio** — when the ring was exactly
  full, the masked head/tail arithmetic made it look empty and writes
  continued, corrupting buffered audio instead of dropping samples. The
  indices are now monotonic 64-bit counters.
- **GUI crashed when processing twice** — reassigning a joinable
  `std::thread` invoked `std::terminate`; the window could also terminate on
  close after a completed run.
- **GUI wrote stale audio to the speakers** — the live worker wrote the whole
  input block even when the streaming shifter produced fewer samples
  (leftover data from earlier blocks was audible).
- **`ocs_studio gui` never worked** — the `gui` subcommand referenced a
  non-existent symbol and an undefined macro; it now launches the GUI via a
  weak-symbol launcher when Qt6 is built (and prints a correct message,
  `OCS_GUI=ON`, when not).
- **LiveEngine left half-started on stream failure** — a failed
  `start_live()` kept the worker thread, virtual mic and PortAudio state
  alive; the engine now tears everything down.
- **CLI crashes on bad numeric options** — `--pitch`, `--formant`,
  `--frames`, `--rate`, `--in`, `--out` accepted garbage (`atof` → 0) or
  missing values silently; out-of-range shifts (±24 st) now error cleanly
  instead of aborting on an uncaught vocoder exception. DSP failures inside
  `job::run` are converted into readable error messages for both CLI and GUI.
- **TTS/regen temp files were predictable** — `mkstemps` was only used when
  ffmpeg support was compiled in, so TTS could write to a fixed
  `/tmp/ocs_tts_XXXXXX.wav`; the AI regen path always used a fixed
  `/tmp/ocs_regen.wav`. Both now always create unique temp files.
- **Transcript write failures were reported as success** — `--write-transcript`
  now surfaces a write error instead of silently continuing.
- **Missing input files got a confusing error** — reading a nonexistent file
  now reports "No such file or directory" instead of "unsupported format".
- **GUI read Qt widgets from worker threads** — the mic-test duration spin
  box was read from a detached thread (data race); it is now captured before
  the thread starts, and preview/record/replay workers are tracked so the
  window destructor waits for them (fixes use-after-free on close).
- **JSON `\u` escapes produced invalid UTF-8** for code points above U+07FF;
  the parser now emits proper 2–4 byte sequences.
- **Selective mode re-ran VAD per segment** — the speech map is now computed
  once and reused for all PII segments.

### Added

- **Settings tab** — AI backend connectivity: configurable Ollama / llama.cpp
  server addresses (local, container, or remote machine), a connection test
  button, and backend launch controls: auto-launch when needed, a launch
  command with working directory, and a manual "Launch now" button. All
  persisted in the settings file; the CLI reads the same values from
  `OCS_OLLAMA_URL`, `OCS_LLAMA_URL`, `OCS_AI_LAUNCH_CMD`, `OCS_AI_LAUNCH_DIR`,
  `OCS_AI_AUTO_LAUNCH`.
- **App logo** — redesigned marks in `assets/`, generated by
  `assets/generate_logo.py`. All variants are square (1:1) and show a
  single vivid gradient chirp (mild-S frequency rise with a slow, round
  first cycle, per-cycle amplitude dynamics, a breath pause, a
  full-amplitude start with a raised-cosine fade, a tapering stroke) with
  a slight drop shadow. The framed variants use round frames. Variants:
  minimal mark (`logo.svg`), framed marks
  (`logo-framed*.svg` — used for the window and desktop icons), light/dark
  theme versions (`logo-light.svg`, `logo-dark.svg` — the GUI picks one from
  the system palette and switches live on theme changes), a multicolored
  wave (`logo-colorful.svg`), a monochrome mark (`logo-mono.svg`), a
  CSS-adaptive web version (`logo-adaptive.svg`), and the tile design
  (`logo-tile.svg`, kept for a potential mobile port).
- **Waveform compare** — a Compare button on the Process tab overlaps the
  original and processed graphs: the main recording stays vivid, the other is
  drawn grayscale behind it.
- **AI correction from the graphs** — "Correct with AI" extracts the RMS
  envelope curves of both recordings plus difference statistics, sends them
  to the LLM, and applies the suggested pitch/formant/denoise configuration
  with an automatic re-process. (New `dsp::analyze_diff` and
  `ai::suggest_correction` modules.)
- **More robust backend detection** — both configured URLs are now probed
  against both protocols (`/api/tags` and `/v1/models`), so servers that
  answer with a `{"models":...}` list (Ollama-style) on either address are
  found, fixing auto-detection for non-standard setups.
- Unit tests for the waveform diff analysis and the AI correction
  response parsing.
- **LLM connectivity robustness** — probe results are cached (no repeated
  multi-second stalls), probes use connect timeouts, endpoint URLs are
  normalized, and the AI backend is launched automatically on first use when
  auto-launch is configured.
- **Voice source switching fixed** — "Translate to voice" now runs on a
  background thread, so the UI no longer freezes for up to a minute while a
  dead/remote endpoint times out (the app no longer feels stuck in AI mode);
  the voice panels show live AI connectivity status.
- Unit tests for the LLM config/env parsing, plus an offscreen GUI test
  suite (voice source switching, settings persistence).
- The Process and Live tabs scroll when the window is too small, so controls
  remain reachable when the AI panel expands.

## [0.1.0] — 2026-08-05

### Added

- Initial voice anonymization toolkit:
  - Batch pitch/formant shifting (VAD-gated), gender profiles and presets.
  - Hybrid mode: global gender shift + rules-engine/LLM phrase rewriting +
    TTS re-voicing.
  - Transcript extraction (whisper) and regex/heuristic PII redaction.
  - Live microphone masking with virtual mic routing (PipeWire pipe-source or
    snd-aloop).
  - Qt6 GUI and full-featured CLI with an interactive install wizard.
