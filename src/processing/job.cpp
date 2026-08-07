// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "processing/job.h"

#include <cstdio>
#include <fstream>

#include <samplerate.h>

#include "ai/rewriter.h"
#include "anonymize/redactor.h"
#include "audio/fileio.h"
#include "dsp/pipeline.h"
#include "transcribe/whisper.h"
#include "tts.h"

namespace ocs::job {

namespace {

std::string fmt_ts(double sec) {
    const int total = static_cast<int>(sec + 0.5);
    const int ms = static_cast<int>((sec - static_cast<double>(total)) * 1000.0);
    const int h = total / 3600;
    const int m = (total % 3600) / 60;
    const int s = total % 60;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d", h, m, s, ms < 0 ? 0 : ms);
    return buf;
}

bool write_transcript(const std::string& path,
                      const std::vector<Segment>& segments) {
    std::ofstream f(path);
    if (!f) return false;
    for (const auto& seg : segments) {
        f << "[" << fmt_ts(seg.start) << " - " << fmt_ts(seg.end) << "] "
          << seg.text << "\n";
    }
    return true;
}

std::vector<double> resample_to(const std::vector<double>& in, int from_sr,
                                int to_sr) {
    if (from_sr <= 0 || from_sr == to_sr || in.empty()) return in;
    const double ratio = static_cast<double>(to_sr) / static_cast<double>(from_sr);
    const size_t out_cap = static_cast<size_t>(in.size() * ratio) + 64;
    std::vector<float> in_f(in.size()), out_f(out_cap);
    for (size_t i = 0; i < in.size(); ++i) in_f[i] = static_cast<float>(in[i]);
    SRC_DATA sd{};
    sd.data_in = in_f.data();
    sd.input_frames = static_cast<long>(in_f.size());
    sd.data_out = out_f.data();
    sd.output_frames = static_cast<long>(out_cap);
    sd.src_ratio = ratio;
    sd.end_of_input = 1;
    if (src_simple(&sd, SRC_SINC_MEDIUM_QUALITY, 1) != 0) return in;
    std::vector<double> out(static_cast<size_t>(sd.output_frames_gen));
    for (size_t i = 0; i < out.size(); ++i) out[i] = static_cast<double>(out_f[i]);
    return out;
}

// Replaces out[s,e) with `tts`, crossfading the seams to avoid clicks.
std::vector<double> replace_region(std::vector<double> out,
                                   const std::vector<double>& tts,
                                   int sample_rate, size_t s, size_t e) {
    if (s >= out.size() || tts.empty()) return out;
    e = std::min(e, out.size());
    if (e <= s) return out;

    const size_t fade = std::min<size_t>(sample_rate / 100, tts.size() / 2);
    std::vector<double> t = tts;
    for (size_t i = 0; i < fade; ++i) {
        const double g = static_cast<double>(i) / static_cast<double>(fade);
        t[i] *= g;
        t[t.size() - 1 - i] *= g;
    }

    std::vector<double> result;
    result.reserve(out.size() - (e - s) + t.size());
    result.insert(result.end(), out.begin(), out.begin() + static_cast<long>(s));
    result.insert(result.end(), t.begin(), t.end());
    result.insert(result.end(), out.begin() + static_cast<long>(e), out.end());
    return result;
}

// Hybrid: TTS re-voices the rewritten behavior segments on top of the global
// shift. Returns the rewritten segments (marked has_pii) and the processed
// audio.
struct HybridResult {
    std::vector<Segment> segments;
    std::vector<double> audio;
};

HybridResult run_hybrid(const AudioData& audio, dsp::Gender target,
                        const dsp::ShiftParams& shift,
                        const std::vector<Segment>& segments) {
    HybridResult res;
    res.audio = std::vector<double>(audio.samples.begin(), audio.samples.end());

    // 1. Rewrite gendered self-references toward the target gender.
    std::vector<Segment> rewritten;
    for (auto seg : segments) {
        bool changed = false;
        const std::string rw =
            rewrite::rewrite_for_gender(seg.text, target, changed);
        if (changed) {
            seg.text = rw;
            seg.has_pii = true;
            rewritten.push_back(seg);
        }
        res.segments.push_back(seg);
    }

    // 2. Global target-gender shift of the whole recording.
    res.audio = dsp::shift_audio(res.audio, audio.sample_rate, shift);

    // 3. Re-voice the rewritten segments with TTS in the target voice.
    if (!rewritten.empty() && tts::available()) {
        for (const auto& seg : rewritten) {
            AudioData spoken;
            std::string terr;
            if (!tts::synthesize(seg.text, target, spoken, terr)) continue;
            std::vector<double> sp(spoken.samples.begin(), spoken.samples.end());
            sp = resample_to(sp, spoken.sample_rate, audio.sample_rate);
            sp = dsp::shift_audio(sp, audio.sample_rate, shift);
            const size_t s = static_cast<size_t>(seg.start * audio.sample_rate);
            const size_t e = static_cast<size_t>(seg.end * audio.sample_rate);
            res.audio = replace_region(res.audio, sp, audio.sample_rate, s, e);
        }
    }
    return res;
}

}  // namespace

JobResult run(const JobOptions& opts) {
    JobResult res;

    AudioData audio;
    {
        std::string err;
        audio = audio::read_file(opts.input, &err);
        if (audio.samples.empty()) {
            res.message = "cannot read '" + opts.input + "': " +
                          (err.empty() ? "empty audio" : err);
            return res;
        }
    }

    transcribe::WhisperConfig wcfg;
    wcfg.binary = opts.whisper_binary;
    wcfg.model = opts.whisper_model;
    wcfg.language = opts.whisper_language;

    const bool need_transcript = opts.transcribe || opts.write_transcript ||
                                 opts.mode == dsp::AnonMode::Selective ||
                                 opts.mode == dsp::AnonMode::Hybrid;
    std::vector<Segment> segments;
    if (need_transcript) {
        std::string err;
        if (!transcribe::transcribe(opts.input, wcfg, segments, err)) {
            res.message = err;
            return res;
        }
    }

    redact::RedactOptions ropts;
    ropts.replacement = opts.name_replacement;
    ropts.use_synthetic_names = opts.synthetic_names;

    std::vector<double> processed;
    if (opts.mode == dsp::AnonMode::Hybrid) {
        const auto hr = run_hybrid(audio, opts.target_gender, opts.shift,
                                   segments);
        segments = hr.segments;
        processed = hr.audio;
    }
    res.flagged = redact::flag_segments(segments, ropts);
    res.transcript = segments;

    if (opts.write_transcript && !opts.output.empty()) {
        res.transcript_path = opts.output + ".txt";
        write_transcript(res.transcript_path, segments);
    }

    if (!opts.output.empty()) {
        if (opts.mode != dsp::AnonMode::Hybrid) {
            std::vector<double> in_d(audio.samples.begin(), audio.samples.end());
            if (opts.mode == dsp::AnonMode::Selective) {
                processed = dsp::process_selective(in_d, audio.sample_rate,
                                                   segments, opts.shift);
            } else {
                processed = dsp::process_global(in_d, audio.sample_rate,
                                                opts.shift);
            }
        }
        AudioData out;
        out.sample_rate = audio.sample_rate;
        out.samples.reserve(processed.size());
        for (double v : processed) out.samples.push_back(static_cast<Sample>(v));
        std::string err;
        if (!audio::write_file(opts.output, out, &err)) {
            res.message = "cannot write '" + opts.output + "': " + err;
            return res;
        }
        res.redacted_path = opts.output;
    }

    res.ok = true;
    res.message = "ok";
    return res;
}

}  // namespace ocs::job
