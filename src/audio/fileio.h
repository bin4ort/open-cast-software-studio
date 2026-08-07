// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <string>

#include "common.h"

namespace ocs::audio {

// Reads any file into mono PCM. Uses libsndfile natively; falls back to an
// ffmpeg subprocess for formats sndfile does not handle (mp3, m4a, ...).
// Returns empty AudioData and sets *err on failure.
AudioData read_file(const std::string& path, std::string* err = nullptr);

// Writes mono PCM. WAV/FLAC/OGG go through libsndfile; anything else is
// encoded via ffmpeg.
bool write_file(const std::string& path, const AudioData& data,
                std::string* err = nullptr);

bool sndfile_can_handle(const std::string& path);
bool ffmpeg_available();

}  // namespace ocs::audio
