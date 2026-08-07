// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ocs {

// Mono, float (-1.0..1.0) PCM sample.
using Sample = float;

// A fully-decoded mono audio buffer plus its metadata.
struct AudioData {
    std::vector<Sample> samples;
    int sample_rate = 0;
};

// A speech/text segment with a temporal span (seconds).
struct Segment {
    double start = 0.0;
    double end = 0.0;
    std::string text;
    bool has_pii = false;  // flagged by the redactor
};

}  // namespace ocs
