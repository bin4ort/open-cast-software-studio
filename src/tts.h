// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <string>

#include "common.h"
#include "dsp/gender.h"

namespace ocs::tts {

// True if a text-to-speech backend (espeak-ng) is available on PATH.
bool available();

// Synthesizes `text` with a voice matching `gender` into mono `out`.
// Returns false and sets `err` on failure (no backend, backend error, ...).
bool synthesize(const std::string& text, dsp::Gender gender,
                AudioData& out, std::string& err);

}  // namespace ocs::tts
