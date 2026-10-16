# OCS Studio — Transcribe Module Standard

<!-- SPDX-License-Identifier: GPL-3.0-only -->

## Purpose

Speech-to-text transcription via external whisper.cpp binary.

## Module Root

`src/transcribe/`

## Source Files

- `whisper.h` / `whisper.cpp` — Binary discovery, model loading, transcription

## Dependencies

- External whisper.cpp binary (whisper, whisper-cli, whisper.cpp)
- `common.h` (Segment struct)

## Public API

| Function | Namespace | Purpose |
|----------|-----------|---------|
| `find_whisper_binary()` | `ocs::transcribe` | Search PATH for whisper binary |
| `find_whisper_model()` | `ocs::transcribe` | Search model directories |
| `transcribe(input, cfg, segments, err)` | `ocs::transcribe` | Run transcription |

## Model Discovery Order

1. `$OCS_WHISPER_MODEL` env var
2. `~/.local/share/ocs_studio/models/*.bin`
3. `~/.config/ocs_studio/models/*.bin`

## Test File

`src/transcribe/test/test.sh`

## License

GPL-3.0-only
