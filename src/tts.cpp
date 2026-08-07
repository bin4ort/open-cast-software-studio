// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "tts.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#ifdef OCS_HAVE_FFMPEG
#include <unistd.h>
#endif

#include "audio/fileio.h"

namespace ocs::tts {

namespace {

std::string shell_quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\' || c == '$' || c == '`') out += '\\';
        out += c;
    }
    out += '"';
    return out;
}

std::string find_in_path(const char* name) {
    const char* path = std::getenv("PATH");
    if (!path) return {};
    std::string cur;
    const std::string path_env(path);
    for (size_t i = 0; i <= path_env.size(); ++i) {
        if (i == path_env.size() || path_env[i] == ':') {
            const auto p = std::filesystem::path(cur) / name;
            std::error_code ec;
            if (std::filesystem::is_regular_file(p, ec) &&
                (std::filesystem::status(p, ec).permissions() &
                 std::filesystem::perms::owner_exec) !=
                    std::filesystem::perms::none) {
                return p.string();
            }
            cur.clear();
        } else {
            cur += path_env[i];
        }
    }
    return {};
}

std::string voice_for(dsp::Gender g) {
    switch (g) {
        case dsp::Gender::Male: return "en-us+m3";
        case dsp::Gender::Female: return "en-us+f3";
        case dsp::Gender::Custom:
        case dsp::Gender::Generic:
        default: return "en-us";
    }
}

}  // namespace

bool available() {
    return !find_in_path("espeak-ng").empty();
}

bool synthesize(const std::string& text, dsp::Gender gender,
                AudioData& out, std::string& err) {
    const std::string bin = find_in_path("espeak-ng");
    if (bin.empty()) {
        err = "espeak-ng not found. Install it to enable voice preview "
              "(e.g. 'pacman -S espeak-ng').";
        return false;
    }
    if (text.empty()) {
        err = "no text to synthesize";
        return false;
    }

    std::filesystem::path tmp;
    try {
        const auto dir = std::filesystem::temp_directory_path();
        std::string tmpl = (dir / "ocs_tts_XXXXXX.wav").string();
        std::vector<char> buf(tmpl.begin(), tmpl.end());
        buf.push_back('\0');
#ifdef OCS_HAVE_FFMPEG
        const int fd = mkstemps(buf.data(), 4);
        if (fd >= 0) close(fd);
#endif
        tmp = buf.data();
    } catch (...) {
        err = "failed to create temp file";
        return false;
    }

    const std::string cmd =
        shell_quote(bin) + " -v " + voice_for(gender) + " -s 175 -w " +
        shell_quote(tmp.string()) + " " + shell_quote(text) + " 2>/dev/null";
    if (std::system(cmd.c_str()) != 0) {
        err = "espeak-ng failed to synthesize";
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        return false;
    }

    std::string rerr;
    out = audio::read_file(tmp.string(), &rerr);
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
    if (out.samples.empty()) {
        err = "espeak-ng produced no audio" +
              (rerr.empty() ? std::string() : ": " + rerr);
        return false;
    }
    return true;
}

}  // namespace ocs::tts
