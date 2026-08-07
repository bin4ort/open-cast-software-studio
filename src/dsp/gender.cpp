// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "dsp/gender.h"

namespace ocs::dsp {

const char* gender_name(Gender g) {
    switch (g) {
        case Gender::Generic: return "Generic";
        case Gender::Male: return "Male";
        case Gender::Female: return "Female";
        case Gender::Custom: return "Custom";
    }
    return "Generic";
}

Gender gender_from_name(const char* name) {
    if (name && (name[0] == 'M' || name[0] == 'm')) return Gender::Male;
    if (name && (name[0] == 'F' || name[0] == 'f')) return Gender::Female;
    if (name && (name[0] == 'C' || name[0] == 'c')) return Gender::Custom;
    return Gender::Generic;
}

GenderProfile profile_for(Gender g) {
    switch (g) {
        case Gender::Male:
            return {-5.0, -4.0};  // lower pitch and formants
        case Gender::Female:
            return {5.0, 4.0};  // higher pitch and formants
        case Gender::Custom:
            return {0.0, 0.0};
        case Gender::Generic:
        default:
            return {2.0, 0.0};  // mild disguise, formants preserved
    }
}

}  // namespace ocs::dsp
