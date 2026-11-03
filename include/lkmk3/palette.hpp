#pragma once

// The Launchkey MK3 colour palette (128 entries, "Colour palette" chapter) and nearest-colour matching, so the
// host's RGB colours (mixer tracks, channels, patterns) can be shown on the RGB pads and buttons.

#include <cstdint>

namespace lkmk3::palette
{
// A few palette indices used for fixed meanings.
constexpr std::uint8_t off = 0;
constexpr std::uint8_t dimWhite = 1, midWhite = 2, white = 3;
constexpr std::uint8_t red = 5, dimRed = 7;
constexpr std::uint8_t amber = 9, dimAmber = 11;
constexpr std::uint8_t yellow = 13, dimYellow = 15;
constexpr std::uint8_t green = 21, dimGreen = 23;
constexpr std::uint8_t cyan = 37, dimCyan = 39;
constexpr std::uint8_t blue = 45, dimBlue = 47;
constexpr std::uint8_t purple = 53, dimPurple = 55;

// Approximate display RGB (0xRRGGBB) of a palette index, as printed in the manual.
std::uint32_t rgbOf (std::uint8_t index) noexcept;
// The palette index closest to an RGB colour (0xRRGGBB; alpha ignored). Black-ish colours map to `off`.
std::uint8_t nearest (std::uint32_t rgb) noexcept;
// A dimmer palette entry of the same hue (the manual's palette has 4 brightness steps per hue column).
std::uint8_t dimmed (std::uint8_t index) noexcept;
} // namespace lkmk3::palette
