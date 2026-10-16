# OCS Studio — AI Module Standard

<!-- SPDX-License-Identifier: GPL-3.0-only -->

## Purpose

Provide AI-assisted voice transformation features via local LLM backends (Ollama, llama.cpp).

## Module Root

`src/ai/`

## Source Files

- `correction.h` / `correction.cpp` — Waveform-based correction suggestions
- `llm.h` / `llm.cpp` — LLM backend integration
- `regen.h` / `regen.cpp` — Live regeneration engine

## Dependencies

- `dsp::` (AudioDiff, ShiftParams, Gender)
- nlohmann/json (JSON parsing)
- Ollama or llama.cpp server (runtime)

## Public API

| Function | Namespace | Purpose |
|----------|-----------|---------|
| `llm_available()` | `ocs::llm` | Check backend reachability |
| `launch_backend()` | `ocs::llm` | Auto-launch configured backend |
| `complete(system, user)` | `ocs::llm` | Send prompt, get completion |
| `translate_prompt(desc, out)` | `ocs::llm` | Map text → DSP settings |
| `rewrite_phrase(text, target)` | `ocs::llm` | Rewrite gendered terms |
| `suggest_correction(diff, current, target, corrected, reason)` | `ocs::ai` | AI correction suggestion |
| `RegenEngine::submit()` | `ocs::ai` | Submit audio for live regeneration |
| `RegenEngine::stop()` | `ocs::ai` | Stop regeneration (resets stop_) |

## Test File

`src/ai/test/test.sh`

## License

GPL-3.0-only
