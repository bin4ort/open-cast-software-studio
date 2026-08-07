// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <cstddef>
#include <vector>

namespace ocs::dsp {

struct DenoiseParams {
    double noise_floor_sec = 0.5;  // leading audio used to estimate noise
    double over_subtract = 1.5;    // spectral subtraction strength
    double spectral_floor = 1e-4;  // magnitude floor relative to noise
    double gate_db = -30.0;        // below this frame RMS, attenuate (dB)
};

// Simple spectral-subtraction denoiser. Pass params = {} to disable.
std::vector<double> denoise(const std::vector<double>& input,
                            const DenoiseParams& params = {});

}  // namespace ocs::dsp
