# OCS Studio — Audio Module Standard

<!-- SPDX-License-Identifier: GPL-3.0-only -->

## Purpose

Audio I/O, device enumeration, and real-time streaming via PortAudio.

## Module Root

`src/audio/`

## Source Files

- `device.h` / `device.cpp` — PortAudio device management, live stream
- `fileio.h` / `fileio.cpp` — File read/write (libsndfile + ffmpeg fallback)
- `ringbuf.h` / `ringbuf.cpp` — Lock-free SPSC ring buffer

## Dependencies

- PortAudio (audio I/O)
- libsndfile (WAV/FLAC/OGG)
- ffmpeg (optional fallback)
- `common.h` (AudioData)

## Public API

| Function | Namespace | Purpose |
|----------|-----------|---------|
| `pa_init()` / `pa_shutdown()` | `ocs::audio` | PortAudio lifecycle |
| `list_devices()` / `list_input_devices()` / `list_output_devices()` | `ocs::audio` | Device enumeration |
| `open_live()` / `close_live()` | `ocs::audio` | Full-duplex stream |
| `read_file()` / `write_file()` | `ocs::audio` | File I/O |
| `RingBuffer::write()` / `read()` | `ocs::audio` | Ring buffer I/O |

## Test File

`src/audio/test/test.sh`

## License

GPL-3.0-only
