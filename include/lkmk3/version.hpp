#pragma once

// Semantic version of the Launchkey MK3 integration for LYNX SHARE.
// Bump together with CMakeLists.txt (project VERSION) and CHANGELOG.md; tag releases as vX.Y.Z.

namespace lkmk3
{
inline constexpr int versionMajor = 0;
inline constexpr int versionMinor = 0;
inline constexpr int versionPatch = 1;
inline constexpr const char* versionString = "0.0.1";

// The version compiled into the library (equals versionString unless headers and library are mismatched).
const char* libraryVersion() noexcept;
} // namespace lkmk3
