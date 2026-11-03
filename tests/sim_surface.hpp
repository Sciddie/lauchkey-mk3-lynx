#pragma once

// A simulated Launchkey MK3 49: builds the messages its controls send on the DAW port and interprets what the host
// sends back (DAW mode flag, LED colours per address, LCD text), so tests can assert on the surface's visible state.

#include <map>
#include <string>

#include "lkmk3/controller.hpp"

#include "fake_host.hpp"

namespace lkmk3::test
{
struct SimLed
{
    int colour = 0;
    Lighting lighting = Lighting::stationary;
};

class SimSurface
{
public:
    bool dawMode = false;
    int inquiries = 0;
    int padModeSelected = -1, potModeSelected = -1, faderModeSelected = -1;
    // key: (status & 0xF0 == 0xB0 ? 'c' : 'n'), number, layout channel group (0 = ch1-3, 9 = ch10-12)
    std::map<std::string, SimLed> leds;
    std::string rows[2];
    std::map<int, std::string> paramNames, paramValues; // control index (0x38.., 0x50..)
    int received = 0;

    static std::string key (char kind, int number, int group = 0)
    {
        return std::string (1, kind) + std::to_string (number) + "/" + std::to_string (group);
    }
    const SimLed& ccLed (int cc) { return leds[key ('c', cc)]; }
    const SimLed& padLed (int note) { return leds[key ('n', note)]; }
    const SimLed& drumLed (int note) { return leds[key ('n', note, 9)]; }

    void receive (const Messages& messages)
    {
        for (const auto& m : messages)
            receive (m);
    }

    void receive (const Bytes& m)
    {
        ++received;
        if (m.empty())
            return;
        if (m[0] == 0xF0)
        {
            if (m.size() == 6 && m[1] == 0x7E)
            {
                ++inquiries;
                return;
            }
            if (m.size() < 8)
                return;
            const auto cmd = m[6];
            std::string text;
            const std::size_t start = cmd == 0x06 ? 7 : 8;
            for (std::size_t i = start; i + 1 < m.size(); ++i)
                text += (char) m[i];
            if (cmd == 0x04) rows[m[7] & 1] = text;
            if (cmd == 0x06) rows[0] = rows[1] = "";
            if (cmd == 0x07) paramNames[m[7]] = text;
            if (cmd == 0x08) paramValues[m[7]] = text;
            return;
        }
        if (m.size() < 3)
            return;
        const auto status = m[0] & 0xF0, channel = m[0] & 0x0F;
        if (status == 0x90 && channel == 15)
        {
            if (m[1] == 0x0C)
                dawMode = m[2] != 0;
            return;
        }
        if (status == 0xB0 && channel == 15)
        {
            if (m[1] == 3) padModeSelected = m[2];
            else if (m[1] == 9) potModeSelected = m[2];
            else if (m[1] == 10) faderModeSelected = m[2];
            else leds[key ('c', m[1])] = { m[2], Lighting::stationary }; // grayscale
            return;
        }
        const auto group = channel >= 9 ? 9 : 0;
        const auto lighting = (Lighting) (channel - group);
        if (status == 0x90 || status == 0x80)
            leds[key ('n', m[1], group)] = { status == 0x80 ? 0 : m[2], lighting };
        else if (status == 0xB0)
            leds[key ('c', m[1], group)] = { m[2], lighting };
    }

    // --- what the controls send ---
    static Bytes button (int cc, bool down = true) { return { 0xBF, (std::uint8_t) cc, (std::uint8_t) (down ? 127 : 0) }; }
    static Bytes button1 (int cc, bool down = true) { return { 0xB0, (std::uint8_t) cc, (std::uint8_t) (down ? 127 : 0) }; }
    static Bytes pot (int i, int v) { return { 0xBF, (std::uint8_t) (0x15 + i), (std::uint8_t) v }; }
    static Bytes fader (int i, int v) { return { 0xBF, (std::uint8_t) (0x35 + i), (std::uint8_t) v }; }
    static Bytes potMode (PotMode m) { return { 0xBF, 0x09, (std::uint8_t) m }; }
    static Bytes faderMode (FaderMode m) { return { 0xBF, 0x0A, (std::uint8_t) m }; }
    static Bytes padMode (PadMode m) { return { 0xBF, 0x03, (std::uint8_t) m }; }
    static Bytes pad (int index, int velocity) { return { 0x90, sessionPadNote (index), (std::uint8_t) velocity }; }
    static Bytes deviceSelectPad (int index, int velocity) { return { 0x90, deviceSelectPadNote (index), (std::uint8_t) velocity }; }
    static Bytes drumPad (int index, int velocity) { return { 0x99, proto::drumNotes[(std::size_t) index], (std::uint8_t) velocity }; }
};

// A controller wired to a simulated surface: press() / turn() send input and apply the controller's answers and a
// refresh, as the LYNX adapter's timer would.
struct Rig
{
    explicit Rig (Controller::Options o = {}) : controller (host, o) {}
    FakeHost host;
    Controller controller;
    SimSurface surface;

    void connect() { surface.receive (controller.connect()); }
    void send (const Bytes& m)
    {
        surface.receive (controller.handle (m));
        surface.receive (controller.refresh());
    }
    void tap (const Bytes& down, const Bytes& up)
    {
        send (down);
        send (up);
    }
    void press (int cc) { tap (SimSurface::button (cc, true), SimSurface::button (cc, false)); }
    void press1 (int cc) { tap (SimSurface::button1 (cc, true), SimSurface::button1 (cc, false)); }
    void refresh() { surface.receive (controller.refresh()); }
};
} // namespace lkmk3::test
