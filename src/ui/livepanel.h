// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 voice_anonymizer contributors
#pragma once

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "audio/device.h"

namespace va::ui {

// Encapsulates the real-time masking stream used by both the CLI and the GUI:
// PortAudio duplex stream + a DSP worker thread (VAD -> pitch shift) shuttling
// audio through the stream's ring buffers.
class LiveEngine {
public:
    LiveEngine() = default;
    ~LiveEngine();

    LiveEngine(const LiveEngine&) = delete;
    LiveEngine& operator=(const LiveEngine&) = delete;

    bool start(int input_dev, int output_dev, int sample_rate,
               size_t frames_per_buffer, std::string& err);
    void stop();
    bool running() const { return running_.load(); }

    void set_pitch_semitones(double st);
    void set_mute_silence(bool on) { mute_.store(on); }
    void set_dry(bool on) { dry_.store(on); }

    double pitch_semitones() const { return pitch_st_; }

    // Drains recent output samples for waveform display.
    void consume_output(std::vector<float>& out);

private:
    void worker_loop();

    audio::LiveStream stream_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};

    std::mutex pitch_mu_;
    double pitch_st_ = 2.0;
    std::atomic<bool> mute_{false};
    std::atomic<bool> dry_{false};

    std::mutex display_mu_;
    std::deque<float> display_;
};

}  // namespace va::ui
