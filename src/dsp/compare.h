// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <cstddef>
#include <vector>

namespace ocs::dsp {

// Comparison between an original recording and its processed version. The
// RMS envelope curves (normalized to 0..1, `buckets` points) are the "graph
// values" that the AI correction path feeds to the LLM.
struct AudioDiff {
    size_t buckets = 0;
    std::vector<double> orig_env;  // RMS envelope of the original
    std::vector<double> proc_env;  // RMS envelope of the processed audio
    double orig_rms = 0.0;         // global RMS of the original
    double proc_rms = 0.0;         // global RMS of the processed audio
    double rms_ratio = 1.0;        // proc_rms / orig_rms (1.0 = same level)
    double mean_abs_dev = 0.0;     // mean |orig - proc| over the aligned span
    double peak_orig = 0.0;        // absolute peak of the original
    double peak_proc = 0.0;        // absolute peak of the processed audio
    size_t orig_len = 0;           // sample counts used for the diff
    size_t proc_len = 0;
    size_t aligned_len = 0;        // min(orig_len, proc_len)
};

// Analyzes the difference between the original and processed mono audio.
// Both buffers may differ in length (hybrid mode splices TTS regions); the
// alignment always uses the shorter buffer. Returns a zeroed struct for
// empty input.
AudioDiff analyze_diff(const std::vector<float>& orig,
                       const std::vector<float>& proc, size_t buckets = 128);

}  // namespace ocs::dsp
