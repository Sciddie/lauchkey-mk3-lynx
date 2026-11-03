#include "test.hpp"

#include "lkmk3/protocol.hpp"

using namespace lkmk3;

namespace
{
Event cc (int channel1Based, int number, int value)
{
    return decode (Bytes { (std::uint8_t) (0xB0 | (channel1Based - 1)), (std::uint8_t) number, (std::uint8_t) value });
}
} // namespace

TEST_CASE ("protocol: DAW mode handshake bytes match the manual")
{
    CHECK_EQ (toHex (encode::dawMode (true)), std::string ("9F 0C 7F"));
    CHECK_EQ (toHex (encode::dawMode (false)), std::string ("9F 0C 00"));
    CHECK_EQ (toHex (encode::touchEvents (true)), std::string ("9F 0B 7F"));
    CHECK_EQ (toHex (encode::potPickup (false)), std::string ("9F 0A 00"));
    CHECK_EQ (toHex (encode::deviceInquiry()), std::string ("F0 7E 7F 06 01 F7"));
}

TEST_CASE ("protocol: mode select messages")
{
    CHECK_EQ (toHex (encode::padMode (PadMode::session)), std::string ("BF 03 02"));
    CHECK_EQ (toHex (encode::potMode (PotMode::volume)), std::string ("BF 09 01"));
    CHECK_EQ (toHex (encode::faderMode (FaderMode::sendB)), std::string ("BF 0A 05"));
}

TEST_CASE ("protocol: mode reports decode")
{
    auto e = cc (16, 3, 1);
    CHECK (e.type == Event::Type::padModeReport);
    CHECK (e.padMode == PadMode::drum);
    e = cc (16, 9, 4);
    CHECK (e.type == Event::Type::potModeReport);
    CHECK (e.potMode == PotMode::sendA);
    e = cc (16, 10, 2);
    CHECK (e.type == Event::Type::faderModeReport);
    CHECK (e.faderMode == FaderMode::device);
}

TEST_CASE ("protocol: transport and navigation buttons (channel 16)")
{
    struct { int cc; Button b; } table[] = {
        { 115, Button::play }, { 116, Button::stop }, { 117, Button::record }, { 118, Button::loop },
        { 74, Button::captureMidi }, { 75, Button::quantise }, { 76, Button::click }, { 77, Button::undo },
        { 102, Button::trackLeft }, { 103, Button::trackRight }, { 106, Button::sceneUp }, { 107, Button::sceneDown },
        { 51, Button::deviceSelect }, { 52, Button::deviceLock }, { 45, Button::armSelect },
    };
    for (const auto& row : table)
    {
        const auto down = cc (16, row.cc, 127), up = cc (16, row.cc, 0);
        CHECK (down.type == Event::Type::button);
        CHECK (down.button == row.b);
        CHECK (down.pressed);
        CHECK (up.type == Event::Type::button);
        CHECK (! up.pressed);
    }
}

TEST_CASE ("protocol: channel 1 buttons (scene launch, stop/solo/mute, shift)")
{
    CHECK (cc (1, 104, 127).button == Button::sceneLaunch);
    CHECK (cc (1, 105, 127).button == Button::stopSoloMute);
    CHECK (cc (1, 108, 127).button == Button::shift);
    CHECK (cc (1, 108, 0).type == Event::Type::button);
    CHECK (cc (1, 21, 64).type == Event::Type::none); // pots are on channel 16 only
}

TEST_CASE ("protocol: the Mini MK3 compatibility echoes of Scene Up/Down are ignored")
{
    CHECK (cc (16, 104, 127).type == Event::Type::none);
    CHECK (cc (16, 105, 127).type == Event::Type::none);
}

TEST_CASE ("protocol: pots, faders, master fader, fader buttons")
{
    for (int i = 0; i < 8; ++i)
    {
        const auto p = cc (16, 21 + i, 10 + i);
        CHECK (p.type == Event::Type::pot);
        CHECK_EQ (p.index, i);
        CHECK_EQ (p.value, 10 + i);
        const auto b = cc (16, 37 + i, 127);
        CHECK (b.type == Event::Type::button);
        CHECK (b.button == Button::faderButton);
        CHECK_EQ (b.index, i);
    }
    for (int i = 0; i < 9; ++i)
    {
        const auto f = cc (16, 53 + i, 100);
        CHECK (f.type == Event::Type::fader);
        CHECK_EQ (f.index, i);
    }
    CHECK (cc (16, 62, 1).type == Event::Type::none);
}

TEST_CASE ("protocol: touch events on channel 15")
{
    auto t = cc (15, 0x15, 127);
    CHECK (t.type == Event::Type::potTouch);
    CHECK_EQ (t.index, 0);
    CHECK (t.pressed);
    t = cc (15, 0x35, 0);
    CHECK (t.type == Event::Type::faderTouch);
    CHECK (! t.pressed);
}

TEST_CASE ("protocol: pad layouts round-trip")
{
    for (int i = 0; i < 16; ++i)
    {
        CHECK_EQ (sessionPadIndex (sessionPadNote (i)), i);
        CHECK_EQ (deviceSelectPadIndex (deviceSelectPadNote (i)), i);
        CHECK_EQ (drumPadIndex (proto::drumNotes[(std::size_t) i]), i);
    }
    CHECK_EQ ((int) sessionPadNote (0), 0x60);  // upper left (manual example "flashing the upper left pad")
    CHECK_EQ ((int) sessionPadNote (8), 0x70);  // lower left ("lighting the lower left pad")
    CHECK_EQ ((int) sessionPadNote (15), 0x77); // lower right
    CHECK_EQ (sessionPadIndex (0x68), -1);
    CHECK_EQ (drumPadIndex (52), -1);
}

TEST_CASE ("protocol: pad notes decode (session, device select, drum, note-off forms)")
{
    auto e = decode (Bytes { 0x90, 0x70, 100 });
    CHECK (e.type == Event::Type::pad);
    CHECK_EQ (e.index, 8);
    CHECK (e.pressed);
    e = decode (Bytes { 0x90, 0x70, 0 }); // note-on velocity 0 = release (the device sends this)
    CHECK (e.type == Event::Type::pad);
    CHECK (! e.pressed);
    e = decode (Bytes { 0x80, 0x60, 64 });
    CHECK (e.type == Event::Type::pad);
    CHECK (! e.pressed);
    e = decode (Bytes { 0x90, 0x41, 90 });
    CHECK (e.type == Event::Type::deviceSelectPad);
    CHECK_EQ (e.index, 1);
    e = decode (Bytes { 0x99, 36, 80 });
    CHECK (e.type == Event::Type::drumPad);
    CHECK_EQ (e.index, 8);
    CHECK_EQ (e.note, 36);
    e = decode (Bytes { 0xA0, 0x61, 33 });
    CHECK (e.type == Event::Type::padPressure);
    CHECK_EQ (e.value, 33);
    CHECK (decode (Bytes { 0x90, 60, 100 }).type == Event::Type::none); // not a pad note
    CHECK (decode (Bytes { 0x95, 0x60, 100 }).type == Event::Type::none);
}

TEST_CASE ("protocol: LED colour examples from the manual")
{
    using K = Led::Kind;
    CHECK_EQ (toHex (encode::ledColour ({ K::sessionPad, 0x70 }, 5)), std::string ("90 70 05"));
    CHECK_EQ (toHex (encode::ledColour ({ K::sessionPad, 0x60 }, 0x13, Lighting::flashing)), std::string ("91 60 13"));
    CHECK_EQ (toHex (encode::ledColour ({ K::sessionPad, 0x77 }, 0x2D, Lighting::pulsing)), std::string ("92 77 2D"));
    CHECK_EQ (toHex (encode::ledColour ({ K::sessionPad, 0x77 }, 0)), std::string ("90 77 00"));
    CHECK_EQ (toHex (encode::ledColour ({ K::drumPad, 36 }, 21)), std::string ("99 24 15"));
    CHECK_EQ (toHex (encode::ledColour ({ K::drumPad, 36 }, 21, Lighting::pulsing)), std::string ("9B 24 15"));
    CHECK_EQ (toHex (encode::ledColour ({ K::cc, 0x68 }, 21)), std::string ("B0 68 15"));
    CHECK_EQ (toHex (encode::ledGrayscale (0x2D, 127)), std::string ("BF 2D 7F"));
}

TEST_CASE ("protocol: LCD sysex")
{
    CHECK_EQ (toHex (encode::clearDefaultDisplay()), std::string ("F0 00 20 29 02 0F 06 F7"));
    CHECK_EQ (toHex (encode::setDefaultDisplay (1, "Hi")), std::string ("F0 00 20 29 02 0F 04 01 48 69 F7"));
    CHECK_EQ (toHex (encode::setPotParameterName (0, "Vol")), std::string ("F0 00 20 29 02 0F 07 38 56 6F 6C F7"));
    CHECK_EQ (toHex (encode::setFaderParameterValue (8, "0")), std::string ("F0 00 20 29 02 0F 08 58 30 F7"));
    CHECK_EQ (encode::displayText ("0123456789abcdefXYZ").size(), (std::size_t) 16);
    CHECK_EQ (toHex (encode::displayText ("\xC3\xA9" "a")), std::string ("3F 61"));     // é -> ?
    CHECK_EQ (toHex (encode::displayText ("5\xC2\xB0")), std::string ("35 11 30"));      // degree sign escape
    CHECK_EQ (toHex (encode::displayText ("a\nb")), std::string ("61 62"));             // control chars dropped
    CHECK_EQ (toHex (encode::displayText ("\xE2\x82")), std::string ("3F 3F"));         // truncated UTF-8
}

TEST_CASE ("protocol: device inquiry reply identifies the model")
{
    const Bytes reply { 0xF0, 0x7E, 0x00, 0x06, 0x02, 0x00, 0x20, 0x29, 0x36, 0x01, 0x00, 0x00, 0, 1, 2, 3, 0xF7 };
    const auto e = decode (reply);
    CHECK (e.type == Event::Type::deviceInquiryReply);
    CHECK_EQ (e.value, 0x36);
    CHECK_EQ (modelName (e.value), std::string ("Launchkey MK3 49"));
    CHECK (modelName (0x10).empty());
    CHECK (decode (Bytes { 0xF0, 0x7E }).type == Event::Type::none);
}

TEST_CASE ("protocol: robust against short or empty input")
{
    CHECK (decode (nullptr, 0).type == Event::Type::none);
    CHECK (decode (Bytes { 0xBF }).type == Event::Type::none);
    CHECK (decode (Bytes { 0xBF, 0x73 }).type == Event::Type::none);
    CHECK (decode (Bytes { 0xF8 }).type == Event::Type::none);
}
