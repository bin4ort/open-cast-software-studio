// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "transcribe/whisper.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "util/json.h"

namespace ocs::transcribe {

namespace {

bool executable(const std::filesystem::path& p) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(p, ec)) return false;
    const auto perm = std::filesystem::status(p, ec).permissions();
    return (perm & std::filesystem::perms::owner_exec) !=
           std::filesystem::perms::none;
}

}  // namespace

std::string find_whisper_binary() {
    const char* path = std::getenv("PATH");
    if (!path) return {};
    const std::string path_env(path);
    std::stringstream ss(path_env);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        if (dir.empty()) continue;
        for (const char* name : {"whisper-cli", "whisper", "main"}) {
            const auto p = std::filesystem::path(dir) / name;
            if (executable(p)) return p.string();
        }
    }
    return {};
}

std::string find_whisper_model() {
    if (const char* m = std::getenv("OCS_WHISPER_MODEL"); m && *m) {
        if (std::filesystem::exists(m)) return m;
    }
    const char* home = std::getenv("HOME");
    if (!home) return {};
    for (const auto& sub : {"/.local/share/ocs_studio/models",
                            "/.config/ocs_studio/models"}) {
        const auto dir = std::filesystem::path(home) / sub;
        std::error_code ec;
        if (!std::filesystem::is_directory(dir, ec)) continue;
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (entry.path().extension() == ".bin") return entry.path().string();
        }
    }
    return {};
}

bool transcribe(const std::string& input_path, const WhisperConfig& cfg,
                std::vector<Segment>& segments, std::string& err) {
    segments.clear();

    std::string binary = cfg.binary;
    if (binary.empty()) binary = find_whisper_binary();
    if (binary.empty()) {
        err = "no whisper binary found. Install whisper.cpp or openai/whisper, "
              "or pass --whisper <binary>.";
        return false;
    }

    const auto base = std::filesystem::path(input_path).filename().string();
    const auto outbase =
        (std::filesystem::temp_directory_path() / ("ocs_whisper_" + base)).string();
    const auto outjson = outbase + ".json";

    const std::filesystem::path bin_path(binary);
    const std::string bin_name = bin_path.filename().string();
    const bool is_cpp_cli =
        bin_name == "whisper-cli" || bin_name == "main" ||
        bin_name == "whisper-cpp";

    std::string cmd;
    if (is_cpp_cli) {
        cmd = "\"" + binary + "\" -f \"" + input_path + "\" -oj -of \"" +
              outbase + "\"";
        if (!cfg.model.empty()) cmd += " -m \"" + cfg.model + "\"";
        if (!cfg.language.empty()) cmd += " -l " + cfg.language;
    } else {
        cmd = "\"" + binary + "\" \"" + input_path +
              "\" --output_format json --output_dir \"" +
              std::filesystem::temp_directory_path().string() + "\"";
        if (!cfg.language.empty()) cmd += " --language " + cfg.language;
    }
    cmd += " 2>/dev/null";

    const int rc = std::system(cmd.c_str());
    if (rc != 0) {
        err = "whisper failed (exit " + std::to_string(rc) + ")";
        return false;
    }

    // Locate the JSON produced (whisper.cpp names it by `-of`, python by the
    // input basename in --output_dir).
    std::vector<std::string> candidates = {outjson};
    const auto py_json =
        (std::filesystem::temp_directory_path() /
         (std::filesystem::path(input_path).stem().string() + ".json"))
            .string();
    candidates.push_back(py_json);

    std::string text;
    for (const auto& c : candidates) {
        if (std::filesystem::exists(c)) {
            text = [&] {
                std::ifstream f(c);
                std::ostringstream ss;
                ss << f.rdbuf();
                return ss.str();
            }();
            break;
        }
    }
    std::filesystem::remove(outjson);
    if (text.empty()) {
        err = "whisper produced no JSON output";
        return false;
    }

    try {
        const util::Json root = util::Json::parse(text);
        if (root.contains("segments") && root.at("segments").is_array()) {
            // openai/whisper style: segments[] with start/end in seconds.
            for (const auto& s : root.at("segments").as_array()) {
                Segment seg;
                seg.start = s.num("start", 0.0);
                seg.end = s.num("end", seg.start + 1.0);
                seg.text = s.str("text", "");
                if (seg.end <= seg.start) continue;
                segments.push_back(std::move(seg));
            }
            return !segments.empty();
        }
        // whisper.cpp style: transcription[] with offsets (milliseconds).
        if (root.contains("transcription") && root.at("transcription").is_array()) {
            for (const auto& s : root.at("transcription").as_array()) {
                Segment seg;
                if (s.contains("offsets") && s.at("offsets").is_object()) {
                    const auto& off = s.at("offsets");
                    const double from = off.num("from", -1.0);
                    const double to = off.num("to", -1.0);
                    // whisper.cpp reports offsets in milliseconds.
                    seg.start = from / 1000.0;
                    seg.end = to / 1000.0;
                }
                seg.text = s.str("text", "");
                if (seg.end <= seg.start) continue;
                segments.push_back(std::move(seg));
            }
            return !segments.empty();
        }
        err = "unexpected whisper JSON (no segments/transcription array)";
        return false;
    } catch (const std::exception& e) {
        err = std::string("failed to parse whisper JSON: ") + e.what();
        return false;
    }
    return !segments.empty();
}

}  // namespace ocs::transcribe
