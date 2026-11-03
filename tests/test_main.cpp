#include "test.hpp"

#include <cstring>

namespace lkmk3::test
{
namespace
{
int failures = 0;
}

std::vector<Case>& registry()
{
    static std::vector<Case> cases;
    return cases;
}

void fail (const char* file, int line, const std::string& what)
{
    ++failures;
    std::fprintf (stderr, "  FAIL %s:%d: %s\n", file, line, what.c_str());
}
} // namespace lkmk3::test

int main (int argc, char** argv)
{
    using namespace lkmk3::test;
    const char* filter = argc > 1 ? argv[1] : nullptr; // optional substring filter
    int run = 0, failedCases = 0;
    for (const auto& c : registry())
    {
        if (filter != nullptr && std::strstr (c.name, filter) == nullptr)
            continue;
        const auto before = failures;
        c.body();
        ++run;
        if (failures != before)
        {
            ++failedCases;
            std::fprintf (stderr, "[FAILED] %s\n", c.name);
        }
        else
            std::printf ("[  OK  ] %s\n", c.name);
    }
    std::printf ("\n%d test cases, %d failed, %d failed checks\n", run, failedCases, failures);
    return failures == 0 && run > 0 ? 0 : 1;
}
