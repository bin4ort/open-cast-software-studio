// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "test_util.h"

#include "ai/rewriter.h"
#include "dsp/gender.h"

using ocs::dsp::Gender;

TEST(rewrite_male_self_ident) {
    bool changed = false;
    const std::string out =
        ocs::rewrite::rewrite_for_gender("I am a woman, and I am proud.",
                                         Gender::Male, changed);
    CHECK(changed);
    CHECK(out.find("I am a man") != std::string::npos);
}

TEST(rewrite_female_self_ident) {
    bool changed = false;
    const std::string out =
        ocs::rewrite::rewrite_for_gender("I am a man from New York.",
                                         Gender::Female, changed);
    CHECK(changed);
    CHECK(out.find("I am a woman") != std::string::npos);
}

TEST(rewrite_case_insensitive) {
    bool changed = false;
    const std::string out =
        ocs::rewrite::rewrite_for_gender("MY MOTHER lives here.",
                                         Gender::Male, changed);
    CHECK(changed);
    CHECK(out.find("MY MOTHER") == std::string::npos);
    CHECK(out.find("father") != std::string::npos);
}

TEST(rewrite_spouse_swap) {
    bool changed = false;
    const std::string out =
        ocs::rewrite::rewrite_for_gender("I love my wife very much.",
                                         Gender::Female, changed);
    CHECK(changed);
    CHECK(out.find("my wife") == std::string::npos);
    CHECK(out.find("my husband") != std::string::npos);
}

TEST(rewrite_generic_noop) {
    bool changed = false;
    const std::string out =
        ocs::rewrite::rewrite_for_gender("I am a woman.",
                                         Gender::Generic, changed);
    CHECK(!changed);
    CHECK(out.find("woman") != std::string::npos);
}

TEST(rewrite_keeps_unrelated) {
    bool changed = false;
    const std::string out =
        ocs::rewrite::rewrite_for_gender("The weather is nice today.",
                                         Gender::Male, changed);
    CHECK(!changed);
    CHECK(out == "The weather is nice today.");
}
