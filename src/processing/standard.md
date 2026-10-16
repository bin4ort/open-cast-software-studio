# OCS Studio — Processing Module Standard

<!-- SPDX-License-Identifier: GPL-3.0-only -->

## Purpose

Batch job orchestration: manage job state, progress tracking, and sequential execution of processing pipelines.

## Module Root

`src/processing/`

## Source Files

- `job.h` / `job.cpp` — Job definition, state machine, progress reporting

## Dependencies

- `dsp::` — Voice transformation
- `transcribe::` — Whisper.cpp transcription (optional)
- `redact::` — PII redaction (optional)
- `llm::` — AI rewriting (optional)
- `vocoder::` — TTS re-voicing (optional)
- `audio::` — File I/O

## Public API

| Function | Namespace | Purpose |
|----------|-----------|---------|
| `run(opts)` | `ocs::job` | Execute batch job pipeline |

## Job Pipeline Stages

1. Decode input audio (`audio::read_file`)
2. Transcribe (`transcribe::transcribe`, optional)
3. Redact (`redact::redact_text`, optional)
4. Shift voice (`dsp::shift_voice`)
5. Re-voice (`vocoder`, optional)
6. Write output (`audio::write_file`)

## Test File

`src/processing/test/test.sh`

## License

GPL-3.0-only
