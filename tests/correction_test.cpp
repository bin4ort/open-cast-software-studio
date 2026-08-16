// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "test_util.h"

#include <string>

#include "ai/correction.h"
#include "dsp/compare.h"

using ocs::dsp::Gender;
using ocs::dsp::ShiftParams;

namespace {

// A plain diff without any network access.
ocs::dsp::AudioDiff sample_diff() {
    ocs::dsp::AudioDiff d;
    d.buckets = 4;
    d.orig_env = {1.0, 0.9, 0.5, 0.2};
    d.proc_env = {0.8, 0.7, 0.4, 0.3};
    d.orig_rms = 0.5;
    d.proc_rms = 0.4;
    d.rms_ratio = 0.8;
    d.mean_abs_dev = 0.05;
    d.peak_orig = 0.9;
    d.peak_proc = 0.7;
    d.orig_len = d.proc_len = d.aligned_len = 48000;
    return d;
}

}  // namespace

TEST(correction_parse_valid) {
    ShiftParams current{2.0, 0.0, false};
    ShiftParams out;
    std::string reason;
    const bool ok = ocs::ai::parse_correction_response(
        R"({"pitch": -5.5, "formant": -4.0, "denoise": true, "reason": "too bright"})",
        current, out, reason);
    CHECK(ok);
    CHECK_NEAR(out.pitch_semitones, -5.5, 1e-9);
    CHECK_NEAR(out.formant_semitones, -4.0, 1e-9);
    CHECK(out.enable_denoise == true);
    CHECK(reason == "too bright");
}

TEST(correction_parse_missing_keys_falls_back) {
    ShiftParams current{3.0, 1.0, false};
    ShiftParams out{99.0, 99.0, true};
    std::string reason;
    const bool ok = ocs::ai::parse_correction_response(
        R"({"pitch": 1.0})", current, out, reason);
    CHECK(!ok);
    CHECK_NEAR(out.pitch_semitones, 3.0, 1e-9);   // unchanged
    CHECK_NEAR(out.formant_semitones, 1.0, 1e-9);
    CHECK(out.enable_denoise == false);
}

TEST(correction_parse_malformed_falls_back) {
    ShiftParams current{3.0, 1.0, true};
    ShiftParams out;
    std::string reason;
    CHECK(!ocs::ai::parse_correction_response("not json at all", current, out, reason));
    CHECK_NEAR(out.pitch_semitones, 3.0, 1e-9);
    CHECK(out.enable_denoise == true);
    CHECK(!ocs::ai::parse_correction_response("", current, out, reason));
}

TEST(correction_parse_clamps_range) {
    ShiftParams current{2.0, 0.0, false};
    ShiftParams out;
    std::string reason;
    const bool ok = ocs::ai::parse_correction_response(
        R"({"pitch": 999, "formant": -999, "denoise": false})", current, out,
        reason);
    CHECK(ok);
    CHECK_NEAR(out.pitch_semitones, 24.0, 1e-9);
    CHECK_NEAR(out.formant_semitones, -24.0, 1e-9);
}

TEST(correction_prompt_contains_values) {
    const std::string p = ocs::ai::correction_prompt(sample_diff(),
                                                     ShiftParams{2.0, 0.0, false},
                                                     Gender::Female);
    // Envelopes are quantized to 0..100 integers.
    CHECK(p.find("100 90 50 20") != std::string::npos);  // original envelope
    CHECK(p.find("80 70 40 30") != std::string::npos);   // processed envelope
    CHECK(p.find("Female") != std::string::npos);        // target voice
    CHECK(p.find("mean absolute deviation") != std::string::npos);
    CHECK(p.find("\"pitch\"") != std::string::npos);     // JSON shape hint
}
