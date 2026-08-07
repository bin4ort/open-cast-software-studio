// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

namespace ocs::dsp {

// Target voice gender. Presets map to a pitch/formant profile; Custom leaves
// the sliders fully user-controlled.
enum class Gender { Generic, Male, Female, Custom };

struct GenderProfile {
    double pitch_semitones = 0.0;
    double formant_semitones = 0.0;
};

const char* gender_name(Gender g);
Gender gender_from_name(const char* name);

// Starting point profiles. Users can always fine-tune afterwards.
GenderProfile profile_for(Gender g);

}  // namespace ocs::dsp
