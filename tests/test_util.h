// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace ocs_test {

struct TestCase {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> reg;
    return reg;
}

struct Reg {
    Reg(const char* name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

inline int& failure_count() {
    static int count = 0;
    return count;
}

inline void report(bool ok, const char* file, int line, const std::string& msg) {
    if (!ok) {
        std::printf("  FAIL %s:%d  %s\n", file, line, msg.c_str());
        ++failure_count();
    }
}

}  // namespace ocs_test

#define TEST(name)                                                           \
    static void test_##name();                                               \
    static ocs_test::Reg reg_##name(#name, test_##name);                      \
    static void test_##name()

#define CHECK(cond) \
    ocs_test::report((cond), __FILE__, __LINE__, std::string(#cond))

#define CHECK_NEAR(a, b, eps)                                                \
    ocs_test::report((std::fabs(static_cast<double>(a) - static_cast<double>(b)) <= \
                     (eps)),                                                  \
                    __FILE__, __LINE__,                                       \
                    std::string(#a " ~= " #b))

#define CHECK_THROWS(expr)                                                   \
    ocs_test::report(([&] {                                                  \
                         try {                                               \
                             (void)(expr);                                   \
                         } catch (...) {                                     \
                             return true;                                    \
                         }                                                   \
                         return false;                                       \
                     })(),                                                   \
                    __FILE__, __LINE__, std::string(#expr " throws"))
