#pragma once

// The Launchkey MK3 <-> DAW mapping. Feed it what arrives on the surface's DAW port (handle), call refresh()
// regularly (e.g. 30 times a second) for LED / LCD feedback, and send everything it returns to the DAW port.
// It owns no I/O and no timer, so tests drive it with simulated MIDI.

#include <array>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "lkmk3/daw_host.hpp"
#include "lkmk3/protocol.hpp"

namespace lkmk3
{
using Messages = std::vector<Bytes>;

class Controller
{
public:
    struct Options
    {
        // Soft takeover: a pot / fader whose position differs from the parameter only takes over once it reaches
        // (or crosses) the parameter's value, so absolute controls don't make the value jump.
        bool softTakeover = true;
        // Show transport state / bank / names on the 16x2 LCD.
        bool useDisplay = true;
    };

    static constexpr int potCount = 8;
    static constexpr int faderCount = 9; // 8 + master (LK 49 / 61)
    static constexpr int bankSize = 8;

    explicit Controller (DawHost& host);
    Controller (DawHost& host, Options options);

    // DAW-mode handshake. connect(): enter DAW mode, initialise the pad / pot / fader modes, ask the device who it
    // is, then a full feedback refresh. disconnect(): release the display and return the surface to Standalone mode.
    Messages connect();
    Messages disconnect();
    bool isConnected() const noexcept { return connected; }

    // One complete MIDI message from the DAW port. Returns messages to send back (often none).
    Messages handle (const Bytes& message);
    // LED / LCD feedback for whatever changed since the last call (everything after connect()).
    Messages refresh();

    // --- state, for the adapter's UI and the tests ---
    int mixerBankStart() const noexcept { return bankStart; } // first insert of the 8 strips (>= 1)
    PotMode potMode() const noexcept { return pots; }
    FaderMode faderMode() const noexcept { return faders; }
    PadMode padMode() const noexcept { return pads; }
    bool faderButtonsArm() const noexcept { return armMode; }
    bool shiftHeld() const noexcept { return shift; }
    int deviceType() const noexcept { return model; } // from the device inquiry reply, 0 until known
    // What pot / fader `index` drives right now (Kind::none when nothing).
    Parameter potTarget (int index) const;
    Parameter faderTarget (int index) const;

    // Moves the mixer bank so `track` is among the 8 strips.
    void showTrack (int track);

private:
    struct Takeover
    {
        Parameter target;
        bool caught = false;
        int lastHardware = -1;
        float lastSent = -1.0f;
    };

    struct LedState
    {
        std::uint8_t colour = 0;
        Lighting lighting = Lighting::stationary;
        bool operator== (const LedState&) const = default;
    };

    void onButton (const Event& e, Messages& out);
    void onContinuous (Takeover& slot, const Parameter& target, int value);
    void moveBank (int delta);
    void selectChannelBy (int delta);

    std::map<Led, LedState> desiredLeds() const;
    std::map<int, std::string> desiredTexts() const;
    std::uint8_t trackColour (int track) const;
    std::uint8_t transportColour (Lighting& lighting) const;

    DawHost& host;
    Options options;
    bool connected = false;
    bool shift = false;
    bool armMode = false;
    int bankStart = 1;
    int model = 0;
    PadMode pads = PadMode::session;
    PotMode pots = PotMode::pan;
    FaderMode faders = FaderMode::volume;
    std::array<Takeover, potCount> potSlots {};
    std::array<Takeover, faderCount> faderSlots {};
    std::map<Led, LedState> sentLeds;
    std::map<int, std::string> sentTexts; // display key -> text (see desiredTexts)
};
} // namespace lkmk3
