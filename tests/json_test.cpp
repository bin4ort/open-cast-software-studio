// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "test_util.h"

#include <map>
#include <string>
#include <vector>

#include "util/json.h"

using ocs::util::Json;
using ocs::util::JsonError;

TEST(json_parse_object) {
    const Json j = Json::parse(
        R"({"a": 1.5, "b": "text", "c": true, "d": null, "e": [1, 2]})");
    CHECK(j.is_object());
    CHECK_NEAR(j.num("a", 0.0), 1.5, 1e-9);
    CHECK(j.str("b", "") == "text");
    CHECK(j.flag("c", false));
    CHECK(j.contains("d"));
    CHECK(j.at("d").is_null());
    CHECK(j.at("e").is_array());
    CHECK(j.at("e").at(1).as_number() == 2.0);
}

TEST(json_parse_escapes) {
    const Json j = Json::parse(R"({"s": "line\n\"q\" \\ \u00e9 \u20ac"})");
    CHECK(j.str("s", "") == "line\n\"q\" \\ \u00e9 \u20ac");
}

TEST(json_dump_round_trip) {
    std::map<std::string, Json> m;
    m["name"] = Json("Alex");
    m["pitch"] = Json(-5.5);
    m["on"] = Json(true);
    m["tags"] = Json(std::vector<Json>{Json("a"), Json("b")});
    const Json j(std::move(m));
    const Json again = Json::parse(j.dump());
    CHECK(again.str("name", "") == "Alex");
    CHECK_NEAR(again.num("pitch", 0.0), -5.5, 1e-9);
    CHECK(again.flag("on", false));
    CHECK(again.at("tags").as_array().size() == 2);
}

TEST(json_parse_number_edge_cases) {
    CHECK_NEAR(Json::parse("-3.25").as_number(), -3.25, 1e-9);
    CHECK_NEAR(Json::parse("1e3").as_number(), 1000.0, 1e-9);
    CHECK_NEAR(Json::parse("2.5E-1").as_number(), 0.25, 1e-9);
}

TEST(json_parse_errors_throw) {
    CHECK_THROWS(Json::parse("{"));
    CHECK_THROWS(Json::parse("[1, 2"));
    CHECK_THROWS(Json::parse("tru"));
    CHECK_THROWS(Json::parse("{\"a\" 1}"));
    CHECK_THROWS(Json::parse("\"unterminated"));
    CHECK_THROWS(Json::parse("{\"a\": 1,}"));
    CHECK_THROWS(Json::parse(""));
}

TEST(json_missing_keys_default) {
    const Json j = Json::parse("{}");
    CHECK(j.num("missing", 7.0) == 7.0);
    CHECK(j.str("missing", "d") == "d");
    CHECK(j.flag("missing", true));
    CHECK(j.arr("missing").empty());
    CHECK_THROWS(j.at("missing"));
}

TEST(json_array_access) {
    const Json j = Json::parse("[10, 20, 30]");
    CHECK(j.is_array());
    CHECK(j.at(0).as_number() == 10.0);
    CHECK(j.at(2).as_number() == 30.0);
    CHECK_THROWS(j.at(3));
}
