// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <string>

#include "dsp/pipeline.h"

namespace ocs::llm {

// Connection and launch settings for the local AI backends.
struct Config {
    // Base URLs (http://host:port). Ollama speaks the /api/* protocol,
    // llama.cpp speaks the OpenAI-compatible /v1/* protocol.
    std::string ollama_url = "http://localhost:11434";
    std::string llama_url = "http://localhost:8080";

    // Optional backend launcher (e.g. "ollama serve" or
    // "/path/to/llama-server -m model.gguf"), executed in `launch_dir`.
    std::string launch_command;
    std::string launch_dir;
    // When true, the backend is launched automatically the first time a
    // request is made and nothing is listening yet.
    bool auto_launch = false;
};

// Applies the given configuration (call before using any other function).
void set_config(const Config& cfg);
const Config& config();

// Builds a Config from the environment (OCS_OLLAMA_URL, OCS_LLAMA_URL,
// OCS_AI_LAUNCH_CMD, OCS_AI_LAUNCH_DIR, OCS_AI_AUTO_LAUNCH). The CLI uses
// this at startup; the GUI applies its settings-tab values instead.
Config config_from_env();

// True if a local LLM endpoint is reachable (Ollama or llama.cpp server).
bool llm_available();

// Starts the configured launcher command in the background (working dir =
// config.launch_dir, output appended to /tmp/ocs_ai_launch.log). Returns
// false when no launcher is configured or the process could not be started.
bool launch_backend();

// Returns "ollama" or "llama.cpp" depending on what was detected, "" if none.
std::string backend_name();

// Human-readable connectivity status, including the configured endpoints.
std::string status_text();

// Invalidates the probe cache so the next llm_available()/status_text() call
// re-probes the endpoints (call after launching a backend).
void test_connection_reset();

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
