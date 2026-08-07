// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "ai/llm.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "util/json.h"

namespace ocs::llm {

namespace {

const char* kOllamaUrl = "http://localhost:11434";
const char* kLlamaUrl = "http://localhost:8080";

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

std::string probe_ollama() {
    const std::string r = run_capture(
        "curl -s --max-time 2 http://localhost:11434/api/tags 2>/dev/null");
    return r.find("\"models\"") != std::string::npos ? kOllamaUrl : "";
}

std::string probe_llama() {
    const std::string r = run_capture(
        "curl -s --max-time 2 http://localhost:8080/v1/models 2>/dev/null");
    return r.find("data") != std::string::npos ? kLlamaUrl : "";
}

// Detect a usable Ollama model name (any pulled model).
std::string ollama_model() {
    const std::string r = run_capture(
        "curl -s --max-time 2 http://localhost:11434/api/tags 2>/dev/null");
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
    std::ofstream f(path, std::ios::trunc);
    f << payload;
    f.close();
    const std::string cmd =
        "curl -s --max-time 120 -H \"Content-Type: application/json\" "
        "--data-binary @" + path + " " + url + " 2>/dev/null";
    std::string out = run_capture(cmd);
    std::remove(path.c_str());
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

bool llm_available() {
    return curl_available() && (!probe_ollama().empty() || !probe_llama().empty());
}

std::string backend_name() {
    if (!curl_available()) return "";
    if (!probe_ollama().empty()) return "ollama";
    if (!probe_llama().empty()) return "llama.cpp";
    return "";
}

std::string complete(const std::string& system_prompt,
                     const std::string& user_prompt) {
    if (!llm_available()) return "";

    const std::string path = "/tmp/ocs_llm_payload.json";
    const std::string ollama = probe_ollama();
    if (!ollama.empty()) {
        std::string model = ollama_model();
        if (model.empty()) return "";
        std::string payload =
            R"({"model":")" + json_escape(model) +
            R"(","stream":false,"system":")" + json_escape(system_prompt) +
            R"(","prompt":")" + json_escape(user_prompt) + "\"}";
        return parse_ollama_response(post_json(ollama + "/api/generate", payload, path));
    }
    const std::string llama = probe_llama();
    if (!llama.empty()) {
        std::string payload =
            R"({"model":"local","stream":false,"messages":[{"role":"system","content":")" +
            json_escape(system_prompt) + R"("},{"role":"user","content":")" +
            json_escape(user_prompt) + "\"}]}";
        return parse_llama_response(post_json(llama + "/v1/chat/completions", payload, path));
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
