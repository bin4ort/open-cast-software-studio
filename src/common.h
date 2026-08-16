// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <cstdint>
#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <vector>

namespace ocs {

// Single-quote a string for POSIX shells: ' -> '\'' . Prevents command
// injection when user-controlled values reach system()/popen().
inline std::string shell_q(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('\'');
    for (char c : s) {
        if (c == '\'')
            out += "'\\''";
        else
            out.push_back(c);
    }
    out.push_back('\'');
    return out;
}

// Per-user, private scratch directory for FIFOs / logs / payloads. Never
// a world-writable fixed /tmp path (symlink and eavesdropping attacks).
// Returns "" on failure.
inline std::string ocs_tmp_dir() {
    const char* rt = std::getenv("XDG_RUNTIME_DIR");
    std::string base = (rt && rt[0]) ? std::string(rt)
                                     : "/tmp/ocs-" + std::to_string(::getuid());
    std::string dir = base + "/ocs";
    if (::mkdir(dir.c_str(), 0700) != 0 && errno != EEXIST)
        return "";
    return dir;
}

// A private per-process payload path (0700 dir, pid-unique name).
inline std::string tmp_payload_path() {
    const std::string dir = ocs_tmp_dir();
    if (dir.empty()) return "";
    return dir + "/llm-" + std::to_string(::getpid()) + ".json";
}

// Mono, float (-1.0..1.0) PCM sample.
using Sample = float;

// A fully-decoded mono audio buffer plus its metadata.
struct AudioData {
    std::vector<Sample> samples;
    int sample_rate = 0;
};

// A speech/text segment with a temporal span (seconds).
struct Segment {
    double start = 0.0;
    double end = 0.0;
    std::string text;
    bool has_pii = false;  // flagged by the redactor
};

}  // namespace ocs
