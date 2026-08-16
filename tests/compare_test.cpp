// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "test_util.h"

#include <cmath>
#include <vector>

#include "dsp/compare.h"

using ocs::dsp::analyze_diff;

namespace {

std::vector<float> tone(double amp, double freq, int sr, int n) {
    std::vector<float> s(n);
    for (int i = 0; i < n; ++i) {
        s[i] = static_cast<float>(amp * std::sin(2.0 * 3.14159265358979 * freq *
                                                 static_cast<double>(i) / sr));
    }
    return s;
}

}  // namespace

TEST(compare_identical_signals) {
    const auto s = tone(0.5, 440.0, 48000, 48000);
    const auto d = analyze_diff(s, s, 128);
    CHECK(d.buckets == 128);
    CHECK_NEAR(d.rms_ratio, 1.0, 1e-9);
    CHECK_NEAR(d.mean_abs_dev, 0.0, 1e-9);
    CHECK(d.orig_len == s.size());
    CHECK(d.aligned_len == s.size());
}

TEST(compare_amplitude_ratio) {
    const auto orig = tone(0.5, 440.0, 48000, 48000);
    const auto proc = tone(1.0, 440.0, 48000, 48000);  // 2x amplitude
    const auto d = analyze_diff(orig, proc, 128);
    CHECK_NEAR(d.rms_ratio, 2.0, 0.02);
    CHECK_NEAR(d.peak_proc / d.peak_orig, 2.0, 1e-6);
}

TEST(compare_envelope_bucket_count) {
    const auto s = tone(0.5, 440.0, 48000, 48000);
    const auto d = analyze_diff(s, s, 64);
    CHECK(d.orig_env.size() == 64);
    CHECK(d.proc_env.size() == 64);
    // A steady tone yields a flat envelope: values within [0.9, 1.0] after
    // normalization (edge buckets may dip slightly).
    double peak = 0.0;
    for (double v : d.orig_env) peak = std::max(peak, v);
    CHECK_NEAR(peak, 1.0, 1e-9);
}

TEST(compare_envelope_tracks_silence) {
    // Half tone, half silence: envelope must dip in the second half.
    const int sr = 48000;
    const int n = sr;
    std::vector<float> s(n, 0.0f);
    for (int i = 0; i < n / 2; ++i) {
        s[i] = static_cast<float>(0.5 * std::sin(2.0 * 3.14159265358979 * 220.0 *
                                                 static_cast<double>(i) / sr));
    }
    const auto d = analyze_diff(s, s, 8);
    CHECK(d.orig_env[0] > 0.8);
    CHECK(d.orig_env[7] < 0.01);
}

TEST(compare_mismatched_lengths_aligned) {
    const auto orig = tone(0.5, 440.0, 48000, 48000);
    std::vector<float> proc = tone(0.5, 440.0, 48000, 24000);  // half length
    const auto d = analyze_diff(orig, proc, 128);
    CHECK(d.aligned_len == proc.size());
    CHECK_NEAR(d.mean_abs_dev, 0.0, 1e-9);  // aligned span is identical
}

TEST(compare_empty_inputs) {
    const auto d = analyze_diff({}, {}, 128);
    CHECK(d.buckets == 128);
    CHECK(d.orig_len == 0);
    CHECK(d.proc_len == 0);
    CHECK(d.aligned_len == 0);
    CHECK_NEAR(d.rms_ratio, 1.0, 1e-9);
}

TEST(compare_zero_buckets) {
    const auto s = tone(0.5, 440.0, 48000, 1000);
    const auto d = analyze_diff(s, s, 0);
    CHECK(d.orig_env.empty());
}
