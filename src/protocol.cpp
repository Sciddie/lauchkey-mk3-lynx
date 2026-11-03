#include "lkmk3/protocol.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace lkmk3
{
namespace
{
Event buttonEvent (Button b, int value, int index = 0)
{
    Event e;
    e.type = Event::Type::button;
    e.button = b;
    e.index = index;
    e.value = value;
    e.pressed = value > 0;
    return e;
}

Event decodeCcChannel16 (std::uint8_t cc, std::uint8_t value)
{
    using namespace proto;
    Event e;
    if (cc >= potFirstCc && cc < potFirstCc + 8)
    {
        e.type = Event::Type::pot;
        e.index = cc - potFirstCc;
        e.value = value;
        return e;
    }
    if (cc >= faderFirstCc && cc <= masterFaderCc)
    {
        e.type = Event::Type::fader;
        e.index = cc - faderFirstCc;
        e.value = value;
        return e;
    }
    if (cc >= faderButtonFirstCc && cc < faderButtonFirstCc + 8)
        return buttonEvent (Button::faderButton, value, cc - faderButtonFirstCc);

    switch (cc)
    {
        case padModeCc:
            e.type = Event::Type::padModeReport;
            e.padMode = static_cast<PadMode> (value);
            e.value = value;
            return e;
        case potModeCc:
            e.type = Event::Type::potModeReport;
            e.potMode = static_cast<PotMode> (value);
            e.value = value;
            return e;
        case faderModeCc:
            e.type = Event::Type::faderModeReport;
            e.faderMode = static_cast<FaderMode> (value);
            e.value = value;
            return e;
        case armSelectCc: return buttonEvent (Button::armSelect, value);
        case deviceSelectCc: return buttonEvent (Button::deviceSelect, value);
        case deviceLockCc: return buttonEvent (Button::deviceLock, value);
        case captureMidiCc: return buttonEvent (Button::captureMidi, value);
        case quantiseCc: return buttonEvent (Button::quantise, value);
        case clickCc: return buttonEvent (Button::click, value);
        case undoCc: return buttonEvent (Button::undo, value);
        case trackLeftCc: return buttonEvent (Button::trackLeft, value);
        case trackRightCc: return buttonEvent (Button::trackRight, value);
        case sceneUpCc: return buttonEvent (Button::sceneUp, value);
        case sceneDownCc: return buttonEvent (Button::sceneDown, value);
        case playCc: return buttonEvent (Button::play, value);
        case stopCc: return buttonEvent (Button::stop, value);
        case recordCc: return buttonEvent (Button::record, value);
        case loopCc: return buttonEvent (Button::loop, value);
        // 104 / 105 on channel 16 are the Launchkey Mini MK3 compatibility echoes of Scene Up / Down: the surface also
        // sends 106 / 107 for the same press, so acting on both would move twice.
        case sceneUpAliasCc:
        case sceneDownAliasCc:
        default: return e;
    }
}

Event decodeNote (std::uint8_t channel, std::uint8_t note, std::uint8_t velocity, bool on)
{
    Event e;
    e.value = velocity;
    e.note = note;
    e.pressed = on && velocity > 0;
    if (channel == proto::ch1)
    {
        if (const auto i = sessionPadIndex (note); i >= 0)
        {
            e.type = Event::Type::pad;
            e.index = i;
            return e;
        }
        if (const auto i = deviceSelectPadIndex (note); i >= 0)
        {
            e.type = Event::Type::deviceSelectPad;
            e.index = i;
            return e;
        }
    }
    if (channel == proto::ch10)
        if (const auto i = drumPadIndex (note); i >= 0)
        {
            e.type = Event::Type::drumPad;
            e.index = i;
            return e;
        }
    return {};
}

Event decodeSysex (const std::uint8_t* d, std::size_t n)
{
    // Universal Device Inquiry reply: F0 7E <id> 06 02 00 20 29 <dev_type> <01|11> 00 00 <v1 v2 v3 v4> F7
    if (n >= 17 && d[1] == 0x7E && d[3] == 0x06 && d[4] == 0x02 && d[5] == 0x00 && d[6] == 0x20 && d[7] == 0x29)
    {
        Event e;
        e.type = Event::Type::deviceInquiryReply;
        e.value = d[8];
        e.pressed = d[9] == 0x01; // application (0x11 = bootloader)
        return e;
    }
    return {};
}

Bytes sysex (std::uint8_t command, std::initializer_list<std::uint8_t> args, const Bytes& tail = {})
{
    Bytes b (proto::sysexHeader.begin(), proto::sysexHeader.end());
    b.push_back (command);
    b.insert (b.end(), args.begin(), args.end());
    b.insert (b.end(), tail.begin(), tail.end());
    b.push_back (0xF7);
    return b;
}

// Next code point of a UTF-8 string (invalid sequences yield U+FFFD and skip one byte).
char32_t nextCodePoint (std::string_view s, std::size_t& i)
{
    const auto c = static_cast<unsigned char> (s[i]);
    auto need = 0;
    char32_t cp = 0;
    if (c < 0x80) { ++i; return c; }
    if ((c & 0xE0) == 0xC0) { need = 1; cp = c & 0x1Fu; }
    else if ((c & 0xF0) == 0xE0) { need = 2; cp = c & 0x0Fu; }
    else if ((c & 0xF8) == 0xF0) { need = 3; cp = c & 0x07u; }
    else { ++i; return 0xFFFD; }
    if (i + (std::size_t) need >= s.size()) { ++i; return 0xFFFD; }
    for (int k = 1; k <= need; ++k)
    {
        const auto cc = static_cast<unsigned char> (s[i + (std::size_t) k]);
        if ((cc & 0xC0) != 0x80) { ++i; return 0xFFFD; }
        cp = (cp << 6) | (cc & 0x3Fu);
    }
    i += (std::size_t) need + 1;
    return cp;
}
} // namespace

int sessionPadIndex (int note) noexcept
{
    if (note >= proto::sessionTopFirst && note < proto::sessionTopFirst + 8)
        return note - proto::sessionTopFirst;
    if (note >= proto::sessionBottomFirst && note < proto::sessionBottomFirst + 8)
        return 8 + note - proto::sessionBottomFirst;
    return -1;
}

int deviceSelectPadIndex (int note) noexcept
{
    if (note >= proto::deviceSelectTopFirst && note < proto::deviceSelectTopFirst + 8)
        return note - proto::deviceSelectTopFirst;
    if (note >= proto::deviceSelectBottomFirst && note < proto::deviceSelectBottomFirst + 8)
        return 8 + note - proto::deviceSelectBottomFirst;
    return -1;
}

int drumPadIndex (int note) noexcept
{
    const auto it = std::find (proto::drumNotes.begin(), proto::drumNotes.end(), note);
    return it == proto::drumNotes.end() ? -1 : (int) (it - proto::drumNotes.begin());
}

std::uint8_t sessionPadNote (int index) noexcept
{
    index = std::clamp (index, 0, 15);
    return (std::uint8_t) (index < 8 ? proto::sessionTopFirst + index : proto::sessionBottomFirst + index - 8);
}

std::uint8_t deviceSelectPadNote (int index) noexcept
{
    index = std::clamp (index, 0, 15);
    return (std::uint8_t) (index < 8 ? proto::deviceSelectTopFirst + index : proto::deviceSelectBottomFirst + index - 8);
}

Event decode (const std::uint8_t* data, std::size_t size)
{
    if (data == nullptr || size == 0)
        return {};
    if (data[0] == 0xF0)
        return size >= 2 && data[size - 1] == 0xF7 ? decodeSysex (data, size) : Event {};
    if (size < 3)
        return {};

    const auto status = (std::uint8_t) (data[0] & 0xF0);
    const auto channel = (std::uint8_t) (data[0] & 0x0F);
    const auto d1 = (std::uint8_t) (data[1] & 0x7F), d2 = (std::uint8_t) (data[2] & 0x7F);

    switch (status)
    {
        case proto::noteOn: return decodeNote (channel, d1, d2, true);
        case proto::noteOff: return decodeNote (channel, d1, 0, false);
        case proto::polyPressure:
        {
            auto e = decodeNote (channel, d1, d2, true);
            if (e.type == Event::Type::pad)
                e.type = Event::Type::padPressure;
            else if (e.type == Event::Type::drumPad)
                e.type = Event::Type::drumPressure;
            else
                return {};
            e.pressed = false;
            return e;
        }
        case proto::controlChange:
            if (channel == proto::ch16)
                return decodeCcChannel16 (d1, d2);
            if (channel == proto::ch1)
            {
                if (d1 == proto::sceneLaunchCc) return buttonEvent (Button::sceneLaunch, d2);
                if (d1 == proto::stopSoloMuteCc) return buttonEvent (Button::stopSoloMute, d2);
                if (d1 == proto::shiftCc) return buttonEvent (Button::shift, d2);
                return {};
            }
            if (channel == proto::ch15) // Touch On (127) / Off (0), when enabled
            {
                Event e;
                e.pressed = d2 > 0;
                e.value = d2;
                if (d1 >= proto::potFirstCc && d1 < proto::potFirstCc + 8)
                {
                    e.type = Event::Type::potTouch;
                    e.index = d1 - proto::potFirstCc;
                    return e;
                }
                if (d1 >= proto::faderFirstCc && d1 <= proto::masterFaderCc)
                {
                    e.type = Event::Type::faderTouch;
                    e.index = d1 - proto::faderFirstCc;
                    return e;
                }
            }
            return {};
        default: return {};
    }
}

namespace encode
{
Bytes dawMode (bool on) { return { (std::uint8_t) (proto::noteOn | proto::ch16), proto::dawModeNote, (std::uint8_t) (on ? 0x7F : 0x00) }; }
Bytes touchEvents (bool on) { return { (std::uint8_t) (proto::noteOn | proto::ch16), proto::touchEventsNote, (std::uint8_t) (on ? 0x7F : 0x00) }; }
Bytes potPickup (bool on) { return { (std::uint8_t) (proto::noteOn | proto::ch16), proto::potPickupNote, (std::uint8_t) (on ? 0x7F : 0x00) }; }
Bytes padMode (PadMode m) { return { (std::uint8_t) (proto::controlChange | proto::ch16), proto::padModeCc, (std::uint8_t) m }; }
Bytes potMode (PotMode m) { return { (std::uint8_t) (proto::controlChange | proto::ch16), proto::potModeCc, (std::uint8_t) m }; }
Bytes faderMode (FaderMode m) { return { (std::uint8_t) (proto::controlChange | proto::ch16), proto::faderModeCc, (std::uint8_t) m }; }

Bytes ledColour (const Led& led, std::uint8_t paletteIndex, Lighting lighting)
{
    const auto colour = (std::uint8_t) (paletteIndex & 0x7F);
    const auto mode = (std::uint8_t) lighting;
    switch (led.kind)
    {
        case Led::Kind::drumPad: return { (std::uint8_t) (proto::noteOn | (proto::ch10 + mode)), led.number, colour };
        case Led::Kind::sessionPad:
        case Led::Kind::deviceSelectPad: return { (std::uint8_t) (proto::noteOn | (proto::ch1 + mode)), led.number, colour };
        case Led::Kind::cc:
        default: return { (std::uint8_t) (proto::controlChange | (proto::ch1 + mode)), led.number, colour };
    }
}

Bytes ledGrayscale (std::uint8_t ccIndex, std::uint8_t level)
{
    return { (std::uint8_t) (proto::controlChange | proto::ch16), ccIndex, (std::uint8_t) (level & 0x7F) };
}

Bytes deviceInquiry() { return { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 }; }

Bytes displayText (std::string_view text, std::size_t maxColumns)
{
    Bytes out;
    for (std::size_t i = 0; i < text.size() && out.size() < maxColumns * 2;)
    {
        const auto cp = nextCodePoint (text, i);
        if (cp >= 0x20 && cp <= 0x7E)
            out.push_back ((std::uint8_t) cp);
        else if (cp == 0xB0) // degree sign: ISO-8859-2 upper bank escape (11h) + (B0h - 80h)
        {
            out.push_back (0x11);
            out.push_back (0x30);
        }
        else if (cp == '\t')
            out.push_back (' ');
        else if (cp >= 0x20) // anything else the device can't show
            out.push_back ('?');
        // control characters are dropped
    }
    // count columns: an escape pair is one column
    Bytes truncated;
    std::size_t columns = 0;
    for (std::size_t i = 0; i < out.size() && columns < maxColumns; ++i, ++columns)
    {
        truncated.push_back (out[i]);
        if (out[i] == 0x11 && i + 1 < out.size())
            truncated.push_back (out[++i]);
    }
    return truncated;
}

Bytes setDefaultDisplay (int row, std::string_view text)
{
    return sysex (proto::setDefaultDisplayCmd, { (std::uint8_t) std::clamp (row, 0, 1) }, displayText (text));
}
Bytes clearDefaultDisplay() { return sysex (proto::clearDefaultDisplayCmd, {}); }
Bytes setPotParameterName (int pot, std::string_view text)
{
    return sysex (proto::setParamNameCmd, { (std::uint8_t) (proto::potDisplayFirst + std::clamp (pot, 0, 7)) }, displayText (text));
}
Bytes setPotParameterValue (int pot, std::string_view text)
{
    return sysex (proto::setParamValueCmd, { (std::uint8_t) (proto::potDisplayFirst + std::clamp (pot, 0, 7)) }, displayText (text));
}
Bytes setFaderParameterName (int fader, std::string_view text)
{
    return sysex (proto::setParamNameCmd, { (std::uint8_t) (proto::faderDisplayFirst + std::clamp (fader, 0, 8)) }, displayText (text));
}
Bytes setFaderParameterValue (int fader, std::string_view text)
{
    return sysex (proto::setParamValueCmd, { (std::uint8_t) (proto::faderDisplayFirst + std::clamp (fader, 0, 8)) }, displayText (text));
}
} // namespace encode

bool isDawPortName (std::string_view portName)
{
    std::string n (portName);
    std::transform (n.begin(), n.end(), n.begin(), [] (unsigned char c) { return (char) std::toupper (c); });
    if (n.find ("LKMK3 DAW") != std::string::npos)
        return true;
    if (n.find ("LKMK3") != std::string::npos && (n.find ("MIDIIN2") != std::string::npos || n.find ("MIDIOUT2") != std::string::npos))
        return true;
    return n.find ("LAUNCHKEY MK3") != std::string::npos && n.find ("DAW") != std::string::npos;
}

std::string modelName (int deviceType)
{
    switch (deviceType)
    {
        case 0x34: return "Launchkey MK3 25";
        case 0x35: return "Launchkey MK3 37";
        case 0x36: return "Launchkey MK3 49";
        case 0x37: return "Launchkey MK3 61";
        default: return {};
    }
}

std::string toHex (const Bytes& bytes)
{
    std::string s;
    char buf[4];
    for (auto b : bytes)
    {
        std::snprintf (buf, sizeof buf, "%02X", (unsigned) b);
        if (! s.empty())
            s += ' ';
        s += buf;
    }
    return s;
}
} // namespace lkmk3
