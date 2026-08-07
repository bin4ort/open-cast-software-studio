// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <string>
#include <vector>

#include "common.h"

namespace ocs::dsp {

// Net shift expressed in semitones relative to the original recording.
// `formant_semitones` controls where the formants land independently of
// pitch: 0 = formants preserved (natural voice, new pitch),
// == pitch_semitones = everything shifts together (strong disguise).
struct ShiftParams {
    double pitch_semitones = 2.0;
    double formant_semitones = 2.0;
    bool enable_denoise = false;
};

enum class AnonMode {
    Global,     // VAD-gated: shift all speech, leave non-speech untouched
    Selective,  // shift only segments flagged as containing PII
    Hybrid,     // global gender shift + TTS re-voice rewritten behavior segments
};

struct Preset {
    std::string name;
    std::string description;
    ShiftParams params;
};

const std::vector<Preset>& presets();
// Returns nullptr if not found.
const Preset* preset_named(const std::string& name);

// Applies the pitch + formant shift to the entire buffer (no VAD gating).
// Used for TTS previews where the whole signal is speech.
std::vector<double> shift_audio(const std::vector<double>& input,
                                int sample_rate, const ShiftParams& params);

// VAD-gated global shift: every detected speech region is pitch/formant
// shifted and crossfade-spliced back into the (denoised) original.
std::vector<double> process_global(const std::vector<double>& input,
                                   int sample_rate, const ShiftParams& params);

// Selective re-voicing: only `segments` with has_pii == true are shifted.
std::vector<double> process_selective(
    const std::vector<double>& input, int sample_rate,
    const std::vector<Segment>& segments, const ShiftParams& params);

}  // namespace ocs::dsp
