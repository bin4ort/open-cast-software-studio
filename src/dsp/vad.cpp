// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 voice_anonymizer contributors
#include "dsp/vad.h"

#include <algorithm>
#include <cmath>

namespace va::dsp {

namespace {

double frame_rms(const std::vector<double>& s, size_t start, size_t n) {
    double sum = 0.0;
    const size_t end = std::min(start + n, s.size());
    for (size_t i = start; i < end; ++i) sum += s[i] * s[i];
    const size_t cnt = end - start;
    if (cnt == 0) return 0.0;
    return std::sqrt(sum / static_cast<double>(cnt));
}

}  // namespace

std::vector<std::pair<size_t, size_t>> detect_speech(
    const std::vector<double>& samples, int sample_rate, const VADParams& params) {
    std::vector<std::pair<size_t, size_t>> result;
    if (samples.empty() || sample_rate <= 0) return result;

    const size_t frame_size =
        static_cast<size_t>(params.frame_ms) * static_cast<size_t>(sample_rate) / 1000;
    if (frame_size == 0) return result;

    std::vector<double> rms;
    for (size_t start = 0; start < samples.size(); start += frame_size) {
        rms.push_back(frame_rms(samples, start, frame_size));
    }
    if (rms.empty()) return result;

    // Noise floor: a low percentile of per-frame RMS. The absolute threshold
    // below already gates quiet recordings, so the adaptive floor only has to
    // separate speech from louder ambient noise; a very low percentile is
    // robust to speech occupying most of the file.
    std::vector<double> sorted = rms;
    std::sort(sorted.begin(), sorted.end());
    const size_t noise_idx = std::min<size_t>(sorted.size() / 50, sorted.size() - 1);
    const double noise_floor = sorted[noise_idx];
    const double gate =
        std::max(params.threshold, noise_floor * params.noise_margin);

    std::vector<bool> voiced(rms.size());
    for (size_t i = 0; i < rms.size(); ++i) voiced[i] = rms[i] >= gate;

    // Hangover.
    const size_t hangover_frames = static_cast<size_t>(
        std::lround(params.hangover_s * 1000.0 / params.frame_ms));
    size_t hangover = 0;
    std::vector<bool> speech(rms.size(), false);
    for (size_t i = 0; i < voiced.size(); ++i) {
        if (voiced[i]) {
            hangover = hangover_frames;
            speech[i] = true;
        } else if (hangover > 0) {
            speech[i] = true;
            --hangover;
        }
    }

    // Segment the speech mask.
    size_t i = 0;
    const size_t min_len = static_cast<size_t>(
        params.min_speech_s * static_cast<double>(sample_rate));
    while (i < speech.size()) {
        if (!speech[i]) { ++i; continue; }
        size_t j = i;
        while (j < speech.size() && speech[j]) ++j;
        const size_t start = i * frame_size;
        const size_t end = std::min(samples.size(), j * frame_size);
        if (end - start >= min_len) result.emplace_back(start, end);
        i = j;
    }
    return result;
}

StreamVAD::StreamVAD(int sample_rate, VADParams params)
    : sample_rate_(sample_rate), params_(params) {
    frame_size_ = static_cast<size_t>(params.frame_ms) * sample_rate / 1000;
    if (frame_size_ == 0) frame_size_ = 1;
    hangover_frames_ = static_cast<size_t>(
        std::lround(params.hangover_s * 1000.0 / params.frame_ms));
}

void StreamVAD::roll_frame() {
    const double rms = std::sqrt(frame_acc_[0] / static_cast<double>(frame_size_));
    frame_acc_[0] = 0.0;

    // Slow-adapting noise floor (rms < floor lowers it, else creeps up).
    if (rms < noise_floor_) {
        noise_floor_ = 0.9 * noise_floor_ + 0.1 * rms;
    } else {
        noise_floor_ += (0.0005 - noise_floor_) * 0.001;
    }

    const double gate = std::max(params_.threshold, noise_floor_ * params_.noise_margin);
    if (rms >= gate) {
        active_ = true;
        hangover_count_ = hangover_frames_;
    } else if (hangover_count_ > 0) {
        --hangover_count_;
        if (hangover_count_ == 0) active_ = false;
    } else {
        active_ = false;
    }
}

bool StreamVAD::update(const float* samples, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        const double s = static_cast<double>(samples[i]);
        frame_acc_[0] += s * s;
        if (++frame_fill_ >= frame_size_) {
            frame_fill_ = 0;
            roll_frame();
        }
    }
    return active_;
}

}  // namespace va::dsp
