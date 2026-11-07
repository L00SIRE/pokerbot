#pragma once

#include <cmath>
#include <cstdio>
#include <vector>

// tiny test harness, didn't want to pull gtest in for this

struct TestCase {
    const char* name;
    void (*fn)();
};

inline std::vector<TestCase>& all_tests() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failures() {
    static int n = 0;
    return n;
}

struct Register {
    Register(const char* name, void (*fn)()) { all_tests().push_back({name, fn}); }
};

#define TEST(name)                           \
    static void name();                      \
    static Register name##_reg(#name, name); \
    static void name()

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::printf("    FAILED %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            failures()++;                                                     \
        }                                                                     \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                               \
    do {                                                                                    \
        double a_ = (a), b_ = (b);                                                          \
        if (std::fabs(a_ - b_) > (eps)) {                                                   \
            std::printf("    FAILED %s:%d  %s = %g, wanted %g\n", __FILE__, __LINE__, #a, a_, \
                        b_);                                                                \
            failures()++;                                                                   \
        }                                                                                   \
    } while (0)
