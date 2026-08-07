// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace ocs::util {

// Minimal recursive-descent JSON parser (no external deps). Enough for the
// whisper transcript JSON and small config files.
class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() = default;
    Json(bool b);
    Json(double n);
    Json(int n);
    Json(const char* s);
    Json(std::string s);
    Json(std::vector<Json> arr);
    Json(std::map<std::string, Json> obj);

    static Json parse(const std::string& text);
    static Json parse_file(const std::string& path);

    // Serializes this value to compact JSON text.
    std::string dump() const;

    Type type() const { return type_; }
    bool is_null() const { return type_ == Type::Null; }
    bool is_bool() const { return type_ == Type::Bool; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }

    bool as_bool() const;
    double as_number() const;
    const std::string& as_string() const;
    const std::vector<Json>& as_array() const;
    const std::map<std::string, Json>& as_object() const;

    bool contains(const std::string& key) const;
    const Json& at(const std::string& key) const;  // throws std::out_of_range
    const Json& at(size_t i) const;

    double num(const std::string& key, double dflt) const;
    std::string str(const std::string& key, const std::string& dflt) const;
    bool flag(const std::string& key, bool dflt) const;
    std::vector<Json> arr(const std::string& key) const;

private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double num_ = 0.0;
    std::string str_;
    std::vector<Json> array_;
    std::map<std::string, Json> object_;
};

// Thrown by Json::parse on malformed input.
struct JsonError : std::runtime_error {
    explicit JsonError(const std::string& m) : std::runtime_error(m) {}
};

}  // namespace ocs::util
