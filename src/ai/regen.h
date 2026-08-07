// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "dsp/gender.h"
#include "dsp/pipeline.h"

namespace ocs::ai {

// Transcribes captured utterances and re-synthesizes them in a target voice
// (TTS + the configured DSP shift), on a background thread. This is the
// "AI regeneration" live mode: your speech is turned into text, then spoken
// back by the target voice instead of merely being pitch-shifted.
class Regenerator {
public:
    Regenerator() = default;
    ~Regenerator();
    Regenerator(const Regenerator&) = delete;
    Regenerator& operator=(const Regenerator&) = delete;

    // Configures the whisper model, target voice and output sample rate.
    // Must be called before submit(). Safe to call anytime.
    void configure(const std::string& model, dsp::Gender gender,
                   const dsp::ShiftParams& shift, int sample_rate);

    // True once configured with a usable model and whisper binary.
    bool ready() const;

    // Queues an utterance (mono, at the configured sample rate).
    void submit(const std::vector<float>& samples);

    // Drains up to `max` regenerated output samples. Non-blocking.
    size_t poll(std::vector<float>& out, size_t max);

    void stop();

private:
    void worker();

    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::deque<std::vector<float>> pending_;
    std::deque<float> output_;
    bool stop_ = false;
    bool configured_ = false;
    std::string model_;
    dsp::Gender gender_ = dsp::Gender::Generic;
    dsp::ShiftParams shift_;
    int sample_rate_ = 48000;
    std::thread thread_;
};

}  // namespace ocs::ai
