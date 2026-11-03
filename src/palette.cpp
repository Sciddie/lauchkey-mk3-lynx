#include "lkmk3/palette.hpp"

#include <array>
#include <cstdlib>

namespace lkmk3::palette
{
namespace
{
// Sampled from the colour table in the Programmer's reference manual (page 14). The printed chart is lightened (its
// darkest channel sits around 0x61), so matching stretches 0x61..0xFF back to 0..0xFF first (see nearest()).
constexpr std::array<std::uint32_t, 128> table {
    0x616161, 0xB3B3B3, 0xDDDDDD, 0xFFFFFF, // 0..3
    0xFFB3B3, 0xFF6161, 0xDD6161, 0xB36161, // 4..7
    0xFFF3D5, 0xFFB361, 0xDD8C61, 0xB37661, // 8..11
    0xFFEEA1, 0xFFFF61, 0xDDDD61, 0xB3B361, // 12..15
    0xDDFFA1, 0xC2FF61, 0xA1DD61, 0x81B361, // 16..19
    0xC2FFB3, 0x61FF61, 0x61DD61, 0x61B361, // 20..23
    0xC2FFC2, 0x61FF8C, 0x61DD76, 0x61B36B, // 24..27
    0xC2FFCC, 0x61FFCC, 0x61DDA1, 0x61B381, // 28..31
    0xC2FFF3, 0x61FFE9, 0x61DDC2, 0x61B396, // 32..35
    0xC2F3FF, 0x61EEFF, 0x61C7DD, 0x61A1B3, // 36..39
    0xC2DDFF, 0x61C7FF, 0x61A1DD, 0x6181B3, // 40..43
    0xA18CFF, 0x6161FF, 0x6161DD, 0x6161B3, // 44..47
    0xCCB3FF, 0xA161FF, 0x8161DD, 0x7661B3, // 48..51
    0xFFB3FF, 0xFF61FF, 0xDD61DD, 0xB361B3, // 52..55
    0xFFB3D5, 0xFF61C2, 0xDD61A1, 0xB3618C, // 56..59
    0xFF7661, 0xE9B361, 0xDDC261, 0xA1A161, // 60..63
    0x61B361, 0x61B38C, 0x618CD5, 0x6161FF, // 64..67
    0x61B3B3, 0x8C61F3, 0xCCB3C2, 0x8C7681, // 68..71
    0xFF6161, 0xF3FFA1, 0xEEFC61, 0xCCFF61, // 72..75
    0x76DD61, 0x61FFCC, 0x61E9FF, 0x61A1FF, // 76..79
    0x8C61FF, 0xCC61FC, 0xEE8CDD, 0xA17661, // 80..83
    0xFFA161, 0xDDF961, 0xD5FF8C, 0x61FF61, // 84..87
    0xB3FFA1, 0xCCFCD5, 0xB3FFF6, 0xCCE4FF, // 88..91
    0xA1C2F6, 0xD5C2F9, 0xF98CFF, 0xFF61CC, // 92..95
    0xFFC261, 0xF3EE61, 0xE4FF61, 0xDDCC61, // 96..99
    0xB3A161, 0x61BA76, 0x76C28C, 0x8181A1, // 100..103
    0x818CCC, 0xCCAA81, 0xDD6161, 0xF9B3A1, // 104..107
    0xF9BA76, 0xFFF38C, 0xE9F9A1, 0xD5EE76, // 108..111
    0x8181A1, 0xF9F9D5, 0xDDFCE4, 0xE9E9FF, // 112..115
    0xE4D5FF, 0xB3B3B3, 0xD5D5D5, 0xF9FFFF, // 116..119
    0xE96161, 0xAA6161, 0x81F661, 0x61B361, // 120..123
    0xF3EE61, 0xB3A161, 0xEEC261, 0xC27661, // 124..127
};

int stretch (int c) noexcept
{
    const auto v = (c - 0x61) * 255 / (0xFF - 0x61);
    return v < 0 ? 0 : (v > 255 ? 255 : v);
}

int channel (std::uint32_t rgb, int shift) noexcept { return (int) ((rgb >> shift) & 0xFFu); }
} // namespace

std::uint32_t rgbOf (std::uint8_t index) noexcept { return table[(std::size_t) (index & 0x7F)]; }

std::uint8_t nearest (std::uint32_t rgb) noexcept
{
    const auto r = channel (rgb, 16), g = channel (rgb, 8), b = channel (rgb, 0);
    if (r + g + b < 40)
        return off;
    long best = -1;
    std::uint8_t bestIndex = white;
    for (std::size_t i = 1; i < table.size(); ++i)
    {
        const auto pr = stretch (channel (table[i], 16)), pg = stretch (channel (table[i], 8)), pb = stretch (channel (table[i], 0));
        // "redmean" weighted distance: cheap and closer to perception than plain RGB
        const long rmean = (r + pr) / 2;
        const long dr = r - pr, dg = g - pg, db = b - pb;
        const long d = (((512 + rmean) * dr * dr) >> 8) + 4 * dg * dg + (((767 - rmean) * db * db) >> 8);
        if (best < 0 || d < best)
        {
            best = d;
            bestIndex = (std::uint8_t) i;
        }
    }
    return bestIndex;
}

std::uint8_t dimmed (std::uint8_t index) noexcept
{
    index &= 0x7F;
    if (index == off)
        return off;
    if (index <= white) // the gray column: 3 -> 1, 2 -> 1, 1 -> 1
        return dimWhite;
    if (index < 64) // hue blocks of four (4 = light, 5 = full, 6 = mid, 7 = dark): go to the darkest one
        return (std::uint8_t) ((index & ~3u) + 3);
    return index; // the second half has no regular structure: keep the colour
}
} // namespace lkmk3::palette
