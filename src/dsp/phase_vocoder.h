// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <complex>
#include <cstddef>
#include <deque>
#include <vector>

#include "dsp/stft.h"

namespace ocs::dsp {

// Semitone delta -> frequency ratio (12 semitones = one octave).
double semitones_to_ratio(double semitones);

// Phase-vocoder time-stretch with pitch preservation.
// `stretch` > 1.0 slows the signal down, < 1.0 speeds it up.
// The result is windowed overlap-add normalized; length ~= input*factor.
std::vector<double> time_stretch(const std::vector<double>& input,
                                 double stretch,
                                 size_t nfft = 2048,
                                 size_t hop = 512);

// Pitch shift by `factor` (>1 raises pitch). Formants are shifted along with
// the pitch (i.e. classic "chipmunk/deep" voice). Duration is preserved.
std::vector<double> pitch_shift(const std::vector<double>& input,
                                double factor,
                                int sample_rate);

// Streaming pitch shifter for the live path. Implements the same
// time-stretch-then-resample algorithm as `pitch_shift` (verified by unit
// tests) but with state carried across calls so it can run block-by-block in
// real time. Formants follow the pitch (classic).
//
// `process` may produce up to `max_out` samples and returns the actual count;
// over a long stream the count averages to `n` (1:1 with the input).
class PitchShifter {
public:
    PitchShifter(size_t nfft = 2048, size_t osamp = 4, double pitch_factor = 1.0);
    ~PitchShifter();

    PitchShifter(const PitchShifter&) = delete;
    PitchShifter& operator=(const PitchShifter&) = delete;

    void set_pitch_factor(double factor);
    double pitch_factor() const { return pitch_factor_; }

    // Reads up to n input samples, writes up to max_out output samples.
    // Returns the number of output samples produced.
    size_t process(const float* in, size_t n, float* out, size_t max_out);

    void reset();

private:
    void analyze_available_frames();
    void drain_stretched();
    void flush_resampler(float* out, size_t max_out, size_t& produced);

    size_t nfft_;
    size_t osamp_;
    size_t hop_;               // analysis/synthesis stride (nfft/osamp)
    size_t hs_;                // synthesis stride = hop * stretch
    double pitch_factor_;
    double stretch_;           // 1/pitch-relative time-stretch factor

    // Analysis state.
    STFT stft_;
    std::deque<double> in_;
    std::vector<double> prev_phase_;
    std::vector<double> synth_phase_;
    std::vector<std::complex<double>> bins_;
    std::vector<double> frame_;
    size_t frames_ = 0;

    // Synthesis OLA state. ola_[i] holds output at stream position
    // (emit_pos_ + i); ola_norm_ holds the running sum of window^2.
    std::deque<double> ola_;
    std::deque<double> ola_norm_;
    size_t emit_pos_ = 0;
    size_t frame_start_ = 0;

    // Post-stretch resampling state.
    std::deque<double> stretched_;
    void* src_state_ = nullptr;
    double resample_ratio_ = 1.0;
};

}  // namespace ocs::dsp
