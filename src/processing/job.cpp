// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 voice_anonymizer contributors
#include "processing/job.h"

#include <cstdio>
#include <fstream>

#include "anonymize/redactor.h"
#include "audio/fileio.h"
#include "dsp/pipeline.h"
#include "transcribe/whisper.h"

namespace va::job {

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
                                 opts.mode == dsp::AnonMode::Selective;
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
    res.flagged = redact::flag_segments(segments, ropts);
    res.transcript = segments;

    if (opts.write_transcript && !opts.output.empty()) {
        res.transcript_path = opts.output + ".txt";
        write_transcript(res.transcript_path, segments);
    }

    if (!opts.output.empty()) {
        std::vector<double> in_d(audio.samples.begin(), audio.samples.end());
        std::vector<double> processed;
        if (opts.mode == dsp::AnonMode::Selective) {
            processed = dsp::process_selective(in_d, audio.sample_rate,
                                               segments, opts.shift);
        } else {
            processed = dsp::process_global(in_d, audio.sample_rate,
                                            opts.shift);
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

}  // namespace va::job
