# OCS Studio — UI Module

<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 Open Cast Software Studio contributors -->

## Overview

The `ui` module provides the Qt6-based GUI and CLI dispatcher for OCS Studio. It manages the main window with multiple tabs, live masking controls, file processing panels, and settings management.

## Components

| File | Description |
|------|-------------|
| `gui_entry.cpp` | CLI entry point — dispatches subcommands to the core library |
| `gui_main.cpp/h` | `main()` for the GUI binary, QApplication setup |
| `livepanel.cpp/h` | Live voice masking panel (start/stop, device selector) |
| `filepanel.cpp/h` | File processing panel (input/output, voice params) |
| `settingspanel.cpp/h` | Settings panel (AI backend, audio devices, preferences) |
| `waveformpanel.cpp/h` | Waveform display widget with compare overlay |
| `mainwindow.cpp/h` | Main window — QTabWidget layout, thread management |

## GUI Architecture

```
ocs_studio_gui
├── MainWindow (QMainWindow)
│   ├── Process Tab
│   │   ├── FilePanel (input/output, voice params)
│   │   └── WaveformPanel (compare overlay)
│   ├── Live Tab
│   │   ├── LivePanel (start/stop, device selector)
│   │   └── VoicePanel (pitch/formant/gender)
│   ├── Transcript Tab
│   │   └── WhisperControls (model, language, output format)
│   └── Settings Tab
│       ├── AICfg (Ollama/llama.cpp URLs, auto-launch)
│       ├── AudioCfg (input/output devices, sample rate)
│       └── Preferences (theme, language, hotkeys)
```

## Thread Safety

- All heavy processing runs in worker threads (joinable, tracked)
- Widget values captured before thread start (no data races)
- Worker threads properly joined in `MainWindow` destructor
- No crashes on repeat processing or window close

## Key Features

- **Settings persistence** — AI backend URLs, auto-launch, working directory saved to config
- **Backend robustness** — Dual protocol probing, endpoint normalization, timeout handling
- **AI correction** — `dsp::analyze_diff` + `ai::suggest_correction` for waveform-based settings recommendation
- **Waveform compare** — Overlays original + processed waveforms
- **Background voice source switching** — No UI freeze during backend probe
- **Connectivity caching** — LLM probe results cached to prevent repeated stalls

## Testing

```bash
bash src/ui/test/test.sh
```

## Dependencies

- Qt6 (Core, Gui, Widgets, Multimedia, Network)
- `dsp::` — Voice transformation parameters
- `llm::` — AI backend connectivity
- `audio::` — Device enumeration
- `job::` — Batch job execution
- `LiveEngine` — Real-time audio processing

## License

GPL-3.0-only. See [../../LICENSE](../../LICENSE) for details.
