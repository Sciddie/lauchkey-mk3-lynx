#include "test.hpp"

#include "lkmk3/palette.hpp"
#include "sim_surface.hpp"

using namespace lkmk3;
using namespace lkmk3::test;

namespace
{
void tapPad (Rig& rig, int index)
{
    rig.tap (SimSurface::pad (index, 100), SimSurface::pad (index, 0));
}
} // namespace

TEST_CASE ("steps: session pads toggle the selected channel's steps")
{
    Rig rig;
    rig.connect();
    tapPad (rig, 0);
    CHECK (rig.host.steps[0][0]);
    tapPad (rig, 9); // bottom row, second pad = step 10
    CHECK (rig.host.steps[0][9]);
    tapPad (rig, 0);
    CHECK (! rig.host.steps[0][0]);
    rig.host.selected = 2;
    tapPad (rig, 3);
    CHECK (rig.host.steps[2][3]);
    CHECK (! rig.host.steps[0][3]);
}

TEST_CASE ("steps: LEDs show set steps in the channel colour, the beat grid and the playhead")
{
    Rig rig;
    rig.host.channels[0].colour = 0x0000FF;
    rig.host.steps[0][2] = true;
    rig.connect();
    CHECK_EQ (rig.surface.padLed (0x62).colour, (int) palette::blue); // step 3 set
    CHECK_EQ (rig.surface.padLed (0x60).colour, (int) palette::dimWhite); // step 1: beat
    CHECK_EQ (rig.surface.padLed (0x61).colour, 0);                    // step 2: empty
    CHECK_EQ (rig.surface.padLed (0x70).colour, (int) palette::dimWhite); // step 9: beat
    rig.host.playhead = 1;
    rig.refresh();
    CHECK_EQ (rig.surface.padLed (0x61).colour, (int) palette::white);
    rig.host.playhead = -1;
    rig.refresh();
    CHECK_EQ (rig.surface.padLed (0x61).colour, 0);
}

TEST_CASE ("steps: default colour when the channel has none, and the LED follows a toggle")
{
    Rig rig;
    rig.connect();
    tapPad (rig, 5);
    CHECK_EQ (rig.surface.padLed (0x65).colour, (int) palette::amber);
}

TEST_CASE ("steps: scene up/down page through longer patterns")
{
    Rig rig;
    for (auto& row : rig.host.steps)
        row.resize (32);
    rig.connect();
    CHECK_EQ (rig.surface.ccLed (106).colour, 0);
    CHECK_EQ (rig.surface.ccLed (107).colour, (int) palette::white);
    rig.press (107);
    CHECK_EQ (rig.controller.stepPage(), 1);
    tapPad (rig, 0);
    CHECK (rig.host.steps[0][16]);
    CHECK_EQ (rig.surface.ccLed (106).colour, (int) palette::white);
    CHECK_EQ (rig.surface.ccLed (107).colour, 0);
    rig.press (107);
    CHECK_EQ (rig.controller.stepPage(), 1); // clamped
    rig.press (106);
    CHECK_EQ (rig.controller.stepPage(), 0);
}

TEST_CASE ("steps: pads past the pattern length are dark and do nothing")
{
    Rig rig;
    for (auto& row : rig.host.steps)
        row.resize (8);
    rig.connect();
    CHECK_EQ (rig.surface.padLed (0x70).colour, 0);
    tapPad (rig, 8);
    CHECK_EQ ((int) rig.host.steps[0].size(), 8);
}

TEST_CASE ("patterns: Scene Launch switches the pads to pattern selection")
{
    Rig rig;
    rig.connect();
    CHECK_EQ (rig.surface.ccLed (104).colour, (int) palette::amber);
    rig.press1 (104);
    CHECK (rig.controller.padLayer() == Controller::PadLayer::patterns);
    CHECK_EQ (rig.surface.ccLed (104).colour, (int) palette::cyan);
    // pattern 0 (red, current) pulses; pattern 1 (green) dimmed; pattern 2 gets the cycle colour; the rest is dark
    CHECK (rig.surface.padLed (0x60).lighting == Lighting::pulsing);
    CHECK_EQ (rig.surface.padLed (0x60).colour, (int) palette::red);
    CHECK_EQ (rig.surface.padLed (0x61).colour, (int) palette::dimGreen);
    CHECK_EQ (rig.surface.padLed (0x62).colour, (int) palette::dimmed (palette::yellow));
    CHECK_EQ (rig.surface.padLed (0x63).colour, 0);
    tapPad (rig, 1);
    CHECK_EQ (rig.host.pattern, 1);
    CHECK (rig.surface.padLed (0x61).lighting == Lighting::pulsing);
    CHECK (rig.surface.padLed (0x60).lighting == Lighting::stationary);
    tapPad (rig, 7); // no pattern 8
    CHECK_EQ (rig.host.pattern, 1);
    rig.press1 (104);
    CHECK (rig.controller.padLayer() == Controller::PadLayer::steps);
}

TEST_CASE ("drum layout: pads play channels at C5, LEDs show channel colours")
{
    Rig rig;
    rig.host.channels[1].colour = 0xFF0000;
    rig.connect();
    rig.send (SimSurface::padMode (PadMode::drum));
    CHECK (rig.controller.padMode() == PadMode::drum);
    CHECK_EQ (rig.surface.drumLed (40).colour, (int) palette::white);  // pad 1 = channel 0 (selected)
    CHECK_EQ (rig.surface.drumLed (41).colour, (int) palette::red);    // pad 2 = channel 1
    CHECK_EQ (rig.surface.drumLed (48).colour, 0);                     // pad 5 = channel 4: none
    rig.send (SimSurface::drumPad (1, 127));
    rig.send (SimSurface::drumPad (1, 0));
    CHECK_EQ ((int) rig.host.notes.size(), 2);
    CHECK_EQ (rig.host.notes[0].channel, 1);
    CHECK_EQ (rig.host.notes[0].key, 60);
    CHECK (rig.host.notes[0].on);
    CHECK_EQ (rig.host.notes[0].velocity, 1.0f);
    CHECK (! rig.host.notes[1].on);
    rig.send (SimSurface::drumPad (6, 100)); // no channel 6
    CHECK_EQ ((int) rig.host.notes.size(), 2);
}

TEST_CASE ("device select: holding Device Select lets the pads pick a channel")
{
    Rig rig;
    rig.connect();
    rig.send (SimSurface::padMode (PadMode::deviceSelect)); // reported while the button is held
    CHECK_EQ (rig.surface.leds[SimSurface::key ('n', 0x40)].colour, (int) palette::white);
    rig.tap (SimSurface::deviceSelectPad (3, 100), SimSurface::deviceSelectPad (3, 0));
    CHECK_EQ (rig.host.selected, 3);
    CHECK_EQ (rig.surface.leds[SimSurface::key ('n', 0x43)].colour, (int) palette::white);
    rig.send (SimSurface::padMode (PadMode::session)); // released: back to steps, colours resent
    CHECK_EQ (rig.surface.padLed (0x60).colour, (int) palette::dimWhite);
}

TEST_CASE ("pad layout change resends the pad colours")
{
    Rig rig;
    rig.connect();
    rig.send (SimSurface::padMode (PadMode::drum));
    rig.surface.leds.clear();
    rig.send (SimSurface::padMode (PadMode::session));
    CHECK_EQ (rig.surface.padLed (0x60).colour, (int) palette::dimWhite);
    CHECK_EQ (rig.surface.ccLed (0x69).colour, 0); // cc LEDs unchanged, so not resent
}

TEST_CASE ("device mode: pots and faders drive the selected channel's plugin parameters")
{
    Rig rig (Controller::Options { false, true });
    rig.host.selected = 1; // 12 parameters
    rig.connect();
    rig.send (SimSurface::potMode (PotMode::device));
    CHECK (rig.controller.potTarget (0).kind == Parameter::Kind::pluginParam);
    rig.send (SimSurface::pot (2, 127));
    CHECK_EQ ((rig.host.pluginParams[{ 1, 2 }]), 1.0f);
    rig.send (SimSurface::faderMode (FaderMode::device));
    rig.send (SimSurface::fader (1, 0)); // parameter 9 (8 + 1)
    CHECK_EQ ((rig.host.pluginParams[{ 1, 9 }]), 0.0f);
    CHECK (rig.controller.faderTarget (4).kind == Parameter::Kind::none); // parameter 12 does not exist
    CHECK_EQ (rig.surface.paramNames[0x38 + 2], std::string ("Prm 1"));
    // a channel without parameters: nothing
    rig.press (102);
    CHECK_EQ (rig.host.selected, 0);
    CHECK (rig.controller.potTarget (0).kind == Parameter::Kind::none);
    // the master fader stays the master volume in device mode
    rig.send (SimSurface::fader (8, 0));
    CHECK_EQ (rig.host.tracks[0].volume, 0.0f);
}
