// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "audio/device.h"
#include "audio/virtualmic.h"

namespace ocs::ui {

// Where the processed audio goes.
enum class OutputMode { Speaker, VirtualMic, Both };

// Encapsulates the real-time masking stream used by both the CLI and the GUI:
// PortAudio duplex stream + a DSP worker thread (VAD -> pitch shift) shuttling
// audio through the stream's ring buffers, optionally to a virtual mic.
class LiveEngine {
public:
    LiveEngine() = default;
    ~LiveEngine();

    LiveEngine(const LiveEngine&) = delete;
    LiveEngine& operator=(const LiveEngine&) = delete;

    bool start(int input_dev, int output_dev, int sample_rate,
               size_t frames_per_buffer, OutputMode mode, std::string& err);
    void stop();
    bool running() const { return running_.load(); }

    void set_pitch_semitones(double st);
    void set_mute_silence(bool on) { mute_.store(on); }
    void set_dry(bool on) { dry_.store(on); }

    double pitch_semitones() const { return pitch_st_; }
    OutputMode output_mode() const { return mode_; }

    // Drains recent output samples for waveform display.
    void consume_output(std::vector<float>& out);

private:
    void worker_loop();

    audio::LiveStream stream_;
    std::unique_ptr<audio::VirtualMic> virtual_mic_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};
    OutputMode mode_ = OutputMode::Speaker;

    std::mutex pitch_mu_;
    double pitch_st_ = 2.0;
    std::atomic<bool> mute_{false};
    std::atomic<bool> dry_{false};

    std::mutex display_mu_;
    std::deque<float> display_;
};

}  // namespace ocs::ui

