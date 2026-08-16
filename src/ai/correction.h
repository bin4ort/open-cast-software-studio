// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <string>

#include "dsp/compare.h"
#include "dsp/gender.h"
#include "dsp/pipeline.h"

namespace ocs::ai {

// Asks the local LLM to correct the voice-transformation configuration based
// on the waveform difference between the original recording and the processed
// output (the envelope curves are fed in as the "graph values"). Returns
// false when no backend responds; on success fills `corrected` (clamped to
// the supported range) and a short human-readable `reason`.
bool suggest_correction(const dsp::AudioDiff& diff,
                        const dsp::ShiftParams& current,
                        dsp::Gender target,
                        dsp::ShiftParams& corrected,
                        std::string& reason);

// Builds the system/user prompt that is sent to the LLM (kept separate so it
// can be unit tested without a running backend).
std::string correction_prompt(const dsp::AudioDiff& diff,
                              const dsp::ShiftParams& current,
                              dsp::Gender target);

// Parses the JSON reply into a corrected config. Falls back to `current`
// (and returns false) for malformed or out-of-range replies; reason is
// optional prose from the model.
bool parse_correction_response(const std::string& json,
                               const dsp::ShiftParams& current,
                               dsp::ShiftParams& corrected,
                               std::string& reason);

}  // namespace ocs::ai
