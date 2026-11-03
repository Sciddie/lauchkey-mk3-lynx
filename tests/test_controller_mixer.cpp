#include "test.hpp"

#include "lkmk3/palette.hpp"
#include "sim_surface.hpp"

using namespace lkmk3;
using namespace lkmk3::test;

TEST_CASE ("handshake: connect enters DAW mode and initialises the surface")
{
    Rig rig;
    CHECK (! rig.surface.dawMode);
    rig.connect();
    CHECK (rig.controller.isConnected());
    CHECK (rig.surface.dawMode);
    CHECK_EQ (rig.surface.padModeSelected, (int) PadMode::session);
    CHECK_EQ (rig.surface.potModeSelected, (int) PotMode::pan);
    CHECK_EQ (rig.surface.faderModeSelected, (int) FaderMode::volume);
    CHECK_EQ (rig.surface.inquiries, 1);
}

TEST_CASE ("handshake: the first message is the DAW mode enable")
{
    FakeHost host;
    Controller c (host);
    const auto out = c.connect();
    CHECK (! out.empty());
    CHECK_EQ (toHex (out.front()), std::string ("9F 0C 7F"));
}

TEST_CASE ("handshake: disconnect releases the display and leaves DAW mode last")
{
    Rig rig;
    rig.connect();
    const auto out = rig.controller.disconnect();
    CHECK (out.size() >= 2);
    CHECK_EQ (toHex (out.back()), std::string ("9F 0C 00"));
    CHECK_EQ (toHex (out.front()), std::string ("F0 00 20 29 02 0F 06 F7"));
    rig.surface.receive (out);
    CHECK (! rig.surface.dawMode);
    CHECK (! rig.controller.isConnected());
    CHECK (rig.controller.disconnect().empty()); // idempotent
}

TEST_CASE ("handshake: input is ignored while not connected; reconnect resends everything")
{
    Rig rig;
    rig.controller.handle (SimSurface::button (115));
    CHECK (! rig.host.playing);
    rig.connect();
    rig.surface.receive (rig.controller.disconnect());
    rig.surface.leds.clear();
    rig.connect();
    CHECK (rig.surface.ccLed (0x69).colour != 0); // the transport LED is sent again after reconnecting
}

TEST_CASE ("handshake: device inquiry reply records the model")
{
    Rig rig;
    rig.connect();
    rig.send ({ 0xF0, 0x7E, 0x00, 0x06, 0x02, 0x00, 0x20, 0x29, 0x36, 0x01, 0x00, 0x00, 0, 1, 2, 3, 0xF7 });
    CHECK_EQ (rig.controller.deviceType(), 0x36);
}

TEST_CASE ("transport: play toggles, stop stops, record and loop toggle")
{
    Rig rig;
    rig.connect();
    rig.press (115);
    CHECK (rig.host.playing);
    rig.press (115);
    CHECK (! rig.host.playing);
    rig.press (115);
    rig.press (116);
    CHECK (! rig.host.playing);
    rig.press (117);
    CHECK (rig.host.recording);
    rig.press (117);
    CHECK (! rig.host.recording);
    rig.press (118);
    CHECK (rig.host.songMode);
    rig.press (118);
    CHECK (! rig.host.songMode);
}

TEST_CASE ("transport: acts on press only, not on release")
{
    Rig rig;
    rig.connect();
    rig.send (SimSurface::button (115, true));
    CHECK_EQ (rig.host.playCalls, 1);
    rig.send (SimSurface::button (115, false));
    CHECK_EQ (rig.host.playCalls, 1);
    CHECK (rig.host.playing);
}

TEST_CASE ("transport: click toggles the metronome, undo / shift+undo redo")
{
    Rig rig;
    rig.connect();
    rig.press (76);
    CHECK (rig.host.metronome);
    rig.press (77);
    CHECK_EQ (rig.host.undos, 1);
    rig.send (SimSurface::button1 (108, true)); // shift held
    rig.press (77);
    rig.send (SimSurface::button1 (108, false));
    CHECK_EQ (rig.host.redos, 1);
    CHECK_EQ (rig.host.undos, 1);
}

TEST_CASE ("transport: LED feedback on Stop/Solo/Mute and the LCD")
{
    Rig rig;
    rig.connect();
    CHECK_EQ (rig.surface.ccLed (0x69).colour, (int) palette::dimWhite);
    CHECK (rig.surface.rows[0].find ("STOP") == 0);
    rig.press (115);
    CHECK_EQ (rig.surface.ccLed (0x69).colour, (int) palette::green);
    CHECK (rig.surface.rows[0].find ("PLAY") == 0);
    rig.press (117);
    CHECK_EQ (rig.surface.ccLed (0x69).colour, (int) palette::red);
    CHECK (rig.surface.ccLed (0x69).lighting == Lighting::pulsing);
    CHECK (rig.surface.rows[0].find ("REC PLAY PAT") == 0);
    rig.press (116);
    CHECK (rig.surface.ccLed (0x69).lighting == Lighting::stationary); // armed but stopped: steady red
    CHECK_EQ (rig.surface.ccLed (0x69).colour, (int) palette::red);
    // a change made in the DAW itself shows up on the next refresh
    rig.host.recording = false;
    rig.host.songMode = true;
    rig.refresh();
    CHECK_EQ (rig.surface.ccLed (0x69).colour, (int) palette::dimWhite);
    CHECK (rig.surface.rows[0].find ("STOP SONG") == 0);
}

TEST_CASE ("feedback: refresh only sends what changed")
{
    Rig rig;
    rig.connect();
    CHECK (rig.controller.refresh().empty());
    rig.host.playing = true;
    const auto out = rig.controller.refresh();
    CHECK_EQ (out.size(), (std::size_t) 2); // the transport LED and the LCD's top row
}

TEST_CASE ("mixer: faders drive the volumes of the bank, the 9th fader the master")
{
    Rig rig (Controller::Options { false, true }); // no soft takeover: values follow immediately
    rig.connect();
    rig.send (SimSurface::fader (0, 127));
    CHECK_EQ (rig.host.tracks[1].volume, 1.0f);
    rig.send (SimSurface::fader (7, 0));
    CHECK_EQ (rig.host.tracks[8].volume, 0.0f);
    rig.send (SimSurface::fader (8, 0));
    CHECK_EQ (rig.host.tracks[0].volume, 0.0f);
}

TEST_CASE ("mixer: pots follow the reported pot mode (pan default, volume, sends)")
{
    Rig rig (Controller::Options { false, true });
    rig.connect();
    rig.send (SimSurface::pot (0, 0));
    CHECK_EQ (rig.host.tracks[1].pan, 0.0f);
    rig.send (SimSurface::potMode (PotMode::volume));
    CHECK (rig.controller.potMode() == PotMode::volume);
    rig.send (SimSurface::pot (1, 127));
    CHECK_EQ (rig.host.tracks[2].volume, 1.0f);
    rig.send (SimSurface::potMode (PotMode::sendA));
    rig.send (SimSurface::pot (0, 127));
    CHECK_EQ (rig.host.tracks[1].sends[0], 1.0f);
    // a track without a second send: Send-B does nothing
    rig.send (SimSurface::potMode (PotMode::sendB));
    const auto writes = rig.host.parameterWrites;
    rig.send (SimSurface::pot (0, 10));
    CHECK_EQ (rig.host.parameterWrites, writes);
    // a custom mode is not a mixer mode
    rig.send (SimSurface::potMode (PotMode::custom0));
    CHECK (rig.controller.potTarget (0).kind == Parameter::Kind::none);
}

TEST_CASE ("mixer: fader send modes")
{
    Rig rig (Controller::Options { false, true });
    rig.connect();
    rig.send (SimSurface::faderMode (FaderMode::sendA));
    rig.send (SimSurface::fader (0, 0));
    CHECK_EQ (rig.host.tracks[1].sends[0], 0.0f);
    CHECK_EQ (rig.host.tracks[1].volume, 0.8f);
    rig.send (SimSurface::fader (8, 127)); // master stays volume
    CHECK_EQ (rig.host.tracks[0].volume, 1.0f);
}

TEST_CASE ("mixer: shift + track buttons move the bank by 8, clamped")
{
    Rig rig (Controller::Options { false, true });
    rig.connect();
    CHECK_EQ (rig.controller.mixerBankStart(), 1);
    rig.send (SimSurface::button1 (108, true));
    rig.press (103);
    CHECK_EQ (rig.controller.mixerBankStart(), 9);
    rig.press (103);
    CHECK_EQ (rig.controller.mixerBankStart(), 17); // inserts 17..20 (21 tracks with the master)
    rig.press (103);
    CHECK_EQ (rig.controller.mixerBankStart(), 17);
    rig.press (102);
    rig.press (102);
    rig.press (102);
    CHECK_EQ (rig.controller.mixerBankStart(), 1);
    rig.send (SimSurface::button1 (108, false));
    rig.press (103);
    rig.press (103); // without shift: channel selection moves, not the bank
    rig.send (SimSurface::fader (0, 0));
    CHECK_EQ (rig.host.tracks[1].volume, 0.0f);
    CHECK_EQ (rig.host.selected, 2);
}

TEST_CASE ("mixer: the bank follows on the LCD and the faders past the last insert do nothing")
{
    Rig rig (Controller::Options { false, true });
    rig.connect();
    CHECK (rig.surface.rows[1].find ("1-8") != std::string::npos);
    rig.controller.showTrack (18);
    CHECK_EQ (rig.controller.mixerBankStart(), 17);
    rig.refresh();
    CHECK (rig.surface.rows[1].find ("17-20") != std::string::npos);
    CHECK (rig.controller.faderTarget (4).kind == Parameter::Kind::none); // insert 21 does not exist
    CHECK_EQ (rig.surface.paramNames[0x50 + 4], std::string ("-"));
    CHECK_EQ (rig.surface.paramNames[0x50], std::string ("Vol 17"));
}

TEST_CASE ("mixer: parameter names and values go to the LCD and follow changes")
{
    Rig rig (Controller::Options { false, true });
    rig.connect();
    CHECK_EQ (rig.surface.paramNames[0x38], std::string ("Pan 1"));
    CHECK_EQ (rig.surface.paramValues[0x38], std::string ("50%"));
    CHECK_EQ (rig.surface.paramNames[0x58], std::string ("Vol 0"));
    rig.send (SimSurface::pot (0, 127));
    CHECK_EQ (rig.surface.paramValues[0x38], std::string ("100%"));
    rig.send (SimSurface::potMode (PotMode::volume));
    CHECK_EQ (rig.surface.paramNames[0x38], std::string ("Vol 1"));
}

TEST_CASE ("soft takeover: a far-away control waits until it reaches the value")
{
    Rig rig; // soft takeover on by default
    rig.connect();
    // insert 1 volume is 0.8 (~ 102/127)
    rig.send (SimSurface::fader (0, 10));
    CHECK_EQ (rig.host.tracks[1].volume, 0.8f);
    rig.send (SimSurface::fader (0, 60));
    CHECK_EQ (rig.host.tracks[1].volume, 0.8f);
    rig.send (SimSurface::fader (0, 110)); // crossed 0.8 on the way: takes over
    CHECK_EQ (rig.host.tracks[1].volume, 110.0f / 127.0f);
    rig.send (SimSurface::fader (0, 50)); // caught: follows directly now
    CHECK_EQ (rig.host.tracks[1].volume, 50.0f / 127.0f);
}

TEST_CASE ("soft takeover: a value moved elsewhere must be picked up again")
{
    Rig rig;
    rig.connect();
    rig.send (SimSurface::fader (0, 102)); // ~0.803: within the window, caught
    CHECK_EQ (rig.host.tracks[1].volume, 102.0f / 127.0f);
    rig.host.tracks[1].volume = 0.2f; // the mouse moved it
    rig.refresh();
    rig.send (SimSurface::fader (0, 100));
    CHECK_EQ (rig.host.tracks[1].volume, 0.2f);
    rig.send (SimSurface::fader (0, 20)); // crossed 0.2 (25.4) going down
    CHECK_EQ (rig.host.tracks[1].volume, 20.0f / 127.0f);
}

TEST_CASE ("soft takeover: a new target (bank change) starts uncaught")
{
    Rig rig;
    rig.connect();
    rig.send (SimSurface::fader (0, 102));
    CHECK_EQ (rig.host.tracks[1].volume, 102.0f / 127.0f);
    rig.controller.showTrack (9);
    rig.send (SimSurface::fader (0, 0));
    CHECK_EQ (rig.host.tracks[9].volume, 0.8f);
}

TEST_CASE ("fader buttons: mute by default, shift = solo, Arm/Select switches to record arm")
{
    Rig rig;
    rig.connect();
    rig.press (37);
    CHECK (rig.host.tracks[1].muted);
    CHECK_EQ (rig.surface.ccLed (37).colour, 0);
    rig.press (37);
    CHECK (! rig.host.tracks[1].muted);
    CHECK_EQ (rig.surface.ccLed (37).colour, (int) palette::green);
    rig.send (SimSurface::button1 (108, true));
    rig.press (38);
    rig.send (SimSurface::button1 (108, false));
    CHECK (rig.host.tracks[2].soloed);
    CHECK (rig.surface.ccLed (38).lighting == Lighting::pulsing);
    CHECK_EQ (rig.surface.ccLed (45).colour, (int) palette::dimWhite);
    rig.press (45);
    CHECK (rig.controller.faderButtonsArm());
    CHECK_EQ (rig.surface.ccLed (45).colour, (int) palette::white);
    CHECK_EQ (rig.surface.ccLed (39).colour, (int) palette::dimRed);
    rig.press (39);
    CHECK (rig.host.tracks[3].armed);
    CHECK_EQ (rig.surface.ccLed (39).colour, (int) palette::red);
    CHECK (! rig.host.tracks[3].muted);
}

TEST_CASE ("fader buttons: LEDs use the track colour")
{
    Rig rig;
    rig.host.tracks[4].colour = 0x0000FF;
    rig.connect();
    CHECK_EQ (rig.surface.ccLed (40).colour, (int) palette::blue);
}

TEST_CASE ("channel select: track buttons move the Channel Rack selection, clamped")
{
    Rig rig;
    rig.connect();
    rig.press (103);
    CHECK_EQ (rig.host.selected, 1);
    rig.press (103);
    rig.press (103);
    rig.press (103);
    CHECK_EQ (rig.host.selected, 3);
    rig.press (102);
    CHECK_EQ (rig.host.selected, 2);
    CHECK (rig.surface.rows[1].find ("Chan 2") == 0);
}

TEST_CASE ("scene up/down echo on ch16 104/105 does not act twice")
{
    Rig rig;
    rig.connect();
    rig.send (SimSurface::button (104, true));
    rig.send (SimSurface::button (105, true));
    CHECK (! rig.host.playing); // 105 on ch16 is not Stop/Solo/Mute (that one is on ch1)
}
