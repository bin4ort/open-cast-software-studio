// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 voice_anonymizer contributors
#pragma once

#include <complex>
#include <cstddef>
#include <vector>

namespace va::dsp {

// Static Hann window helper.
std::vector<double> hann_window(size_t n);

// Short-time Fourier transform: frame analysis + windowed overlap-add synthesis.
//
// The class itself is stateless; overlap-add accumulation is done by the
// caller via `synthesize()` into a scratch buffer, followed by a final
// normalization pass (see `normalized_ola` / `overlap_norm_buffer`).
class STFT {
public:
    STFT(size_t nfft, size_t hop);

    size_t nfft() const { return nfft_; }
    size_t hop() const { return hop_; }
    size_t num_bins() const { return nfft_ / 2 + 1; }

    // Current analysis/synthesis window (Hann). Applies in both analyze()
    // and synthesize(), so normalized OLA divides by the squared window sum.
    const std::vector<double>& window() const { return window_; }
    double window_sq(size_t i) const { return window_[i] * window_[i]; }

    // Windows `frame` (exactly nfft samples) and FFTs it into `out`
    // (num_bins() complex bins). Reuses internal scratch buffers.
    void analyze(const double* frame, std::vector<std::complex<double>>& out);

    // IFFTs `bins`, windows and overlap-adds into `ola` at offset `offset`.
    void synthesize(const std::vector<std::complex<double>>& bins,
                    double* ola, size_t offset);

    // Builds a buffer of the same length as `ola` containing the running sum
    // of squared window weights, for normalized overlap-add.
    //   overlap_norm_buffer(size) -> norm[]
    //   after OLA: out[i] = ola[i] / norm[i]
    std::vector<double> overlap_norm_buffer(size_t total) const;

private:
    size_t nfft_;
    size_t hop_;
    std::vector<double> window_;
    std::vector<std::complex<double>> scratch_;
};

}  // namespace va::dsp
