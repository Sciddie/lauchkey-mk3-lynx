#pragma once

// A tiny self-contained test harness (no third-party dependency, so the tests build anywhere CMake does).

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace lkmk3::test
{
struct Case
{
    const char* name;
    std::function<void()> body;
};

std::vector<Case>& registry();
void fail (const char* file, int line, const std::string& what);

struct Register
{
    Register (const char* name, std::function<void()> body) { registry().push_back ({ name, std::move (body) }); }
};

template <typename A, typename B>
std::string describe (const A& a, const B& b)
{
    if constexpr (requires { std::to_string (a); std::to_string (b); })
        return std::to_string (a) + " vs " + std::to_string (b);
    else if constexpr (requires { std::string (a); std::string (b); })
        return "\"" + std::string (a) + "\" vs \"" + std::string (b) + "\"";
    else
        return "(values not printable)";
}
} // namespace lkmk3::test

#define LK_CONCAT2(a, b) a##b
#define LK_CONCAT(a, b) LK_CONCAT2 (a, b)
#define TEST_CASE(name)                                                                                    \
    static void LK_CONCAT (lk_test_, __LINE__)();                                                          \
    static const lkmk3::test::Register LK_CONCAT (lk_reg_, __LINE__) { name, &LK_CONCAT (lk_test_, __LINE__) }; \
    static void LK_CONCAT (lk_test_, __LINE__)()

#define CHECK(cond)                                                                                        \
    do                                                                                                     \
    {                                                                                                      \
        if (! (cond))                                                                                      \
            lkmk3::test::fail (__FILE__, __LINE__, "CHECK(" #cond ")");                                    \
    } while (false)

#define CHECK_EQ(a, b)                                                                                     \
    do                                                                                                     \
    {                                                                                                      \
        const auto& lk_a = (a);                                                                            \
        const auto& lk_b = (b);                                                                            \
        if (! (lk_a == lk_b))                                                                              \
            lkmk3::test::fail (__FILE__, __LINE__,                                                         \
                               "CHECK_EQ(" #a ", " #b "): " + lkmk3::test::describe (lk_a, lk_b));         \
    } while (false)
