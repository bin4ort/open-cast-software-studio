// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "dsp/phase_vocoder.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstring>
#include <stdexcept>

#include <samplerate.h>

#include "dsp/fft.h"
#include "dsp/stft.h"

namespace ocs::dsp {

double semitones_to_ratio(double semitones) {
    return std::pow(2.0, semitones / 12.0);
}

namespace {

inline double princarg(double p) {
    return p - 2.0 * M_PI * std::round(p / (2.0 * M_PI));
}

}  // namespace

std::vector<double> time_stretch(const std::vector<double>& input,
                                 double stretch, size_t nfft, size_t hop) {
    if (input.empty()) return {};
    if (stretch <= 0.0) throw std::invalid_argument("stretch factor must be > 0");

    const size_t analysis_hop = hop;
    const double synthesis_hop_d = static_cast<double>(analysis_hop) * stretch;
    if (synthesis_hop_d < 1.0) {
        // Extreme speed-up: clamp synthesis hop to 1 sample to stay stable.
        // Callers normally use stretch in [0.5, 2.0].
        throw std::invalid_argument("stretch factor too small for this hop size");
    }
    const size_t synthesis_hop = static_cast<size_t>(std::lround(synthesis_hop_d));

    const size_t num_frames =
        (input.size() < nfft ? 1
                             : (input.size() - nfft) / analysis_hop + 2);

    const size_t ola_len = nfft + (num_frames - 1) * synthesis_hop + nfft;
    std::vector<double> ola(ola_len, 0.0);
    std::vector<double> norm(ola_len, 0.0);

    STFT stft(nfft, analysis_hop);
    const size_t nbins = stft.num_bins();
    std::vector<std::complex<double>> bins;
    std::vector<double> prev_phase(nbins, 0.0);
    std::vector<double> synth_phase(nbins, 0.0);
    std::vector<double> frame(nfft, 0.0);

    for (size_t m = 0; m < num_frames; ++m) {
        const size_t start = m * analysis_hop;
        for (size_t i = 0; i < nfft; ++i) {
            frame[i] = (start + i < input.size()) ? input[start + i] : 0.0;
        }

        stft.analyze(frame.data(), bins);

        std::vector<std::complex<double>> synth_bins(nbins);

        for (size_t k = 0; k < nbins; ++k) {
            const double w_k = 2.0 * M_PI * static_cast<double>(k) /
                               static_cast<double>(nfft);
            const double mag = std::abs(bins[k]);
            const double phase = std::arg(bins[k]);

            if (m == 0) {
                synth_phase[k] = phase;
            } else {
                const double delta =
                    princarg(phase - prev_phase[k] - w_k *
                                  static_cast<double>(analysis_hop));
                const double inst_freq =
                    w_k + delta / static_cast<double>(analysis_hop);
                synth_phase[k] +=
                    inst_freq * static_cast<double>(synthesis_hop);
            }
            prev_phase[k] = phase;

            synth_bins[k] =
                std::polar(mag, synth_phase[k]);
        }

        const size_t out_offset = m * synthesis_hop;
        stft.synthesize(synth_bins, ola.data(), out_offset);

        for (size_t i = 0; i < nfft; ++i) {
            norm[out_offset + i] += stft.window_sq(i);
        }
    }

    // Normalized overlap-add, then trim to stretched length.
    std::vector<double> out;
    out.reserve(ola_len);
    for (size_t i = 0; i < ola_len; ++i) {
        out.push_back(norm[i] > 1e-12 ? ola[i] / norm[i] : 0.0);
    }

    size_t target = static_cast<size_t>(
        std::lround(static_cast<double>(input.size()) * stretch));
    if (target > out.size()) target = out.size();
    out.resize(target);
    return out;
}

std::vector<double> pitch_shift(const std::vector<double>& input,
                                double factor, int sample_rate) {
    if (factor <= 0.0 || !std::isfinite(factor)) {
        throw std::invalid_argument("invalid pitch factor");
    }
    if (std::fabs(factor - 1.0) < 1e-9) return input;

    const double stretch = factor;             // slow down (pitch up) ...
    std::vector<double> stretched = time_stretch(input, stretch);

    const double ratio = 1.0 / factor;         // ... then resample back.
    if (sample_rate <= 0) return stretched;

    const size_t in_len = stretched.size();
    const size_t out_cap = static_cast<size_t>(std::lround(in_len * ratio)) + 64;
    std::vector<float> in_f(in_len), out_f(out_cap);
    for (size_t i = 0; i < in_len; ++i) in_f[i] = static_cast<float>(stretched[i]);

    SRC_DATA sd{};
    sd.data_in = in_f.data();
    sd.input_frames = static_cast<long>(in_len);
    sd.data_out = out_f.data();
    sd.output_frames = static_cast<long>(out_cap);
    sd.src_ratio = ratio;
    sd.end_of_input = 1;

    int err = src_simple(&sd, SRC_SINC_MEDIUM_QUALITY, 1);
    if (err != 0) {
        throw std::runtime_error(std::string("libsamplerate: ") + src_strerror(err));
    }
    std::vector<double> resampled(static_cast<size_t>(sd.output_frames_gen));
    for (size_t i = 0; i < resampled.size(); ++i) {
        resampled[i] = static_cast<double>(out_f[i]);
    }
    return resampled;
}

// ---------------------------------------------------------------------------
// Streaming pitch shifter
//
// Real-time version of `pitch_shift`: phase-vocoder time-stretch by
// `stretch_` followed by a streaming resampler at 1/stretch. State is carried
// across `process()` calls so audio can be pushed block-by-block.
// ---------------------------------------------------------------------------
namespace {

inline double wrap_phase(double phase) {
    return phase - 2.0 * M_PI * std::round(phase / (2.0 * M_PI));
}

}  // namespace

PitchShifter::PitchShifter(size_t nfft, size_t osamp, double pitch_factor)
    : nfft_(nfft), osamp_(osamp), hop_(nfft / osamp),
      hs_(nfft / osamp), pitch_factor_(pitch_factor), stft_(nfft, nfft / osamp) {
    const size_t nb = nfft / 2 + 1;
    prev_phase_.assign(nb, 0.0);
    synth_phase_.assign(nb, 0.0);
    bins_.resize(nb);
    frame_.resize(nfft, 0.0);

    // Pitch up (factor > 1) means stretch the signal out, then resample back
    // down; that shifts pitch up while restoring duration (see pitch_shift).
    stretch_ = pitch_factor;
    hs_ = static_cast<size_t>(std::lround(static_cast<double>(hop_) * stretch_));
    if (hs_ < 1) hs_ = 1;
    resample_ratio_ = 1.0 / stretch_;

    int err = 0;
    src_state_ = src_new(SRC_SINC_MEDIUM_QUALITY, 1, &err);
    if (!src_state_) {
        throw std::runtime_error(std::string("libsamplerate: ") + src_strerror(err));
    }
}

PitchShifter::~PitchShifter() {
    if (src_state_) src_delete(static_cast<SRC_STATE*>(src_state_));
}

void PitchShifter::reset() {
    in_.clear();
    ola_.clear();
    ola_norm_.clear();
    stretched_.clear();
    emit_pos_ = 0;
    frame_start_ = 0;
    frames_ = 0;
    std::fill(prev_phase_.begin(), prev_phase_.end(), 0.0);
    std::fill(synth_phase_.begin(), synth_phase_.end(), 0.0);
    if (src_state_) src_reset(static_cast<SRC_STATE*>(src_state_));
}

void PitchShifter::set_pitch_factor(double factor) {
    if (std::fabs(factor - pitch_factor_) < 1e-9) return;
    pitch_factor_ = factor;
    // Restart the pipeline on a pitch change to avoid discontinuities.
    reset();
    stretch_ = factor;
    hs_ = static_cast<size_t>(std::lround(static_cast<double>(hop_) * stretch_));
    if (hs_ < 1) hs_ = 1;
    resample_ratio_ = 1.0 / stretch_;
}

void PitchShifter::analyze_available_frames() {
    const size_t nfft = nfft_;
    const size_t hop = hop_;
    const size_t nb = nfft / 2 + 1;
    const auto& w = stft_.window();

    while (in_.size() >= nfft) {
        for (size_t i = 0; i < nfft; ++i) frame_[i] = in_[i];

        stft_.analyze(frame_.data(), bins_);

        std::vector<std::complex<double>> synth_bins(nb);
        for (size_t k = 0; k < nb; ++k) {
            const double w_k = 2.0 * M_PI * static_cast<double>(k) /
                               static_cast<double>(nfft);
            const double mag = std::abs(bins_[k]);
            const double phase = std::arg(bins_[k]);
            if (frames_ == 0) {
                synth_phase_[k] = phase;
            } else {
                const double delta =
                    wrap_phase(phase - prev_phase_[k] -
                               w_k * static_cast<double>(hop));
                const double inst_freq = w_k + delta / static_cast<double>(hop);
                synth_phase_[k] += inst_freq * static_cast<double>(hs_);
            }
            prev_phase_[k] = phase;
            synth_bins[k] = std::polar(mag, synth_phase_[k]);
        }

        // Raw IFFT (no window here) so the overlap-add below applies it once.
        std::vector<std::complex<double>> Yc(nfft);
        for (size_t k = 0; k < nb; ++k) Yc[k] = synth_bins[k];
        for (size_t k = 1; k < nfft / 2; ++k) Yc[nfft - k] = std::conj(Yc[k]);
        fft(Yc, true);

        // Overlap-add the synthesis frame at stream position frame_start_.
        const size_t need = frame_start_ + nfft - emit_pos_;
        if (ola_.size() < need) {
            ola_.resize(need, 0.0);
            ola_norm_.resize(need, 0.0);
        }
        const size_t off = frame_start_ - emit_pos_;
        for (size_t i = 0; i < nfft; ++i) {
            ola_[off + i] += w[i] * Yc[i].real();
            ola_norm_[off + i] += w[i] * w[i];
        }

        in_.erase(in_.begin(), in_.begin() + static_cast<long>(hop));
        frame_start_ += hs_;
        ++frames_;
    }
}

void PitchShifter::drain_stretched() {
    // Positions below frame_start_ are final (no future frame touches them).
    const size_t final_count = frame_start_ > emit_pos_
                                   ? frame_start_ - emit_pos_
                                   : 0;
    for (size_t i = 0; i < final_count; ++i) {
        const double n = ola_norm_[i];
        stretched_.push_back(n > 1e-12 ? ola_[i] / n : 0.0);
    }
    if (final_count > 0) {
        ola_.erase(ola_.begin(), ola_.begin() + static_cast<long>(final_count));
        ola_norm_.erase(ola_norm_.begin(),
                        ola_norm_.begin() + static_cast<long>(final_count));
        emit_pos_ += final_count;
    }
}

void PitchShifter::flush_resampler(float* out, size_t max_out,
                                   size_t& produced) {
    SRC_STATE* st = static_cast<SRC_STATE*>(src_state_);
    const size_t out_cap = max_out + 64;
    std::vector<float> out_buf(out_cap, 0.0f);
    std::vector<float> in_buf;

    while (!stretched_.empty() && produced < max_out) {
        const size_t chunk = std::min<size_t>(stretched_.size(), 4096);
        in_buf.resize(chunk);
        for (size_t i = 0; i < chunk; ++i) {
            in_buf[i] = static_cast<float>(stretched_[i]);
        }
        SRC_DATA sd{};
        sd.data_in = in_buf.data();
        sd.input_frames = static_cast<long>(chunk);
        sd.data_out = out_buf.data();
        sd.output_frames = static_cast<long>(out_cap);
        sd.src_ratio = resample_ratio_;
        sd.end_of_input = 0;
        const int err = src_process(st, &sd);
        if (err != 0) break;

        const size_t gen = static_cast<size_t>(sd.output_frames_gen);
        const size_t take = std::min(gen, max_out - produced);
        for (size_t i = 0; i < take; ++i) out[produced + i] = out_buf[i];
        produced += take;
        // Extra produced samples beyond max_out are dropped for this call; the
        // resampler's internal history keeps the stream continuous.
        const size_t used = static_cast<size_t>(sd.input_frames_used);
        stretched_.erase(stretched_.begin(),
                         stretched_.begin() + static_cast<long>(used));
    }
}

size_t PitchShifter::process(const float* in, size_t n, float* out,
                             size_t max_out) {
    for (size_t i = 0; i < n; ++i) in_.push_back(static_cast<double>(in[i]));

    analyze_available_frames();
    drain_stretched();

    size_t produced = 0;
    flush_resampler(out, max_out, produced);
    return produced;
}

}  // namespace ocs::dsp
