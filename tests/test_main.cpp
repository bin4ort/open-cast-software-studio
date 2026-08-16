// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "test_util.h"

#include <cstdio>

// Test registration is done via static initializers in the included files.
#include "compare_test.cpp"
#include "correction_test.cpp"
#include "gender_test.cpp"
#include "json_test.cpp"
#include "llm_config_test.cpp"
#include "redactor_test.cpp"
#include "rewriter_test.cpp"
#include "ringbuf_test.cpp"
#include "stft_test.cpp"
#include "vad_test.cpp"
#include "vocoder_test.cpp"

int main() {
    std::printf("OCS Studio unit tests\n");
    int ran = 0;
    for (const auto& t : ocs_test::registry()) {
        std::printf("  - %s\n", t.name);
        ++ran;
        t.fn();
    }
    const int failed = ocs_test::failure_count();
    std::printf("%d/%d tests, %d failures\n", ran - failed, ran, failed);
    return failed == 0 ? 0 : 1;
}
