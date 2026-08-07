// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ocs::audio {

enum class VirtualMicType { None, PipeSource, AlsaLoopback };

// A virtual microphone that other apps (Discord, streaming, recording) can
// select as an audio input. The app writes processed audio to it. Because the
// device only produces audio while this app writes to it, closing or crashing
// the app makes the virtual mic go silent - like the mic died, never
// deanonymizing the user.
//
// Auto-probed in this order:
//   1. PipeSource  - PipeWire/PulseAudio module-pipe-source (user-space FIFO)
//   2. AlsaLoopback- snd-aloop kernel loopback (PortAudio output device)
//   3. None        - unavailable on this system
class VirtualMic {
public:
    VirtualMic() = default;
    ~VirtualMic();
    VirtualMic(const VirtualMic&) = delete;
    VirtualMic& operator=(const VirtualMic&) = delete;

    // Probes and prepares the device at `sample_rate`. Returns false + err if
    // no virtual mic mechanism is available.
    bool create(int sample_rate, std::string& err);
    void destroy();
    bool active() const { return active_; }
    VirtualMicType type() const { return type_; }
    const std::string& name() const { return display_name_; }

    // Writes mono float samples at the configured sample rate. Drops samples
    // if the FIFO is momentarily full. Safe to call from the DSP thread.
    void write(const float* samples, size_t n);

private:
    bool create_pipe_source(int sample_rate, std::string& err);
    bool create_alsa_loopback(int sample_rate, std::string& err);

    VirtualMicType type_ = VirtualMicType::None;
    bool active_ = false;
    std::string display_name_;

    // PipeSource state.
    std::string fifo_path_;
    int fifo_fd_ = -1;
    std::string module_id_;
    std::vector<int16_t> s16buf_;

    // AlsaLoopback state.
    void* loop_stream_ = nullptr;
    int loop_device_ = -1;
    std::vector<float> floatbuf_;
};

// Static probe without creating anything.
VirtualMicType probe_virtual_mic(std::string& display_name);

}  // namespace ocs::audio
