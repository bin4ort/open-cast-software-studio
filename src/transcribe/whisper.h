// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <string>
#include <vector>

#include "common.h"

namespace ocs::transcribe {

struct WhisperConfig {
    std::string binary;    // whisper-cli / whisper / main, or a full path
    std::string model;     // optional (whisper.cpp -m)
    std::string language;  // optional (e.g. "en")
};

// Locates a usable transcription binary on PATH.
std::string find_whisper_binary();

// Locates a whisper model: $OCS_WHISPER_MODEL, else a *.bin under
// ~/.local/share/ocs_studio/models or ~/.config/ocs_studio/models.
std::string find_whisper_model();

// Transcribes `input_path` to timestamped segments. Returns false and sets
// `err` on any failure (binary missing, decode/run error, no output).
bool transcribe(const std::string& input_path, const WhisperConfig& cfg,
                std::vector<Segment>& segments, std::string& err);

}  // namespace ocs::transcribe
