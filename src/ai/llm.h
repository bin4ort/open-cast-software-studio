// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <string>

#include "dsp/pipeline.h"

namespace ocs::llm {

// True if a local LLM endpoint is reachable (Ollama or llama.cpp server).
bool llm_available();

// Returns "ollama" or "llama.cpp" depending on what was detected, "" if none.
std::string backend_name();

// Completes a prompt via the local LLM. Returns the assistant text, or an
// empty string on failure / no backend.
std::string complete(const std::string& system_prompt,
                     const std::string& user_prompt);

// Asks the LLM to map a natural-language voice description to DSP settings.
// Returns false on failure; on success fills `out`.
bool translate_prompt(const std::string& description, dsp::ShiftParams& out);

// Asks the LLM to rewrite gendered self-references in `text` toward `target`.
// Returns the rewritten text, or empty on failure (caller falls back to the
// rules engine).
std::string rewrite_phrase(const std::string& text, const std::string& target);

}  // namespace ocs::llm
