// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "ai/correction.h"

#include <sstream>

#include "ai/llm.h"
#include "util/json.h"

namespace ocs::ai {

namespace {

// Quantizes a 0..1 envelope to a compact 0..100 integer list.
std::string env_line(const std::vector<double>& env) {
    std::ostringstream ss;
    for (size_t i = 0; i < env.size(); ++i) {
        if (i) ss << ' ';
        int v = static_cast<int>(env[i] * 100.0 + 0.5);
        if (v < 0) v = 0;
        if (v > 100) v = 100;
        ss << v;
    }
    return ss.str();
}

double clamp_st(double v) {
    if (v < -24.0) return -24.0;
    if (v > 24.0) return 24.0;
    return v;
}

}  // namespace

std::string correction_prompt(const dsp::AudioDiff& diff,
                              const dsp::ShiftParams& current,
                              dsp::Gender target) {
    std::ostringstream ss;
    ss << "You are tuning a voice-transformation DSP. The original and the "
          "processed recordings were compared. Below are their normalized "
          "RMS envelopes (0..100, " << diff.buckets << " buckets; first line "
          "original, second line processed):\n"
       << env_line(diff.orig_env) << "\n"
       << env_line(diff.proc_env) << "\n"
       << "Statistics: original RMS " << diff.orig_rms
       << ", processed RMS " << diff.proc_rms << ", level ratio "
       << diff.rms_ratio << ", mean absolute deviation " << diff.mean_abs_dev
       << ", peaks " << diff.peak_orig << " -> " << diff.peak_proc << ".\n"
       << "Current configuration: pitch " << current.pitch_semitones
       << " st, formants " << current.formant_semitones << " st, denoise "
       << (current.enable_denoise ? "on" : "off")
       << ". Target voice: " << dsp::gender_name(target) << ".\n"
       << "The processed voice deviates from the target. Suggest a corrected "
          "configuration. Respond with JSON ONLY, no prose, in this exact "
          "shape: {\"pitch\": <semitones -24..24>, \"formant\": <semitones "
          "-24..24>, \"denoise\": true|false, \"reason\": \"<short "
          "explanation>\"}.";
    return ss.str();
}

bool parse_correction_response(const std::string& json,
                               const dsp::ShiftParams& current,
                               dsp::ShiftParams& corrected,
                               std::string& reason) {
    corrected = current;
    reason.clear();
    try {
        const util::Json j = util::Json::parse(json);
        if (!j.is_object() || !j.contains("pitch") || !j.contains("formant")) {
            return false;
        }
        corrected.pitch_semitones = clamp_st(j.num("pitch", current.pitch_semitones));
        corrected.formant_semitones = clamp_st(j.num("formant", current.formant_semitones));
        corrected.enable_denoise = j.flag("denoise", current.enable_denoise);
        reason = j.str("reason", "");
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool suggest_correction(const dsp::AudioDiff& diff,
                        const dsp::ShiftParams& current,
                        dsp::Gender target,
                        dsp::ShiftParams& corrected,
                        std::string& reason) {
    if (!llm::llm_available()) return false;
    const std::string sys =
        "You correct voice-transformation DSP settings from waveform "
        "comparison data. Reply with JSON only, exactly as requested.";
    const std::string resp =
        llm::complete(sys, correction_prompt(diff, current, target));
    if (resp.empty()) return false;
    return parse_correction_response(resp, current, corrected, reason);
}

}  // namespace ocs::ai
