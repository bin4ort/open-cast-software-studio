// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "util/json.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

namespace ocs::util {

Json::Json(bool b) : type_(Type::Bool), bool_(b) {}
Json::Json(double n) : type_(Type::Number), num_(n) {}
Json::Json(int n) : type_(Type::Number), num_(static_cast<double>(n)) {}
Json::Json(const char* s) : type_(Type::String), str_(s ? s : "") {}
Json::Json(std::string s) : type_(Type::String), str_(std::move(s)) {}
Json::Json(std::vector<Json> arr) : type_(Type::Array), array_(std::move(arr)) {}
Json::Json(std::map<std::string, Json> obj)
    : type_(Type::Object), object_(std::move(obj)) {}

bool Json::as_bool() const {
    if (type_ == Type::Bool) return bool_;
    if (type_ == Type::Number) return num_ != 0.0;
    if (type_ == Type::String) return str_ == "true" || str_ == "1";
    return false;
}

double Json::as_number() const {
    if (type_ == Type::Number) return num_;
    if (type_ == Type::String) return std::atof(str_.c_str());
    return 0.0;
}

const std::string& Json::as_string() const {
    return str_;
}

const std::vector<Json>& Json::as_array() const {
    return array_;
}

const std::map<std::string, Json>& Json::as_object() const {
    return object_;
}

bool Json::contains(const std::string& key) const {
    return type_ == Type::Object && object_.count(key) > 0;
}

const Json& Json::at(const std::string& key) const {
    if (type_ != Type::Object) throw std::out_of_range("not an object");
    auto it = object_.find(key);
    if (it == object_.end()) throw std::out_of_range("key not found: " + key);
    return it->second;
}

const Json& Json::at(size_t i) const {
    if (type_ != Type::Array || i >= array_.size()) {
        throw std::out_of_range("array index out of range");
    }
    return array_[i];
}

double Json::num(const std::string& key, double dflt) const {
    return contains(key) ? at(key).as_number() : dflt;
}

std::string Json::str(const std::string& key, const std::string& dflt) const {
    return contains(key) ? at(key).as_string() : dflt;
}

bool Json::flag(const std::string& key, bool dflt) const {
    return contains(key) ? at(key).as_bool() : dflt;
}

std::vector<Json> Json::arr(const std::string& key) const {
    if (!contains(key)) return {};
    return at(key).as_array();
}

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------
namespace {

class Parser {
public:
    explicit Parser(const std::string& text) : s_(text), i_(0) {}

    Json parse_value() {
        skip_ws();
        if (i_ >= s_.size()) throw JsonError("unexpected end of input");
        const char c = s_[i_];
        switch (c) {
            case '{': return parse_object();
            case '[': return parse_array();
            case '"': return Json(parse_string());
            case 't': expect_lit("true"); return Json(true);
            case 'f': expect_lit("false"); return Json(false);
            case 'n': expect_lit("null"); return Json();
            default:
                if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
                    return Json(parse_number());
                }
                throw JsonError(std::string("unexpected character: ") + c);
        }
    }

private:
    const std::string& s_;
    size_t i_;

    void skip_ws() {
        while (i_ < s_.size() &&
               std::isspace(static_cast<unsigned char>(s_[i_]))) {
            ++i_;
        }
    }

    void expect(char c) {
        if (i_ >= s_.size() || s_[i_] != c) {
            throw JsonError(std::string("expected '") + c + "'");
        }
        ++i_;
    }

    void expect_lit(const char* lit) {
        for (const char* p = lit; *p; ++p) {
            if (i_ >= s_.size() || s_[i_] != *p) {
                throw JsonError(std::string("invalid literal"));
            }
            ++i_;
        }
    }

    std::string parse_string() {
        expect('"');
        std::string out;
        while (i_ < s_.size()) {
            const char c = s_[i_++];
            if (c == '"') return out;
            if (c == '\\') {
                if (i_ >= s_.size()) break;
                const char e = s_[i_++];
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        if (i_ + 4 > s_.size()) throw JsonError("bad \\u escape");
                        unsigned int cp = 0;
                        for (int k = 0; k < 4; ++k) {
                            const char h = s_[i_++];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= h - '0';
                            else if (h >= 'a' && h <= 'f') cp |= h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') cp |= h - 'A' + 10;
                            else throw JsonError("bad \\u escape");
                        }
                        // Handle surrogate pairs BEFORE encoding: if cp is a high surrogate (0xD800-0xDBFF),
                        // expect the next \u to be a low surrogate (0xDC00-0xDFFF) and combine.
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            if (i_ + 5 <= s_.size() && s_[i_] == '\\' && s_[i_+1] == 'u') {
                                unsigned int low = 0;
                                i_ += 2;  // skip \u
                                for (int k = 0; k < 4; ++k) {
                                    const char h = s_[i_++];
                                    low <<= 4;
                                    if (h >= '0' && h <= '9') low |= h - '0';
                                    else if (h >= 'a' && h <= 'f') low |= h - 'a' + 10;
                                    else if (h >= 'A' && h <= 'F') low |= h - 'A' + 10;
                                    else throw JsonError("bad \\u escape");
                                }
                                if (low >= 0xDC00 && low <= 0xDFFF) {
                                    // Combine high + low surrogate into single code point
                                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                                } else {
                                    throw JsonError("invalid surrogate pair");
                                }
                                // Encode the combined code point (4 bytes for U+10000+)
                                out += static_cast<char>(0xF0 | (cp >> 18));
                                out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                                out += static_cast<char>(0x80 | (cp & 0x3F));
                                break;  // Exit switch after handling surrogate pair
                            }
                        }
                        // Encode as UTF-8 (2-4 bytes depending on the
                        // code point; surrogate escapes produce single
                        // BMP code points, which is fine for transcripts).
                        // Reject orphan surrogates (low without high, or high not followed by valid low)
                        if (cp >= 0xD800 && cp <= 0xDC00) {
                            // Low surrogate not preceded by high surrogate - error
                            throw JsonError("invalid surrogate pair");
                        }
                        if (cp >= 0xE000 && cp <= 0xFFFF) {
                            // High surrogate not followed by valid low surrogate - error
                            throw JsonError("invalid surrogate pair");
                        }
                        if (cp < 0x80) {
                            out += static_cast<char>(cp);
                        } else if (cp < 0x800) {
                            out += static_cast<char>(0xC0 | (cp >> 6));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        } else if (cp < 0x10000) {
                            out += static_cast<char>(0xE0 | (cp >> 12));
                            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        } else {
                            out += static_cast<char>(0xF0 | (cp >> 18));
                            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        }
                        break;
                    }
                    default: throw JsonError("bad escape");
                }
            } else {
                out += c;
            }
        }
        throw JsonError("unterminated string");
    }

    double parse_number() {
        const size_t start = i_;
        if (i_ < s_.size() && s_[i_] == '-') ++i_;
        while (i_ < s_.size() &&
               std::isdigit(static_cast<unsigned char>(s_[i_]))) ++i_;
        if (i_ < s_.size() && s_[i_] == '.') {
            ++i_;
            while (i_ < s_.size() &&
                   std::isdigit(static_cast<unsigned char>(s_[i_]))) ++i_;
        }
        if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
            ++i_;
            if (i_ < s_.size() && (s_[i_] == '+' || s_[i_] == '-')) ++i_;
            while (i_ < s_.size() &&
                   std::isdigit(static_cast<unsigned char>(s_[i_]))) ++i_;
        }
        return std::strtod(s_.substr(start, i_ - start).c_str(), nullptr);
    }

    Json parse_object() {
        expect('{');
        std::map<std::string, Json> obj;
        skip_ws();
        if (i_ < s_.size() && s_[i_] == '}') {
            ++i_;
            return Json(std::move(obj));
        }
        while (true) {
            skip_ws();
            if (i_ >= s_.size()) throw JsonError("unterminated object");
            std::string key = parse_string();
            skip_ws();
            expect(':');
            Json val = parse_value();
            obj[key] = std::move(val);
            skip_ws();
            if (i_ >= s_.size()) throw JsonError("unterminated object");
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == '}') {
                ++i_;
                return Json(std::move(obj));
            }
            throw JsonError("expected ',' or '}' in object");
        }
    }

    Json parse_array() {
        expect('[');
        std::vector<Json> arr;
        skip_ws();
        if (i_ < s_.size() && s_[i_] == ']') {
            ++i_;
            return Json(std::move(arr));
        }
        while (true) {
            arr.push_back(parse_value());
            skip_ws();
            if (i_ >= s_.size()) throw JsonError("unterminated array");
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == ']') {
                ++i_;
                return Json(std::move(arr));
            }
            throw JsonError("expected ',' or ']' in array");
        }
    }
};

}  // namespace

Json Json::parse(const std::string& text) {
    Parser p(text);
    Json j = p.parse_value();
    return j;
}

Json Json::parse_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw JsonError("cannot open file: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return parse(ss.str());
}

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------
namespace {

std::string dump_string(const std::string& s) {
    std::string out = "\"";
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
    out += '"';
    return out;
}

std::string dump_value(const Json& j) {
    switch (j.type()) {
        case Json::Type::Null: return "null";
        case Json::Type::Bool: return j.as_bool() ? "true" : "false";
        case Json::Type::Number: {
            const double n = j.as_number();
            if (n == static_cast<long long>(n)) {
                return std::to_string(static_cast<long long>(n));
            }
            return std::to_string(n);
        }
        case Json::Type::String: return dump_string(j.as_string());
        case Json::Type::Array: {
            std::string out = "[";
            const auto& arr = j.as_array();
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i) out += ",";
                out += dump_value(arr[i]);
            }
            out += "]";
            return out;
        }
        case Json::Type::Object: {
            std::string out = "{";
            const auto& obj = j.as_object();
            size_t i = 0;
            for (const auto& [k, v] : obj) {
                if (i++) out += ",";
                out += dump_string(k) + ":" + dump_value(v);
            }
            out += "}";
            return out;
        }
    }
    return "null";
}

}  // namespace

std::string Json::dump() const {
    return dump_value(*this);
}

}  // namespace ocs::util
