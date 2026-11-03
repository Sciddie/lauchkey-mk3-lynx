#include "test.hpp"

#include "lkmk3/version.hpp"

TEST_CASE ("version: header matches the CMake project version")
{
    CHECK_EQ (std::string (lkmk3::versionString), std::string (LKMK3_PROJECT_VERSION));
    const auto composed = std::to_string (lkmk3::versionMajor) + "." + std::to_string (lkmk3::versionMinor) + "."
                        + std::to_string (lkmk3::versionPatch);
    CHECK_EQ (composed, std::string (lkmk3::versionString));
}

TEST_CASE ("version: library and header agree")
{
    CHECK_EQ (std::string (lkmk3::libraryVersion()), std::string (lkmk3::versionString));
}
