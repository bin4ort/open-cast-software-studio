// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "test_util.h"

#include <string>
#include <vector>

#include "anonymize/redactor.h"
#include "common.h"

using ocs::redact::RedactOptions;
using ocs::redact::redact_text;
using ocs::redact::flag_segments;
using ocs::redact::synthetic_name;

TEST(redact_email) {
    std::vector<std::string> names;
    const std::string out = redact_text("reach me at john.doe@example.com now",
                                        RedactOptions{}, names);
    CHECK(out.find("john.doe@example.com") == std::string::npos);
    CHECK(out.find("[REDACTED]") != std::string::npos);
}

TEST(redact_url) {
    std::vector<std::string> names;
    const std::string out =
        redact_text("see https://example.com/a?b=1 for details", RedactOptions{}, names);
    CHECK(out.find("https://example.com") == std::string::npos);
    CHECK(out.find("[REDACTED]") != std::string::npos);
}

TEST(redact_phone) {
    std::vector<std::string> names;
    const std::string out = redact_text("call 555-123-4567 tonight", RedactOptions{}, names);
    CHECK(out.find("555-123-4567") == std::string::npos);
    CHECK(out.find("[REDACTED]") != std::string::npos);
}

TEST(redact_said_phrase) {
    std::vector<std::string> names;
    const std::string out = redact_text("John said he would come", RedactOptions{}, names);
    CHECK(out.find("John") == std::string::npos);
    CHECK(out.find("[REDACTED]") != std::string::npos);
    CHECK(names.size() == 1);
    CHECK(names[0] == "John");
}

TEST(redact_self_intro) {
    std::vector<std::string> names;
    const std::string out = redact_text("hi, my name is Sarah and I live here", RedactOptions{}, names);
    CHECK(out.find("Sarah") == std::string::npos);
    CHECK(out.find("[REDACTED]") != std::string::npos);
}

TEST(redact_case_insensitive_trigger) {
    // Transcripts often start sentences with a capital: "My name is X".
    std::vector<std::string> names;
    const std::string out = redact_text("My name is Sarah and John said hello", RedactOptions{}, names);
    CHECK(out.find("Sarah") == std::string::npos);
    CHECK(out.find("John") == std::string::npos);
    CHECK(out.find("hello") != std::string::npos);
}

TEST(redact_keeps_pronouns) {
    // "He said" / "he'd" must not be redacted.
    std::vector<std::string> names;
    const std::string out = redact_text("He said he'd call Mark tomorrow", RedactOptions{}, names);
    CHECK(out.find("Mark") == std::string::npos);
    CHECK(out.find("he'd") != std::string::npos);
    CHECK(out.find("He said") != std::string::npos);
}

TEST(redact_titled) {
    std::vector<std::string> names;
    const std::string out = redact_text("Dr. Smith will be here", RedactOptions{}, names);
    CHECK(out.find("Smith") == std::string::npos);
    CHECK(out.find("[REDACTED]") != std::string::npos);
}

TEST(synthetic_names_deterministic) {
    const std::string a1 = synthetic_name("John");
    const std::string a2 = synthetic_name("John");
    const std::string b = synthetic_name("Margaret");
    CHECK(a1 == a2);
    CHECK(!a1.empty());
    // Different names usually map differently; allow collision but check range.
    CHECK(b != "[REDACTED]");
}

TEST(flag_segments_sets_pii) {
    std::vector<ocs::Segment> segs;
    ocs::Segment s1;
    s1.start = 0.0;
    s1.end = 1.0;
    s1.text = "Hello there";
    ocs::Segment s2;
    s2.start = 1.0;
    s2.end = 2.0;
    s2.text = "Mark told me the plan";
    segs.push_back(s1);
    segs.push_back(s2);

    const size_t flagged = flag_segments(segs, RedactOptions{});
    CHECK(flagged == 1);
    CHECK(segs[0].has_pii == false);
    CHECK(segs[1].has_pii == true);
    CHECK(segs[1].text.find("Mark") == std::string::npos);
}
