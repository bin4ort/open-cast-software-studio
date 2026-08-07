// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <atomic>
#include <cstddef>
#include <string>
#include <vector>

#include "audio/ringbuf.h"
#include "common.h"

namespace ocs::audio {

struct DeviceInfo {
    int index = -1;
    std::string name;
    int max_inputs = 0;
    int max_outputs = 0;
    double default_sample_rate = 0.0;
};

// PortAudio global init/shutdown (reference counted not required; call once).
bool pa_init();
void pa_shutdown();
const char* pa_last_error();

std::vector<DeviceInfo> list_devices();
// Filtered device lists for the GUI: only real audio inputs (or outputs),
// excluding ALSA pseudo-devices (front, surround*, dmix, null, sysdefault,
// pipewire, pulse). The system default device is always kept.
std::vector<DeviceInfo> list_input_devices();
std::vector<DeviceInfo> list_output_devices();
int default_input_device();
int default_output_device();
int default_sample_rate();

// Full-duplex live stream. PortAudio's real-time callback copies microphone
// samples into `input_rb` and drains `output_rb` to the speakers (writing
// silence on underflow). A worker thread performs the DSP between the two
// ring buffers. This keeps all heavy processing off the audio thread.
struct LiveStream {
    void* stream = nullptr;
    int sample_rate = 48000;
    size_t frames_per_buffer = 1024;
    int input_device = -1;
    int output_device = -1;

    RingBuffer input_rb{1};
    RingBuffer output_rb{1};
    std::atomic<bool> running{false};

    LiveStream();
    LiveStream(const LiveStream&) = delete;
    LiveStream& operator=(const LiveStream&) = delete;
};

// Opens the full-duplex stream (does not start it).
bool open_live(LiveStream& ls, int input_dev, int output_dev,
               int sample_rate, size_t frames_per_buffer, std::string& err);
bool start_live(LiveStream& ls);
bool stop_live(LiveStream& ls);
void close_live(LiveStream& ls);

// Plays a mono buffer through `out_device` to completion (blocking; call on a
// worker thread). Sets *err on failure.
void play_buffer(const AudioData& data, int out_device, std::string* err = nullptr);

}  // namespace ocs::audio
