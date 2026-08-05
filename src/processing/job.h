// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 voice_anonymizer contributors
#pragma once

#include <string>
#include <vector>

#include "common.h"
#include "dsp/pipeline.h"

namespace va::job {

struct JobOptions {
    std::string input;
    std::string output;
    dsp::AnonMode mode = dsp::AnonMode::Global;
    dsp::ShiftParams shift;
    bool transcribe = false;
    bool write_transcript = false;   // write <output>.txt with redacted text
    std::string whisper_binary;      // optional override
    std::string whisper_model;
    std::string whisper_language;
    std::string name_replacement = "[REDACTED]";
    bool synthetic_names = false;
};

struct JobResult {
    bool ok = false;
    std::string message;
    std::vector<Segment> transcript;
    std::string transcript_path;
    std::string redacted_path;
    size_t flagged = 0;
};

// Runs the batch anonymization pipeline:
//   decode -> (transcribe -> redact) -> shift -> encode
JobResult run(const JobOptions& opts);

}  // namespace va::job
