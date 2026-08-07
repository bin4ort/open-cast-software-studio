// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "dsp/pipeline.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "dsp/denoise.h"
#include "dsp/formant.h"
#include "dsp/phase_vocoder.h"
#include "dsp/vad.h"

namespace ocs::dsp {

const std::vector<Preset>& presets() {
    static const std::vector<Preset> kPresets = {
        {"subtle", "Slight shift, formants preserved", {2.0, 0.0, false}},
        {"strong", "Larger shift, formants follow", {6.0, 6.0, false}},
        {"deep", "Lower, heavier voice", {-5.0, -3.0, false}},
        {"chipmunk", "High and bright", {12.0, 12.0, false}},
        {"robotic", "Shifted with pitch/denoise for synthetic timbre",
         {4.0, 2.0, true}},
    };
    return kPresets;
}

const Preset* preset_named(const std::string& name) {
    for (const auto& p : presets()) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

namespace {

// Applies pitch + formant shift to a slice of the signal.
std::vector<double> shift_slice(const std::vector<double>& slice,
                                int sample_rate, const ShiftParams& params) {
    const double f_pitch = semitones_to_ratio(params.pitch_semitones);
    std::vector<double> out = pitch_shift(slice, f_pitch, sample_rate);
    // Formants moved with pitch by f_pitch; correct to the requested target.
    const double f_formant = semitones_to_ratio(params.formant_semitones);
    const double correction = f_formant / f_pitch;
    if (std::fabs(correction - 1.0) > 1e-9) {
        out = formant_shift(out, correction);
    }
    return out;
}

void crossfade_splice(std::vector<double>& out,
                      const std::vector<double>& processed,
                      size_t start, size_t end, size_t fade) {
    // `processed` corresponds to exactly [start, end). Crossfade edges to
    // avoid clicks against the original.
    const size_t f1 = std::min(fade, (end - start) / 2);
    for (size_t i = 0; i < f1; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(std::max<size_t>(f1, 1));
        out[start + i] = out[start + i] * (1.0 - t) + processed[i] * t;
    }
    for (size_t i = f1; i + f1 < end - start; ++i) {
        out[start + i] = processed[i];
    }
    const size_t lo = end - start > f1 ? end - start - f1 : 0;
    for (size_t i = lo; i < end - start; ++i) {
        const double t = static_cast<double>(i - lo) /
                         static_cast<double>(std::max<size_t>(end - start - lo, 1));
        out[start + i] = out[start + i] * t + processed[i] * (1.0 - t);
    }
}

std::vector<double> process_regions(
    const std::vector<double>& input, int sample_rate,
    const std::vector<std::pair<size_t, size_t>>& regions,
    const ShiftParams& params) {
    std::vector<double> out = input;
    const size_t pad = static_cast<size_t>(0.05 * sample_rate);  // 50ms context

    for (const auto& [s, e] : regions) {
        if (e <= s || s >= input.size()) continue;
        const size_t start = s > pad ? s - pad : 0;
        const size_t end = std::min(input.size(), e + pad);
        std::vector<double> slice(input.begin() + static_cast<long>(start),
                                  input.begin() + static_cast<long>(end));
        std::vector<double> shifted = shift_slice(slice, sample_rate, params);
        // Trim the padded context back off (shift preserves length).
        if (shifted.size() < end - start) shifted.resize(end - start);
        else if (shifted.size() > end - start) shifted.resize(end - start);
        // The processed slice covers [start,end); splice only the true region
        // [s,e) but keep phase continuity via the padded processing.
        std::vector<double> region(shifted.begin() + static_cast<long>(s - start),
                                   shifted.begin() + static_cast<long>(e - start));
        const size_t fade = static_cast<size_t>(0.02 * sample_rate);
        crossfade_splice(out, region, s, e, fade);
    }
    return out;
}

}  // namespace

std::vector<double> process_global(const std::vector<double>& input,
                                   int sample_rate,
                                   const ShiftParams& params) {
    if (input.empty() || sample_rate <= 0) return input;

    std::vector<double> src = input;
    if (params.enable_denoise) {
        DenoiseParams d;
        d.over_subtract = 1.5;
        src = denoise(src, d);
    }

    const auto regions = detect_speech(src, sample_rate);
    return process_regions(src, sample_rate, regions, params);
}

std::vector<double> process_selective(
    const std::vector<double>& input, int sample_rate,
    const std::vector<Segment>& segments, const ShiftParams& params) {
    if (input.empty() || sample_rate <= 0) return input;

    std::vector<double> src = input;
    if (params.enable_denoise) {
        DenoiseParams d;
        d.over_subtract = 1.5;
        src = denoise(src, d);
    }

    std::vector<std::pair<size_t, size_t>> regions;
    for (const auto& seg : segments) {
        if (!seg.has_pii) continue;
        size_t s = static_cast<size_t>(seg.start * sample_rate);
        size_t e = static_cast<size_t>(seg.end * sample_rate);
        s = s > 0 ? s : 0;
        e = std::min(src.size(), e);
        // Union with VAD to avoid shifting pure silence around the span.
        const auto speech = detect_speech(src, sample_rate);
        for (const auto& [vs, ve] : speech) {
            if (ve > s && vs < e) {
                regions.emplace_back(std::max(vs, s), std::min(ve, e));
            }
        }
    }
    return process_regions(src, sample_rate, regions, params);
}

}  // namespace ocs::dsp
