// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 voice_anonymizer contributors
#pragma once

#include <cstddef>
#include <vector>

namespace va::dsp {

// Shifts the spectral envelope (formants) by `factor` while leaving the
// harmonic frequencies in place. `factor` > 1.0 raises formants, < 1.0
// lowers them, 1.0 is a no-op.
//
// Implemented by cepstral-liftered envelope estimation + envelope warping
// per STFT frame. May be applied after a pitch shift to set the final
// formant position independently of pitch.
std::vector<double> formant_shift(const std::vector<double>& input,
                                  double factor,
                                  size_t nfft = 2048,
                                  size_t hop = 512);

}  // namespace va::dsp
