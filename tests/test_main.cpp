// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 voice_anonymizer contributors
#include "test_util.h"

#include <cstdio>

// Test registration is done via static initializers in the included files.
#include "redactor_test.cpp"
#include "stft_test.cpp"
#include "vocoder_test.cpp"

int main() {
    std::printf("voice_anonymizer unit tests\n");
    int ran = 0;
    for (const auto& t : va_test::registry()) {
        std::printf("  - %s\n", t.name);
        ++ran;
        t.fn();
    }
    const int failed = va_test::failure_count();
    std::printf("%d/%d tests, %d failures\n", ran - failed, ran, failed);
    return failed == 0 ? 0 : 1;
}
