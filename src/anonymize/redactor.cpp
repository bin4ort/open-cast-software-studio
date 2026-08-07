// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "anonymize/redactor.h"

#include <cctype>
#include <cstring>
#include <map>
#include <regex>
#include <set>

namespace ocs::redact {

std::string synthetic_name(const std::string& original) {
    static const std::vector<std::string> kNames = {
        "Alex",   "Sam",    "Jordan", "Casey", "Riley", "Morgan",
        "Taylor", "Avery",  "Quinn",  "Robin", "Parker", "Blake",
    };
    size_t h = 2166136261u;
    for (unsigned char c : original) {
        h = (h ^ c) * 16777619u;
    }
    return kNames[h % kNames.size()];
}

namespace {

std::string replace_all(const std::string& text, const std::regex& re,
                        const std::string& replacement) {
    return std::regex_replace(text, re, replacement);
}

// Replaces every occurrence of `word` (word-bounded, case-sensitive) in text.
std::string replace_word(const std::string& text, const std::string& word,
                         const std::string& replacement) {
    std::regex re("\\b" + word + "\\b");
    return std::regex_replace(text, re, replacement);
}

std::string redact_names(const std::string& text, const RedactOptions& opts,
                         std::vector<std::string>& names_found) {
    std::set<std::string> names;

    // "X said/told/asked/replied" and "said/told/asked/replied X". The trigger
    // words match case-insensitively, but we later reject captures whose first
    // letter is not uppercase in the original text, so "said hello" is not
    // treated as a name.
    static const std::regex after_verb(
        R"(\b(?:said|told|asked|replied|answered|called|call|introduced)\s+([A-Z][a-zA-Z]{1,15})\b)",
        std::regex::icase);
    static const std::regex before_verb(
        R"(\b([A-Z][a-zA-Z]{1,15})\s+(?:said|told|asked|replied|answered|called)\b)",
        std::regex::icase);
    // "my name is X", "I'm X", "call me X", "this is X", "X speaking".
    static const std::regex self_id(
        R"(\b(?:my name(?:'s| is)|\bI'?m|I am|call(?:ed)? me|this is|you can call me|I go by)\s+([A-Z][a-zA-Z]{1,15})\b)",
        std::regex::icase);
    // Titles: "Mr./Ms./Dr./Prof. X".
    static const std::regex titled(
        R"(\b(?:Mr|Mrs|Ms|Miss|Dr|Prof|Sgt|Cpt|Officer|Agent)\.?\s+([A-Z][a-zA-Z]{1,15})\b)",
        std::regex::icase);

    for (const std::regex& re :
         {after_verb, before_verb, self_id, titled}) {
        for (std::sregex_iterator it(text.begin(), text.end(), re), end; it != end;
             ++it) {
            const std::string m = (*it)[1].str();
            // The capture group was matched case-insensitively; only accept it
            // as a name if it is actually capitalized in the original text.
            const size_t pos = static_cast<size_t>(it->position(1));
            if (pos >= text.size() ||
                !std::isupper(static_cast<unsigned char>(text[pos]))) {
                continue;
            }
            // Pronouns and sentence-openers are not names.
            static const char* kStop[] = {
                "i",       "he",     "she",   "we",     "they",   "it",
                "you",     "who",    "this",  "that",   "there",  "here",
                "the",     "and",    "but",   "so",     "well",   "oh",
                "yeah",    "right",  "ok",    "okay",   "now",    "then",
                "me",      "us",     "him",   "her",    "them",   "my",
                "his",     "her",    "its",   "our",    "their",  "your",
                "one",     "everyone", "somebody",
            };
            bool stop = false;
            for (const char* s : kStop) {
                bool same = m.size() == std::strlen(s);
                for (size_t c = 0; same && c < m.size(); ++c) {
                    if (std::tolower(static_cast<unsigned char>(m[c])) != s[c]) {
                        same = false;
                    }
                }
                if (same) { stop = true; break; }
            }
            if (!m.empty() && !stop) names.insert(m);
        }
    }

    std::string out = text;
    for (const auto& name : names) {
        const std::string repl =
            opts.use_synthetic_names ? synthetic_name(name) : opts.replacement;
        out = replace_word(out, name, repl);
        names_found.push_back(name);
    }
    return out;
}

}  // namespace

std::string redact_text(const std::string& text, const RedactOptions& opts,
                        std::vector<std::string>& names_found) {
    std::string out = text;

    if (opts.redact_emails) {
        static const std::regex email(
            R"([A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,})");
        out = replace_all(out, email, opts.replacement);
    }
    if (opts.redact_urls) {
        static const std::regex url(R"(\b(?:https?://|www\.)\S+)");
        out = replace_all(out, url, opts.replacement);
    }
    if (opts.redact_phones) {
        static const std::regex phone(
            R"((?:\+?[0-9][\s\-]?)?(?:\(?[0-9]{3}\)?[\s\-.]?)?[0-9]{3}[\s\-.]?[0-9]{4})");
        out = replace_all(out, phone, opts.replacement);
    }
    if (opts.redact_names) {
        out = redact_names(out, opts, names_found);
    }
    return out;
}

size_t flag_segments(std::vector<Segment>& segments, const RedactOptions& opts) {
    size_t flagged = 0;
    for (auto& seg : segments) {
        std::vector<std::string> names;
        const std::string redacted = redact_text(seg.text, opts, names);
        if (redacted != seg.text) {
            seg.has_pii = true;
            seg.text = redacted;
            ++flagged;
        }
    }
    return flagged;
}

}  // namespace ocs::redact
