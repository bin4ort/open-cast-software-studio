// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "ai/vc.h"

#include <cstdlib>

namespace ocs::vc {

namespace {

#ifdef OCS_NEURAL_VC
bool model_configured() {
    const char* m = std::getenv("OCS_VC_MODEL");
    return m != nullptr && *m != '\0';
}
#endif

}  // namespace

void Converter::load(const std::string& model_path) {
#ifdef OCS_NEURAL_VC
    // Real RVC inference lives behind this flag. It requires an onnxruntime
    // build plus a converted RVC model (HuBERT encoder + VITS decoder). Until
    // a model is provided the converter stays unloaded.
    if (model_path.empty()) {
        error = "no OCS_VC_MODEL configured";
        return;
    }
    // TODO: onnxruntime session creation + HuBERT/VITS forward pass.
    error = "OCS_NEURAL_VC build present but inference not wired yet";
#else
    (void)model_path;
    error = "neural VC not built (configure with -DOCS_NEURAL_VC=ON)";
#endif
}

void Converter::process(std::vector<float>& samples) {
    (void)samples;  // no-op until a model is loaded
}

bool available() {
#ifdef OCS_NEURAL_VC
    return model_configured();
#else
    return false;
#endif
}

std::string status() {
#ifdef OCS_NEURAL_VC
    if (model_configured()) {
        return "Neural VC built and model configured (experimental)";
    }
    return "Neural VC built but no model (set OCS_VC_MODEL)";
#else
    return "Neural VC not built (experimental; -DOCS_NEURAL_VC=ON + onnxruntime)";
#endif
}

}  // namespace ocs::vc
