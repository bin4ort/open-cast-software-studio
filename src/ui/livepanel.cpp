// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "ui/livepanel.h"

#include <algorithm>
#include <chrono>
#include <memory>

#include "dsp/phase_vocoder.h"
#include "dsp/vad.h"

namespace ocs::ui {

LiveEngine::~LiveEngine() {
    stop();
}

bool LiveEngine::start(int input_dev, int output_dev, int sample_rate,
                       size_t frames_per_buffer, std::string& err) {
    if (running_.load()) return true;
    if (!audio::pa_init()) {
        err = "PortAudio init failed";
        return false;
    }
    if (!audio::open_live(stream_, input_dev, output_dev, sample_rate,
                          frames_per_buffer, err)) {
        return false;
    }
    stop_.store(false);
    running_.store(true);
    worker_ = std::thread(&LiveEngine::worker_loop, this);
    return audio::start_live(stream_);
}

void LiveEngine::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    stop_.store(true);
    audio::stop_live(stream_);
    if (worker_.joinable()) worker_.join();
    audio::close_live(stream_);
    audio::pa_shutdown();
}

void LiveEngine::set_pitch_semitones(double st) {
    std::lock_guard<std::mutex> lock(pitch_mu_);
    pitch_st_ = st;
}

void LiveEngine::consume_output(std::vector<float>& out) {
    std::lock_guard<std::mutex> lock(display_mu_);
    out.assign(display_.begin(), display_.end());
    display_.clear();
}

void LiveEngine::worker_loop() {
    const size_t frames = stream_.frames_per_buffer;
    std::vector<float> in_buf(frames), out_buf(frames);
    dsp::StreamVAD vad(stream_.sample_rate);
    std::unique_ptr<dsp::PitchShifter> shifter;

    while (!stop_.load()) {
        const size_t got = stream_.input_rb.read(in_buf.data(), frames);
        if (got == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        double pitch;
        {
            std::lock_guard<std::mutex> lock(pitch_mu_);
            pitch = pitch_st_;
        }
        if (!shifter || std::fabs(shifter->pitch_factor() -
                                  dsp::semitones_to_ratio(pitch)) > 1e-6) {
            shifter = std::make_unique<dsp::PitchShifter>(
                2048, 4, dsp::semitones_to_ratio(pitch));
        }

        const bool speech = vad.update(in_buf.data(), got);
        size_t made = 0;
        if (dry_.load()) {
            out_buf.assign(in_buf.begin(), in_buf.begin() + got);
            made = got;
        } else if (mute_.load() && !speech) {
            std::fill(out_buf.begin(), out_buf.begin() + got, 0.0f);
            made = got;
        } else {
            made = shifter->process(in_buf.data(), got, out_buf.data(), got);
        }

        stream_.output_rb.write(out_buf.data(), made);

        {
            std::lock_guard<std::mutex> lock(display_mu_);
            for (size_t i = 0; i < made; ++i) display_.push_back(out_buf[i]);
            while (display_.size() > 48000 * 4) display_.pop_front();
        }
    }
}

}  // namespace ocs::ui
