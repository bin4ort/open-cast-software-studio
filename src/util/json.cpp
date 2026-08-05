// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 voice_anonymizer contributors
#include "util/json.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace va::util {

Json::Json(bool b) : type_(Type::Bool), bool_(b) {}
Json::Json(double n) : type_(Type::Number), num_(n) {}
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
                        // BMP only (surrogates out of scope for this parser).
                        if (cp >= 0x80) {
                            out += static_cast<char>(0xC0 | (cp >> 6));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        } else {
                            out += static_cast<char>(cp);
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

}  // namespace va::util
