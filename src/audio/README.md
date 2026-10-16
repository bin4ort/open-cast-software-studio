# OCS Studio — Audio Module

<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 Open Cast Software Studio contributors -->

## Overview

The `audio` module handles audio I/O, device enumeration, and real-time streaming for OCS Studio. It wraps PortAudio for live audio and libsndfile/ffmpeg for file I/O.

## Components

| File | Description |
|------|-------------|
| `device.h/cpp` | PortAudio device enumeration, filtering, and live stream management |
| `fileio.h/cpp` | Audio file reading/writing with libsndfile native + ffmpeg fallback |
| `ringbuf.h/cpp` | Lock-free single-producer single-consumer ring buffer for float samples |

## Dependencies

- **PortAudio** — Cross-platform audio I/O
- **libsndfile** — WAV/FLAC/OGG native file I/O
- **ffmpeg** (optional) — Fallback for mp3, m4a, and other unsupported formats

## Ring Buffer

`RingBuffer` is a lock-free FIFO for real-time audio:

```cpp
namespace ocs::audio {
    class RingBuffer {
        void write(const float* data, size_t frames, size_t channels);
        void read(float* out, size_t frames, size_t channels);
        size_t capacity() const;
        size_t available() const;
    };
}
```

## Audio File I/O

```cpp
namespace ocs::audio {
    AudioData read_file(const std::string& path, std::string* err = nullptr);
    bool write_file(const std::string& path, const AudioData& data,
                    std::string* err = nullptr);
    bool sndfile_can_handle(const std::string& path);
    bool ffmpeg_available();
}
```

## Live Streaming

```cpp
namespace ocs::audio {
    struct LiveStream {
        void* stream = nullptr;
        int sample_rate = 48000;
        size_t frames_per_buffer = 1024;
        int input_device = -1;
        int output_device = -1;
        RingBuffer input_rb{1};
        RingBuffer output_rb{1};
        std::atomic<bool> running{false};
    };

    bool open_live(LiveStream& ls, int input_dev, int output_dev,
                   int sample_rate, size_t frames_per_buffer, std::string& err);
    void close_live(LiveStream& ls);
}
```

## Device Filtering

The module excludes ALSA pseudo-devices (front, surround*, dmix, null, sysdefault, pipewire, pulse) from GUI device lists. The system default device is always included.

## Testing

```bash
bash src/audio/test/test.sh
```

## License

GPL-3.0-only. See [../../LICENSE](../../LICENSE) for details.
