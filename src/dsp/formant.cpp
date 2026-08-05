// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 voice_anonymizer contributors
#include "dsp/formant.h"

#include <algorithm>
#include <cmath>
#include <complex>

#include "dsp/fft.h"
#include "dsp/stft.h"

namespace va::dsp {

namespace {

// Cepstral spectral envelope of a single magnitude spectrum.
// `logmag` has nfft/2+1 entries (0..nyquist). `envelope` receives the same
// length, holding the smoothed envelope in the log domain.
void estimate_envelope(const std::vector<double>& logmag, size_t nfft,
                       size_t lifter_cutoff, std::vector<double>& envelope) {
    const size_t nbins = logmag.size();
    std::vector<std::complex<double>> cep(nfft);
    for (size_t k = 0; k < nbins; ++k) cep[k] = std::complex<double>(logmag[k], 0.0);
    // Mirror the positive-freq half for a real IFFT.
    for (size_t k = 1; k < nbins; ++k) cep[nfft - k] = cep[k];

    fft(cep, true);  // -> cepstrum (real, symmetric input)

    // Lifter: keep low quefrencies only.
    for (size_t q = lifter_cutoff + 1; q < nfft - lifter_cutoff; ++q) {
        cep[q] = std::complex<double>(0.0, 0.0);
    }
    fft(cep, false);  // -> smooth log-magnitude envelope (in logmag bins)

    envelope.resize(nbins);
    for (size_t k = 0; k < nbins; ++k) envelope[k] = cep[k].real();
}

double linear_interp(const std::vector<double>& v, double x) {
    const size_t n = v.size();
    if (x <= 0.0) return v[0];
    if (x >= static_cast<double>(n - 1)) return v[n - 1];
    const size_t i = static_cast<size_t>(std::floor(x));
    const size_t j = i + 1;
    const double t = x - static_cast<double>(i);
    return v[i] * (1.0 - t) + v[j] * t;
}

}  // namespace

std::vector<double> formant_shift(const std::vector<double>& input,
                                  double factor, size_t nfft, size_t hop) {
    if (input.empty()) return {};
    if (factor <= 0.0 || !std::isfinite(factor)) {
        throw std::invalid_argument("invalid formant factor");
    }
    if (std::fabs(factor - 1.0) < 1e-9) return input;

    const size_t nbins = nfft / 2 + 1;
    const size_t lifter_cutoff = std::max<size_t>(8, nfft / 16);
    const size_t num_frames =
        (input.size() < nfft ? 1 : (input.size() - nfft) / hop + 2);

    const size_t ola_len = nfft + (num_frames - 1) * hop + nfft;
    std::vector<double> ola(ola_len, 0.0);
    std::vector<double> norm(ola_len, 0.0);

    STFT stft(nfft, hop);
    std::vector<std::complex<double>> bins;
    std::vector<double> logmag(nbins), env(nbins);
    std::vector<double> frame(nfft, 0.0);
    const double eps = 1e-6;

    for (size_t m = 0; m < num_frames; ++m) {
        const size_t start = m * hop;
        for (size_t i = 0; i < nfft; ++i) {
            frame[i] = (start + i < input.size()) ? input[start + i] : 0.0;
        }
        stft.analyze(frame.data(), bins);

        for (size_t k = 0; k < nbins; ++k) {
            logmag[k] = std::log(std::abs(bins[k]) + eps);
        }
        estimate_envelope(logmag, nfft, lifter_cutoff, env);

        std::vector<std::complex<double>> synth_bins(nbins);
        for (size_t k = 0; k < nbins; ++k) {
            // Envelope warped to factor: E'(f_k) = E(f_k / factor).
            const double x = static_cast<double>(k) / factor;
            const double env_shifted = linear_interp(env, x);
            double ratio = std::exp(env_shifted - env[k]);
            ratio = std::clamp(ratio, 0.05, 20.0);
            synth_bins[k] = bins[k] * ratio;
        }

        const size_t out_offset = m * hop;
        stft.synthesize(synth_bins, ola.data(), out_offset);
        for (size_t i = 0; i < nfft; ++i) {
            norm[out_offset + i] += stft.window_sq(i);
        }
    }

    std::vector<double> out;
    out.reserve(ola_len);
    for (size_t i = 0; i < ola_len; ++i) {
        out.push_back(norm[i] > 1e-12 ? ola[i] / norm[i] : 0.0);
    }
    size_t target = input.size();
    if (target > out.size()) target = out.size();
    out.resize(target);
    return out;
}

}  // namespace va::dsp
