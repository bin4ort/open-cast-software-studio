# OCS Studio — UI Module Standard

<!-- SPDX-License-Identifier: GPL-3.0-only -->

## Purpose

Qt6 GUI application: main window, live masking, file processing, settings.

## Module Root

`src/ui/`

## Source Files

- `gui_entry.cpp` — CLI entry point dispatcher
- `gui_main.cpp` / `gui_main.h` — `main()` for GUI binary
- `livepanel.cpp` / `livepanel.h` — Live masking panel
- `filepanel.cpp` / `filepanel.h` — File processing panel
- `settingspanel.cpp` / `settingspanel.h` — Settings panel
- `waveformpanel.cpp` / `waveformpanel.h` — Waveform display
- `mainwindow.cpp` / `mainwindow.h` — Main window, tab management, threading

## Dependencies

- Qt6 (Core, Gui, Widgets, Multimedia, Network)
- `dsp::` — Voice transformation params
- `llm::` — AI backend connectivity
- `audio::` — Device enumeration
- `job::` — Batch job execution
- `LiveEngine` — Real-time audio processing

## Public API (Internal)

| Class | Namespace | Purpose |
|-------|-----------|---------|
| `MainWindow` | `ocs::ui` | Main window (QMainWindow) |
| `LivePanel` | `ocs::ui` | Live masking controls |
| `FilePanel` | `ocs::ui` | File selection + params |
| `SettingsPanel` | `ocs::ui` | AI/audio settings |
| `Waveform` | `ocs::ui` | Waveform display widget |

## Test File

`src/ui/test/test.sh`

## License

GPL-3.0-only
