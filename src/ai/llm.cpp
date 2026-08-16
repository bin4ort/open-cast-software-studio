// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "../common.h"
#include "ai/llm.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>

#include <fcntl.h>
#include <unistd.h>

#include "util/json.h"

namespace ocs::llm {

namespace {

Config g_cfg;              // set via set_config()/config_from_env()
bool g_cfg_set = false;

// Probe results are cached briefly so llm_available()/backend_name() do not
// spawn curl processes on every call (the GUI refreshes status often).
long g_probe_ms = 0;               // steady_clock ms of the last probe
std::string g_found_ollama;        // base URL answering /api/tags ("ollama")
std::string g_found_openai;        // base URL answering /v1/models ("llama.cpp")
constexpr long kProbeTtlMs = 2000;

long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::string run_capture(const std::string& cmd) {
    std::string out;
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return out;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), p)) > 0) out.append(buf, n);
    pclose(p);
    return out;
}

// curl must be present to talk to the local LLM.
bool curl_available() {
    static const bool ok = [] {
        FILE* p = popen("curl --version >/dev/null 2>&1", "r");
        if (!p) return false;
        const int rc = pclose(p);
        return rc == 0;
    }();
    return ok;
}

// Strips a trailing '/' so configured URLs combine cleanly with path parts.
std::string normalize_url(const std::string& url) {
    std::string u = url;
    while (u.size() > 1 && u.back() == '/') u.pop_back();
    return u;
}

std::string get_url(const std::string& url, const std::string& path) {
    if (url.empty() || !curl_available()) return "";
    return run_capture("curl -s --connect-timeout 1 --max-time 2 " +
                       ocs::shell_q(url + path) + " 2>/dev/null");
}

// Determines which protocol a base URL speaks. Some servers (Ollama, and
// non-standard builds) answer /v1/models with a {"models":...} list instead
// of the OpenAI {"data":...} shape, so both markers are accepted.
// Returns "" (nothing), "ollama" (/api/tags) or "openai" (/v1/models).
std::string probe_base(const std::string& base) {
    const std::string tags = get_url(base, "/api/tags");
    if (tags.find("\"models\"") != std::string::npos) return "ollama";
    const std::string models = get_url(base, "/v1/models");
    if (models.find("\"data\"") != std::string::npos ||
        models.find("\"models\"") != std::string::npos) {
        return "openai";
    }
    return "";
}

std::string found_backend() {
    if (!g_found_openai.empty()) return "llama.cpp";
    if (!g_found_ollama.empty()) return "ollama";
    return "";
}

std::string found_url() {
    if (!g_found_openai.empty()) return g_found_openai;
    return g_found_ollama;
}

// Re-probes both configured endpoints (respecting the TTL) and caches the
// result. Both URLs are checked against both protocols, so a llama.cpp
// server on the "Ollama" URL (or vice versa) is still found.
std::string refresh_probe() {
    if (!g_cfg_set) return found_backend();  // not configured yet
    const long t = now_ms();
    if (t - g_probe_ms < kProbeTtlMs) return found_backend();
    g_probe_ms = t;
    g_found_ollama.clear();
    g_found_openai.clear();
    const std::string ollama = normalize_url(g_cfg.ollama_url);
    const std::string llama = normalize_url(g_cfg.llama_url);
    for (const std::string& base : {ollama, llama}) {
        if (base.empty()) continue;
        const std::string proto = probe_base(base);
        if (proto == "ollama" && g_found_ollama.empty()) {
            g_found_ollama = base;
        } else if (proto == "openai" && g_found_openai.empty()) {
            g_found_openai = base;
        }
    }
    return found_backend();
}

// Detect a usable Ollama model name (any pulled model).
std::string ollama_model() {
    const std::string url = g_found_ollama;
    if (url.empty() || !curl_available()) return "";
    const std::string r = get_url(url, "/api/tags");
    try {
        const util::Json root = util::Json::parse(r);
        for (const auto& m : root.arr("models")) {
            const std::string name = m.str("name", "");
            if (!name.empty() && name.find(':') != std::string::npos) {
                return name;
            }
        }
    } catch (...) {
    }
    return "";
}

// POSTs a JSON payload file via curl and returns the raw response.
std::string post_json(const std::string& url, const std::string& payload,
                      const std::string& path) {
    {
        std::ofstream f(path, std::ios::trunc);
        if (!f) return "";
        f << payload;
    }
    const std::string cmd =
        "curl -s --connect-timeout 3 --max-time 60 -H "
        "\"Content-Type: application/json\" --data-binary @" +
        ocs::shell_q(path) + " " + ocs::shell_q(url) + " 2>/dev/null";
    std::string out = run_capture(cmd);
    (void)std::remove(path.c_str()); /* temp cleanup, best effort */
    return out;
}

std::string parse_ollama_response(const std::string& raw) {
    try {
        const util::Json root = util::Json::parse(raw);
        const std::string response = root.str("response", "");
        if (!response.empty()) return response;
        // Error: model not found, etc.
        return "";
    } catch (...) {
        return "";
    }
}

// Minimal JSON string escaping for prompt payloads.
std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string parse_llama_response(const std::string& raw) {
    try {
        const util::Json root = util::Json::parse(raw);
        const auto& choices = root.arr("choices");
        if (!choices.empty()) {
            const std::string content = choices[0].at("message").str("content", "");
            return content;
        }
        return "";
    } catch (...) {
        return "";
    }
}

}  // namespace

void set_config(const Config& cfg) {
    g_cfg = cfg;
    g_cfg.ollama_url = normalize_url(g_cfg.ollama_url);
    g_cfg.llama_url = normalize_url(g_cfg.llama_url);
    g_cfg_set = true;
    g_probe_ms = 0;  // force a fresh probe with the new settings
}

const Config& config() {
    return g_cfg;
}

Config config_from_env() {
    Config c;
    if (const char* v = std::getenv("OCS_OLLAMA_URL"); v && *v) c.ollama_url = v;
    if (const char* v = std::getenv("OCS_LLAMA_URL"); v && *v) c.llama_url = v;
    if (const char* v = std::getenv("OCS_AI_LAUNCH_CMD"); v && *v) c.launch_command = v;
    if (const char* v = std::getenv("OCS_AI_LAUNCH_DIR"); v && *v) c.launch_dir = v;
    if (const char* v = std::getenv("OCS_AI_AUTO_LAUNCH"); v && *v) {
        c.auto_launch = std::string(v) == "1" || std::string(v) == "true" ||
                        std::string(v) == "yes";
    }
    return c;
}

bool launch_backend() {
    if (g_cfg.launch_command.empty()) return false;
    const pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        // Child: detach and run the launcher with output logged to a
        // private per-user file. Every step is checked: a broken child
        // must not silently exec with the wrong setup (debugging that
        // later costs hours).
        if (setsid() < 0) _exit(126);
        if (!g_cfg.launch_dir.empty() && chdir(g_cfg.launch_dir.c_str()) != 0)
            _exit(125);
        const std::string log_dir = ocs_tmp_dir();
        const std::string log_path =
            log_dir.empty() ? "/dev/null"
                            : log_dir + "/launch.log";
        const int fd = open(log_path.c_str(),
                            O_WRONLY | O_CREAT | O_APPEND, 0600);
        if (fd >= 0) {
            if (dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0) {
                (void)close(fd);
                _exit(124);
            }
            (void)close(fd); /* fd cleanup before exec; exec never returns */
        }
        execl("/bin/sh", "sh", "-c", g_cfg.launch_command.c_str(),
              static_cast<char*>(nullptr));
        _exit(127);
    }
    return true;
}

bool llm_available() {
    if (!g_cfg_set) {
        // Fall back to environment defaults (CLI never called set_config).
        set_config(config_from_env());
    }
    return !refresh_probe().empty();
}

std::string backend_name() {
    if (!g_cfg_set) {
        set_config(config_from_env());
    }
    return refresh_probe();
}

std::string status_text() {
    if (!g_cfg_set) {
        set_config(config_from_env());
    }
    const std::string found = refresh_probe();
    const std::string ollama = normalize_url(g_cfg.ollama_url);
    const std::string llama = normalize_url(g_cfg.llama_url);
    std::string out;
    if (!found.empty()) {
        out = found + " reachable (" + found_url() + ")";
    } else {
        out = "no LLM reachable (ollama: " + ollama +
              ", llama.cpp: " + llama + ")";
        if (!g_cfg.launch_command.empty()) {
            out += "; auto-launch " +
                   std::string(g_cfg.auto_launch ? "enabled" : "disabled") +
                   " (\"" + g_cfg.launch_command + "\")";
        }
    }
    return out;
}

void test_connection_reset() {
    g_probe_ms = 0;
}

std::string complete(const std::string& system_prompt,
                     const std::string& user_prompt) {
    if (!llm_available()) {
        // Nothing detected: auto-launch the configured backend once and give
        // it a few seconds to come up before giving up.
        if (!g_cfg.auto_launch || g_cfg.launch_command.empty()) return "";
        if (!launch_backend()) return "";
        for (int i = 0; i < 25; ++i) {  // up to ~5 s
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            g_probe_ms = 0;
            if (!refresh_probe().empty()) break;
        }
        if (!llm_available()) return "";
    }

    const std::string path = ocs::tmp_payload_path();
    if (!g_found_openai.empty()) {
        std::string payload =
            R"({"model":"local","stream":false,"messages":[{"role":"system","content":")" +
            json_escape(system_prompt) + R"("},{"role":"user","content":")" +
            json_escape(user_prompt) + "\"}]}";
        return parse_llama_response(post_json(
            g_found_openai + "/v1/chat/completions", payload, path));
    }
    if (!g_found_ollama.empty()) {
        std::string model = ollama_model();
        if (model.empty()) return "";
        std::string payload =
            R"({"model":")" + json_escape(model) +
            R"(","stream":false,"system":")" + json_escape(system_prompt) +
            R"(","prompt":")" + json_escape(user_prompt) + "\"}";
        return parse_ollama_response(post_json(
            g_found_ollama + "/api/generate", payload, path));
    }
    return "";
}

bool translate_prompt(const std::string& description, dsp::ShiftParams& out) {
    const std::string sys =
        "You map a natural-language voice description to DSP parameters. "
        "Respond with JSON ONLY, no prose, in this exact shape: "
        "{\"pitch\": <semitones -24..24>, \"formant\": <semitones -24..24>}.";
    const std::string user = "Voice description: " + description;
    const std::string resp = complete(sys, user);
    if (resp.empty()) return false;
    try {
        const util::Json j = util::Json::parse(resp);
        out.pitch_semitones = j.num("pitch", 0.0);
        out.formant_semitones = j.num("formant", 0.0);
        return true;
    } catch (...) {
        return false;
    }
}

std::string rewrite_phrase(const std::string& text, const std::string& target) {
    const std::string sys =
        "You rewrite gendered self-referential phrases so the speaker reads as "
        "the requested gender. Keep everything else identical, preserve "
        "meaning and tone. Return ONLY the rewritten sentence.";
    const std::string user = "Make the speaker " + target + ". Text: " + text;
    return complete(sys, user);
}

}  // namespace ocs::llm
