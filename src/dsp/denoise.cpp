// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 voice_anonymizer contributors
#include "dsp/denoise.h"

#include <algorithm>
#include <cmath>
#include <complex>

#include "dsp/stft.h"

namespace va::dsp {

std::vector<double> denoise(const std::vector<double>& input,
                            const DenoiseParams& params) {
    if (input.empty()) return {};
    if (params.noise_floor_sec <= 0.0) return input;

    const size_t nfft = 2048;
    const size_t hop = 512;
    const size_t nbins = nfft / 2 + 1;
    const size_t num_frames =
        (input.size() < nfft ? 1 : (input.size() - nfft) / hop + 2);

    const size_t ola_len = nfft + (num_frames - 1) * hop + nfft;
    std::vector<double> ola(ola_len, 0.0);
    std::vector<double> norm(ola_len, 0.0);

    STFT stft(nfft, hop);
    std::vector<std::complex<double>> bins;
    std::vector<double> frame(nfft, 0.0);

    // Estimate noise magnitude from the leading frames.
    // 50ms @ whatever rate = ~ hop*2 frames; use 25 frames (~1.2s at 44.1k).
    const size_t noise_frames = 25;
    std::vector<double> noise_mag(nbins, 0.0);
    {
        size_t counted = 0;
        for (size_t m = 0; m < num_frames && m < noise_frames; ++m) {
            const size_t start = m * hop;
            for (size_t i = 0; i < nfft; ++i) {
                frame[i] = (start + i < input.size()) ? input[start + i] : 0.0;
            }
            stft.analyze(frame.data(), bins);
            for (size_t k = 0; k < nbins; ++k) {
                noise_mag[k] += std::abs(bins[k]);
            }
            ++counted;
        }
        if (counted > 0) {
            for (auto& v : noise_mag) v /= static_cast<double>(counted);
        }
    }

    for (size_t m = 0; m < num_frames; ++m) {
        const size_t start = m * hop;
        for (size_t i = 0; i < nfft; ++i) {
            frame[i] = (start + i < input.size()) ? input[start + i] : 0.0;
        }
        stft.analyze(frame.data(), bins);

        double frame_rms = 0.0;
        for (size_t i = 0; i < nfft; ++i) frame_rms += frame[i] * frame[i];
        frame_rms = std::sqrt(frame_rms / static_cast<double>(nfft));
        const double gate = std::pow(10.0, params.gate_db / 20.0);
        const double atten = frame_rms > gate ? 1.0
                                              : std::max(frame_rms / gate, 0.05);

        for (size_t k = 0; k < nbins; ++k) {
            const double mag = std::abs(bins[k]);
            double out_mag =
                mag - params.over_subtract * noise_mag[k];
            if (out_mag < params.spectral_floor * noise_mag[k]) {
                out_mag = params.spectral_floor * noise_mag[k];
            }
            const std::complex<double> ph = bins[k] / (mag + 1e-12);
            bins[k] = ph * (out_mag * atten);
        }

        const size_t out_offset = m * hop;
        stft.synthesize(bins, ola.data(), out_offset);
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
