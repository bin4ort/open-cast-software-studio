// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 voice_anonymizer contributors
#include "test_util.h"

#include <complex>
#include <vector>

#include "dsp/fft.h"
#include "dsp/phase_vocoder.h"

using va::dsp::pitch_shift;
using va::dsp::semitones_to_ratio;
using va::dsp::time_stretch;

namespace {

// Dominant frequency via zero crossings (robust across resampling).
double dominant_freq(const std::vector<double>& s, int sr) {
    size_t crossings = 0;
    double last = s.empty() ? 0.0 : s[0];
    size_t start = s.size() / 4;  // skip transient
    for (size_t i = start + 1; i < s.size(); ++i) {
        if ((s[i] > 0.0) != (last > 0.0)) ++crossings;
        last = s[i];
    }
    const double dur = static_cast<double>(s.size() - start) /
                       static_cast<double>(sr);
    return static_cast<double>(crossings) / (2.0 * dur);
}

}  // namespace

TEST(semitones_to_ratio) {
    CHECK_NEAR(semitones_to_ratio(12.0), 2.0, 1e-9);
    CHECK_NEAR(semitones_to_ratio(0.0), 1.0, 1e-9);
    CHECK_NEAR(semitones_to_ratio(-12.0), 0.5, 1e-9);
}

TEST(time_stretch_length) {
    const int sr = 48000;
    std::vector<double> sig(sr);  // 1 second
    for (int i = 0; i < sr; ++i) {
        sig[i] = 0.5 * std::sin(2.0 * 3.14159265358979 * 220.0 *
                                static_cast<double>(i) / static_cast<double>(sr));
    }
    const auto slowed = time_stretch(sig, 2.0);
    CHECK_NEAR(slowed.size(), 2.0 * sig.size(), 0.1 * sr);
    const auto sped = time_stretch(sig, 0.5);
    CHECK_NEAR(sped.size(), 0.5 * sig.size(), 0.1 * sr);
}

TEST(pitch_shift_up_two_semitones) {
    const int sr = 48000;
    std::vector<double> sig(2 * sr);
    for (size_t i = 0; i < sig.size(); ++i) {
        sig[i] = 0.5 * std::sin(2.0 * 3.14159265358979 * 440.0 *
                                static_cast<double>(i) / static_cast<double>(sr));
    }
    const auto out = pitch_shift(sig, semitones_to_ratio(2.0), sr);
    CHECK_NEAR(dominant_freq(out, sr), 440.0 * semitones_to_ratio(2.0), 8.0);
}

TEST(pitch_shift_down_three_semitones) {
    const int sr = 48000;
    std::vector<double> sig(2 * sr);
    for (size_t i = 0; i < sig.size(); ++i) {
        sig[i] = 0.5 * std::sin(2.0 * 3.14159265358979 * 440.0 *
                                static_cast<double>(i) / static_cast<double>(sr));
    }
    const double factor = semitones_to_ratio(-3.0);
    const auto out = pitch_shift(sig, factor, sr);
    CHECK_NEAR(dominant_freq(out, sr), 440.0 * factor, 8.0);
}

TEST(streaming_pitch_shifter_steady) {
    const int sr = 48000;
    const size_t chunk = 512;
    const size_t n = (2 * sr / chunk) * chunk;  // multiple of chunk size
    std::vector<float> in(n), out(n, 0.0f);
    for (size_t i = 0; i < n; ++i) {
        in[i] = 0.5f * std::sin(2.0f * 3.14159265f * 440.0f *
                                static_cast<float>(i) / static_cast<float>(sr));
    }
    va::dsp::PitchShifter shifter(2048, 4, semitones_to_ratio(4.0));
    size_t produced = 0;
    // Stream in 512-sample chunks like the live path does.
    for (size_t off = 0; off < n; off += chunk) {
        produced += shifter.process(in.data() + off, chunk,
                                    out.data() + produced,
                                    n - produced);
    }
    CHECK(produced >= n / 2);  // a real stream must be produced
    std::vector<double> d(out.begin() + produced / 2, out.begin() + produced);
    CHECK_NEAR(dominant_freq(d, sr), 440.0 * semitones_to_ratio(4.0), 12.0);
}
