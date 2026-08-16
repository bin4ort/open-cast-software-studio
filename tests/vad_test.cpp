// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "test_util.h"

#include <vector>

#include "dsp/vad.h"

using ocs::dsp::StreamVAD;
using ocs::dsp::detect_speech;

TEST(stream_vad_active_on_speech) {
    // Regression: frame_acc_ was never sized, so update() wrote out of
    // bounds. Continuous loud audio must be detected as active.
    const int sr = 48000;
    StreamVAD vad(sr);
    std::vector<float> block(1024, 0.2f);
    bool active = false;
    for (int i = 0; i < 50; ++i) active = vad.update(block.data(), block.size());
    CHECK(active);
}

TEST(stream_vad_inactive_on_silence) {
    const int sr = 48000;
    StreamVAD vad(sr);
    std::vector<float> block(1024, 0.0f);
    bool active = false;
    for (int i = 0; i < 50; ++i) active = vad.update(block.data(), block.size());
    CHECK(!active);
}

TEST(stream_vad_tracks_on_off) {
    const int sr = 48000;
    StreamVAD vad(sr);
    std::vector<float> loud(1024, 0.3f);
    std::vector<float> quiet(1024, 0.0f);
    for (int i = 0; i < 30; ++i) vad.update(loud.data(), loud.size());
    CHECK(vad.active());
    for (int i = 0; i < 200; ++i) vad.update(quiet.data(), quiet.size());
    CHECK(!vad.active());
}

TEST(stream_vad_bad_rate_no_crash) {
    // Invalid rates must not crash or hang (they fall back to 48 kHz).
    StreamVAD vad0(0);
    StreamVAD vad_neg(-48000);
    std::vector<float> quiet(256, 0.0f);
    for (int i = 0; i < 10; ++i) {
        vad0.update(quiet.data(), quiet.size());
        vad_neg.update(quiet.data(), quiet.size());
    }
    CHECK(!vad0.active());
    CHECK(!vad_neg.active());
}

TEST(stream_vad_zero_frame_ms_no_crash) {
    ocs::dsp::VADParams p;
    p.frame_ms = 0;  // degenerate: must be clamped internally
    StreamVAD vad(48000, p);
    std::vector<float> block(256, 0.2f);
    for (int i = 0; i < 10; ++i) vad.update(block.data(), block.size());
    CHECK(true);
}

TEST(offline_vad_finds_tone) {
    const int sr = 48000;
    const size_t n = 3 * sr;  // 3 s: 1 s silence, 1 s tone, 1 s silence
    std::vector<double> s(n, 0.0);
    for (size_t i = sr; i < 2 * sr; ++i) {
        s[i] = 0.5 * std::sin(2.0 * 3.14159265358979 * 440.0 *
                              static_cast<double>(i - sr) / sr);
    }
    const auto regions = detect_speech(s, sr);
    CHECK(!regions.empty());
    if (!regions.empty()) {
        CHECK_NEAR(static_cast<double>(regions[0].first) / sr, 1.0, 0.2);
        // The 0.25 s hangover extends the region past the tone's end.
        CHECK_NEAR(static_cast<double>(regions[0].second) / sr, 2.25, 0.2);
    }
}

TEST(offline_vad_silence_empty) {
    std::vector<double> s(48000, 0.0);
    CHECK(detect_speech(s, 48000).empty());
}

TEST(offline_vad_invalid_input_empty) {
    CHECK(detect_speech({}, 48000).empty());
    std::vector<double> s(100, 0.1);
    CHECK(detect_speech(s, 0).empty());
}
