// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "ai/rewriter.h"

#include <cctype>
#include <regex>
#include <string>
#include <vector>

namespace ocs::rewrite {

namespace {

struct Phrase {
    const char* pattern;   // case-insensitive, word-bounded
    const char* male;      // replacement when the speaker becomes male
    const char* female;    // replacement when the speaker becomes female
};

// Self-referential and gender-carrying phrases. The male/female columns are
// from the speaker's perspective (target gender).
const std::vector<Phrase>& table() {
    static const std::vector<Phrase> kTable = {
        {"i am a woman", "i am a man", "i am a woman"},
        {"i am a man", "i am a man", "i am a woman"},
        {"i am a girl", "i am a boy", "i am a girl"},
        {"i am a boy", "i am a boy", "i am a girl"},
        {"i'm a woman", "i'm a man", "i'm a woman"},
        {"i'm a man", "i'm a man", "i'm a woman"},
        {"i'm a girl", "i'm a boy", "i'm a girl"},
        {"i'm a boy", "i'm a boy", "i'm a girl"},
        {"my husband", "my wife", "my husband"},
        {"my wife", "my wife", "my husband"},
        {"my brother", "my brother", "my sister"},
        {"my sister", "my brother", "my sister"},
        {"my father", "my father", "my mother"},
        {"my mother", "my father", "my mother"},
        {"my dad", "my dad", "my mom"},
        {"my mom", "my dad", "my mom"},
        {"my son", "my son", "my daughter"},
        {"my daughter", "my son", "my daughter"},
        {"my uncle", "my uncle", "my aunt"},
        {"my aunt", "my uncle", "my aunt"},
        {"grandfather", "grandfather", "grandmother"},
        {"grandmother", "grandfather", "grandmother"},
        {"grandpa", "grandpa", "grandma"},
        {"grandma", "grandpa", "grandma"},
        {"prom king", "prom king", "prom queen"},
        {"prom queen", "prom king", "prom queen"},
        {"bridegroom", "bridegroom", "bride"},
        {"bride", "bridegroom", "bride"},
        {"actor", "actor", "actress"},
        {"actress", "actor", "actress"},
        {"waiter", "waiter", "waitress"},
        {"waitress", "waiter", "waitress"},
    };
    return kTable;
}

std::string replace_case_insensitive(const std::string& text,
                                     const std::string& pattern,
                                     const std::string& replacement) {
    std::regex re("\\b" + pattern + "\\b", std::regex::icase);
    std::string out;
    size_t pos = 0;
    for (std::sregex_iterator it(text.begin(), text.end(), re), end; it != end;
         ++it) {
        const auto& m = *it;
        const size_t mpos = static_cast<size_t>(m.position(0));
        const size_t mlen = static_cast<size_t>(m.length(0));
        out.append(text, pos, mpos - pos);
        std::string repl = replacement;
        if (!repl.empty() && mlen > 0 &&
            std::isupper(static_cast<unsigned char>(text[mpos]))) {
            repl[0] = static_cast<char>(
                std::toupper(static_cast<unsigned char>(repl[0])));
        }
        out.append(repl);
        pos = mpos + mlen;
    }
    out.append(text, pos, text.size() - pos);
    return out;
}

}  // namespace

std::string rewrite_for_gender(const std::string& text, dsp::Gender target,
                               bool& changed) {
    changed = false;
    if (target != dsp::Gender::Male && target != dsp::Gender::Female) {
        return text;
    }
    const bool to_male = target == dsp::Gender::Male;
    std::string out = text;
    for (const auto& p : table()) {
        const char* repl = to_male ? p.male : p.female;
        const std::string prev = out;
        out = replace_case_insensitive(out, p.pattern, repl);
        if (out != prev) changed = true;
    }
    return out;
}

}  // namespace ocs::rewrite
