# OCS Studio — Transcribe Module

<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 Open Cast Software Studio contributors -->

## Overview

The `transcribe` module provides speech-to-text transcription using external whisper.cpp binaries. It handles binary discovery, model loading, and segment extraction for downstream processing (redaction, hybrid mode, selective re-voicing).

## Components

| File | Description |
|------|-------------|
| `whisper.h/cpp` | Whisper binary discovery and transcription execution |

## API

```cpp
namespace ocs::transcribe {

struct WhisperConfig {
    std::string binary;    // whisper-cli / whisper / full path
    std::string model;     // optional (whisper.cpp -m)
    std::string language;  // optional (e.g. "en")
};

std::string find_whisper_binary();  // Search PATH
std::string find_whisper_model();   // Search model dirs
bool transcribe(const std::string& input_path, const WhisperConfig& cfg,
                std::vector<Segment>& segments, std::string& err);

}  // namespace ocs::transcribe
```

## Model Discovery

Models are found in order:
1. `$OCS_WHISPER_MODEL` environment variable
2. `~/.local/share/ocs_studio/models/*.bin`
3. `~/.config/ocs_studio/models/*.bin`

## Binary Discovery

Searches PATH for: `whisper`, `whisper-cli`, `whisper.cpp`

## Error Handling

- Returns `false` and sets `err` on:
  - No whisper binary found
  - No model found
  - Decode failure
  - No output segments
- Output path uses deterministic basename + shell quoting for safe file paths

## Testing

```bash
bash src/transcribe/test/test.sh
```

## Dependencies

- External whisper.cpp binary
- `common.h` — Segment struct definition

## License

GPL-3.0-only. See [../../LICENSE](../../LICENSE) for details.
