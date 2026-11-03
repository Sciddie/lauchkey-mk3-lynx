#include "test.hpp"

#include "lkmk3/palette.hpp"

using namespace lkmk3;

TEST_CASE ("palette: primaries map to the manual's full-brightness entries")
{
    CHECK_EQ ((int) palette::nearest (0xFF0000), (int) palette::red);
    CHECK_EQ ((int) palette::nearest (0x00FF00), (int) palette::green);
    CHECK_EQ ((int) palette::nearest (0x0000FF), (int) palette::blue);
    CHECK_EQ ((int) palette::nearest (0xFFFFFF), (int) palette::white);
    CHECK_EQ ((int) palette::nearest (0xFFFF00), (int) palette::yellow);
}

TEST_CASE ("palette: black and near-black are off, alpha is ignored")
{
    CHECK_EQ ((int) palette::nearest (0x000000), 0);
    CHECK_EQ ((int) palette::nearest (0xFF050505), 0);
    CHECK_EQ ((int) palette::nearest (0xFFFF0000), (int) palette::red);
}

TEST_CASE ("palette: dimmed stays in the hue block")
{
    CHECK_EQ ((int) palette::dimmed (palette::red), (int) palette::dimRed);
    CHECK_EQ ((int) palette::dimmed (palette::green), (int) palette::dimGreen);
    CHECK_EQ ((int) palette::dimmed (palette::white), (int) palette::dimWhite);
    CHECK_EQ ((int) palette::dimmed (0), 0);
    CHECK_EQ ((int) palette::dimmed (100), 100);
}

TEST_CASE ("palette: table entries sampled from the manual")
{
    CHECK_EQ (palette::rgbOf (3), 0xFFFFFFu);
    CHECK_EQ (palette::rgbOf (5), 0xFF6161u);
    CHECK_EQ (palette::rgbOf (200), palette::rgbOf (200 & 0x7F));
}
