// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <string>

#include "dsp/gender.h"

namespace ocs::rewrite {

// Rewrites gendered self-referential phrases in `text` so the speaker reads as
// `target` (Male/Female). Generic/Custom leave the text unchanged.
// Returns the rewritten text and sets `changed` if anything was rewritten.
std::string rewrite_for_gender(const std::string& text, dsp::Gender target,
                               bool& changed);

}  // namespace ocs::rewrite
