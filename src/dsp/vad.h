// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 voice_anonymizer contributors
#pragma once

#include <cstddef>
#include <utility>
#include <vector>

namespace va::dsp {

struct VADParams {
    double threshold = 0.015;   // absolute RMS gate (0..1)
    double noise_margin = 2.5;  // RMS must exceed noise_floor * margin
    double hangover_s = 0.25;   // keep speech alive after last voiced frame
    double min_speech_s = 0.10; // drop segments shorter than this
    int frame_ms = 30;
};

// Offline VAD: returns voiced regions as [start, end) sample offsets.
std::vector<std::pair<size_t, size_t>> detect_speech(
    const std::vector<double>& samples, int sample_rate,
    const VADParams& params = {});

// Streaming VAD for the live path. Feed fixed-size blocks; poll `active()`.
class StreamVAD {
public:
    explicit StreamVAD(int sample_rate, VADParams params = {});

    // Feeds a block and updates the internal frame energy accumulator.
    // Returns true if this block ends inside an active speech region.
    bool update(const float* samples, size_t n);

    bool active() const { return active_; }

private:
    void roll_frame();
    int sample_rate_;
    VADParams params_;
    size_t frame_size_;
    std::vector<double> frame_acc_;  // squared samples, one per channel slot
    size_t frame_fill_ = 0;
    double noise_floor_ = 1e-4;
    bool active_ = false;
    size_t hangover_frames_ = 0;
    size_t hangover_count_ = 0;
};

}  // namespace va::dsp
