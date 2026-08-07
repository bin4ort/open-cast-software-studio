// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <string>
#include <vector>

#include "common.h"

namespace ocs::redact {

struct RedactOptions {
    bool redact_emails = true;
    bool redact_phones = true;
    bool redact_urls = true;
    bool redact_names = true;
    std::string replacement = "[REDACTED]";
    bool use_synthetic_names = false;  // swap names for plausible stand-ins
};

// Deterministic synthetic name for a given original (used when
// use_synthetic_names is set).
std::string synthetic_name(const std::string& original);

// Anonymizes free text. Returns the redacted text and collects the personal
// names that were detected.
std::string redact_text(const std::string& text, const RedactOptions& opts,
                        std::vector<std::string>& names_found);

// Flags transcript segments that contain PII (sets has_pii) and rewrites
// seg.text to the redacted form. Returns the total number of flagged segments.
size_t flag_segments(std::vector<Segment>& segments, const RedactOptions& opts);

}  // namespace ocs::redact
