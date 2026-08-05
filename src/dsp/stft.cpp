// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 voice_anonymizer contributors
#include "dsp/stft.h"

#include <cmath>

#include "dsp/fft.h"

namespace va::dsp {

std::vector<double> hann_window(size_t n) {
    std::vector<double> w(n);
    for (size_t i = 0; i < n; ++i) {
        w[i] = 0.5 - 0.5 * std::cos(2.0 * M_PI * static_cast<double>(i) /
                                    static_cast<double>(n - 1));
    }
    return w;
}

STFT::STFT(size_t nfft, size_t hop)
    : nfft_(nfft), hop_(hop), window_(hann_window(nfft)) {
    scratch_.resize(nfft);
}

void STFT::analyze(const double* frame, std::vector<std::complex<double>>& out) {
    scratch_.resize(nfft_);
    for (size_t i = 0; i < nfft_; ++i) {
        scratch_[i] = std::complex<double>(frame[i] * window_[i], 0.0);
    }
    fft(scratch_, false);
    out.resize(num_bins());
    for (size_t k = 0; k < num_bins(); ++k) out[k] = scratch_[k];
}

void STFT::synthesize(const std::vector<std::complex<double>>& bins,
                      double* ola, size_t offset) {
    scratch_.resize(nfft_);
    const size_t nb = num_bins();
    for (size_t k = 0; k < nb; ++k) scratch_[k] = bins[k];
    // Conjugate-mirror the positive frequencies so the IFFT recovers the
    // real signal at full amplitude (one-sided spectra only give x/2).
    for (size_t k = 1; k < nfft_ / 2; ++k) {
        scratch_[nfft_ - k] = std::conj(bins[k]);
    }
    fft(scratch_, true);
    for (size_t i = 0; i < nfft_; ++i) {
        ola[offset + i] += scratch_[i].real() * window_[i];
    }
}

std::vector<double> STFT::overlap_norm_buffer(size_t total) const {
    std::vector<double> norm(total, 0.0);
    for (size_t offset = 0; offset + nfft_ <= total; offset += hop_) {
        for (size_t i = 0; i < nfft_; ++i) {
            norm[offset + i] += window_[i] * window_[i];
        }
    }
    return norm;
}

}  // namespace va::dsp
