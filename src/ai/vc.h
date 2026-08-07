// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <string>
#include <vector>

namespace ocs::vc {

// Experimental neural voice conversion (RVC-style via onnxruntime).
//
// This is opt-in and experimental: it is only functional when the project is
// built with OCS_NEURAL_VC=ON and an OCS_VC_MODEL path to a converted RVC
// model is provided. Otherwise it degrades gracefully to "not available".
struct Converter {
    bool loaded = false;
    std::string error;

    // Loads the model; sets `loaded` and `error` accordingly.
    void load(const std::string& model_path);
    // Processes a block of mono float samples in place.
    void process(std::vector<float>& samples);
};

// True if a neural converter is configured and available.
bool available();
// Human-readable status for the UI/CLI.
std::string status();

}  // namespace ocs::vc
