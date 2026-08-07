// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "test_util.h"

#include <vector>

#include "dsp/fft.h"
#include "dsp/stft.h"

using ocs::dsp::STFT;
using ocs::dsp::fft;

TEST(fft_round_trip) {
    const size_t n = 256;
    std::vector<std::complex<double>> x(n);
    for (size_t i = 0; i < n; ++i) {
        x[i] = std::complex<double>(std::sin(2.0 * 3.14159265358979 * 7.0 *
                                             static_cast<double>(i) /
                                             static_cast<double>(n)),
                                    0.0);
    }
    std::vector<std::complex<double>> orig = x;
    fft(x, false);
    fft(x, true);
    for (size_t i = 0; i < n; ++i) {
        CHECK_NEAR(x[i].real(), orig[i].real(), 1e-9);
        CHECK_NEAR(x[i].imag(), 0.0, 1e-9);
    }
}

TEST(stft_ola_round_trip) {
    const size_t sr = 48000;
    const size_t nfft = 1024;
    const size_t hop = 256;
    const double dur = 0.25;
    const size_t total = static_cast<size_t>(sr * dur);

    std::vector<double> sig(total);
    for (size_t i = 0; i < total; ++i) {
        sig[i] = 0.5 * std::sin(2.0 * 3.14159265358979 * 440.0 *
                                static_cast<double>(i) / static_cast<double>(sr));
    }

    STFT stft(nfft, hop);
    // Enough full frames so the overlap-add covers the whole signal; the
    // trailing frame is zero-padded and normalized with the rest.
    const size_t num_frames =
        (total >= nfft ? (total - nfft + hop - 1) / hop + 1 : 1);
    const size_t ola_len = nfft + (num_frames - 1) * hop;
    std::vector<double> ola(ola_len, 0.0);
    std::vector<double> norm(ola_len, 0.0);
    std::vector<std::complex<double>> bins;
    std::vector<double> frame(nfft, 0.0);

    for (size_t m = 0; m < num_frames; ++m) {
        const size_t start = m * hop;
        for (size_t i = 0; i < nfft; ++i) {
            frame[i] = (start + i < total) ? sig[start + i] : 0.0;
        }
        stft.analyze(frame.data(), bins);
        stft.synthesize(bins, ola.data(), m * hop);
        for (size_t i = 0; i < nfft; ++i) {
            norm[m * hop + i] += stft.window_sq(i);
        }
    }

    double err = 0.0;
    for (size_t i = 0; i < total; ++i) {
        const double rec = norm[i] > 1e-12 ? ola[i] / norm[i] : 0.0;
        err += (rec - sig[i]) * (rec - sig[i]);
    }
    err = std::sqrt(err / static_cast<double>(total));
    CHECK_NEAR(err, 0.0, 1e-4);
}
