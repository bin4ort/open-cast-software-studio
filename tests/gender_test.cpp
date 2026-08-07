// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "test_util.h"

#include "dsp/gender.h"

using ocs::dsp::Gender;
using ocs::dsp::profile_for;

TEST(gender_male_lowers) {
    const auto p = profile_for(Gender::Male);
    CHECK(p.pitch_semitones < 0.0);
    CHECK(p.formant_semitones < 0.0);
}

TEST(gender_female_raises) {
    const auto p = profile_for(Gender::Female);
    CHECK(p.pitch_semitones > 0.0);
    CHECK(p.formant_semitones > 0.0);
}

TEST(gender_custom_neutral) {
    const auto p = profile_for(Gender::Custom);
    CHECK_NEAR(p.pitch_semitones, 0.0, 1e-9);
    CHECK_NEAR(p.formant_semitones, 0.0, 1e-9);
}

TEST(gender_from_name) {
    CHECK(ocs::dsp::gender_from_name("male") == Gender::Male);
    CHECK(ocs::dsp::gender_from_name("Female") == Gender::Female);
    CHECK(ocs::dsp::gender_from_name("custom") == Gender::Custom);
    CHECK(ocs::dsp::gender_from_name("whatever") == Gender::Generic);
}
