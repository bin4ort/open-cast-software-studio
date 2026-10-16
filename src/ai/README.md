# OCS Studio — AI Module

<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 Open Cast Software Studio contributors -->

## Overview

The `ai` module provides AI-assisted voice transformation features for OCS Studio. It integrates with local LLM backends (Ollama, llama.cpp) to enable waveform-based correction, gendered text rewriting, and prompt-to-voice translation.

## Components

| File | Description |
|------|-------------|
| `correction.h/cpp` | AI-driven voice correction — analyzes waveform differences and suggests improved DSP settings |
| `llm.h/cpp` | LLM backend integration — Ollama and llama.cpp connectivity, completion, translation, rewriting |
| `regen.h/cpp` | Live regeneration engine — handles stop/restart cycles for real-time voice processing |

## Dependencies

- Ollama (`http://localhost:11434`) or llama.cpp server (`http://localhost:8080`)
- `json.hpp` (nlohmann) for JSON parsing
- `dsp::AudioDiff`, `dsp::ShiftParams`, `dsp::Gender` from the `dsp` module

## API Summary

```cpp
// LLM connectivity
namespace ocs::llm {
    bool llm_available();           // Check if backend is reachable
    bool launch_backend();          // Auto-launch configured backend
    std::string backend_name();     // "ollama" or "llama.cpp"
    std::string complete(sys, usr); // Send prompt, get completion
    bool translate_prompt(desc, out); // Map text → DSP settings
    std::string rewrite_phrase(txt, target); // Rewrite gendered terms
}

// AI correction
namespace ocs::ai {
    bool suggest_correction(diff, current, target, corrected, reason);
    std::string correction_prompt(diff, current, target);
    bool parse_correction_response(json, current, corrected, reason);
}

// Regeneration
namespace ocs::ai {
    class RegenEngine { /* live processing loop */ };
}
```

## Configuration

Set environment variables before calling any LLM functions:

| Variable | Default | Description |
|----------|---------|-------------|
| `OCS_OLLAMA_URL` | `http://localhost:11434` | Ollama API endpoint |
| `OCS_LLAMA_URL` | `http://localhost:8080` | llama.cpp server endpoint |
| `OCS_AI_LAUNCH_CMD` | — | Backend launch command |
| `OCS_AI_LAUNCH_DIR` | — | Working directory for launcher |
| `OCS_AI_AUTO_LAUNCH` | `false` | Auto-start backend on first use |

## Testing

Run the module test script:
```bash
bash src/ai/test/test.sh
```

## Error Handling

- `llm_available()` returns `false` when no backend is reachable
- `complete()` returns empty string on failure
- `suggest_correction()` returns `false` and sets `reason` on failure
- Backend launch failures are logged to `/tmp/ocs_ai_launch.log`

## License

GPL-3.0-only. See [../../LICENSE](../../LICENSE) for details.
