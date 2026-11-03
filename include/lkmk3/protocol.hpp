#pragma once

// Launchkey MK3 DAW-mode protocol (25 / 37 / 49 / 61): decoding what the surface sends on its "LKMK3 DAW" port and
// encoding what the host sends back (DAW mode handshake, mode selection, LED colours, LCD text).
//
// Source of truth: Novation, "Launchkey MK3 Programmer's reference manual" (chapters "DAW mode", "Colouring the
// surface", "Controlling the screen"). Section references in the comments below point there.
//
// This layer knows nothing about any DAW: it turns raw MIDI bytes into typed events and typed commands into bytes.

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lkmk3
{
using Bytes = std::vector<std::uint8_t>;

namespace proto
{
// --- status bytes (channel numbers in the manual are 1-based; nibble = channel - 1) ---
constexpr std::uint8_t noteOff = 0x80, noteOn = 0x90, polyPressure = 0xA0, controlChange = 0xB0;
constexpr std::uint8_t ch1 = 0x0, ch2 = 0x1, ch3 = 0x2, ch10 = 0x9, ch11 = 0xA, ch12 = 0xB, ch15 = 0xE, ch16 = 0xF;

// --- DAW mode control: Note On / Off on channel 16 ("DAW mode control") ---
constexpr std::uint8_t dawModeNote = 0x0C;     // 12: DAW mode enable / disable
constexpr std::uint8_t touchEventsNote = 0x0B; // 11: continuous-control Touch events enable / disable
constexpr std::uint8_t potPickupNote = 0x0A;   // 10: continuous-control Pot Pickup enable / disable

// --- mode report / select: CC on channel 16 ("Mode report and select") ---
constexpr std::uint8_t padModeCc = 0x03, potModeCc = 0x09, faderModeCc = 0x0A;

// --- surface CCs in DAW mode (figure "The Launchkey MK3's surface in DAW mode") ---
// channel 16:
constexpr std::uint8_t potFirstCc = 0x15;       // 21..28: the 8 pots (absolute 0..127)
constexpr std::uint8_t faderFirstCc = 0x35;     // 53..60: faders 1..8; 61 = the 9th (master) fader (LK 49 / 61)
constexpr std::uint8_t masterFaderCc = 0x3D;    // 61
constexpr std::uint8_t faderButtonFirstCc = 0x25; // 37..44: the buttons under faders 1..8 (LK 49 / 61)
constexpr std::uint8_t armSelectCc = 0x2D;      // 45: Arm / Select (white LED)
constexpr std::uint8_t deviceSelectCc = 0x33;   // 51
constexpr std::uint8_t deviceLockCc = 0x34;     // 52 (white LED)
constexpr std::uint8_t captureMidiCc = 0x4A;    // 74
constexpr std::uint8_t quantiseCc = 0x4B;       // 75
constexpr std::uint8_t clickCc = 0x4C;          // 76
constexpr std::uint8_t undoCc = 0x4D;           // 77
constexpr std::uint8_t trackLeftCc = 0x66;      // 102
constexpr std::uint8_t trackRightCc = 0x67;     // 103
constexpr std::uint8_t sceneUpAliasCc = 0x68;   // 104 on ch16: Launchkey Mini MK3 compatibility echo of Scene Up
constexpr std::uint8_t sceneDownAliasCc = 0x69; // 105 on ch16: echo of Scene Down
constexpr std::uint8_t sceneUpCc = 0x6A;        // 106
constexpr std::uint8_t sceneDownCc = 0x6B;      // 107
constexpr std::uint8_t playCc = 0x73, stopCc = 0x74, recordCc = 0x75, loopCc = 0x76; // 115..118
// channel 1:
constexpr std::uint8_t sceneLaunchCc = 0x68;    // 104 (the ► right of the top pad row; RGB LED)
constexpr std::uint8_t stopSoloMuteCc = 0x69;   // 105 (right of the bottom pad row; RGB LED)
constexpr std::uint8_t shiftCc = 0x6C;          // 108

// --- pad note numbers ("Session mode", "Drum mode", "Device Select mode") ---
constexpr std::uint8_t sessionTopFirst = 0x60, sessionBottomFirst = 0x70;       // ch1: 96..103 / 112..119
constexpr std::uint8_t deviceSelectTopFirst = 0x40, deviceSelectBottomFirst = 0x50; // ch1: 64..71 / 80..87
// Drum layout (ch10): top row 40 41 42 43 48 49 50 51, bottom row 36 37 38 39 44 45 46 47
constexpr std::array<std::uint8_t, 16> drumNotes { 40, 41, 42, 43, 48, 49, 50, 51, 36, 37, 38, 39, 44, 45, 46, 47 };

// --- LCD ("Controlling the screen") ---
constexpr std::array<std::uint8_t, 6> sysexHeader { 0xF0, 0x00, 0x20, 0x29, 0x02, 0x0F };
constexpr std::uint8_t setDefaultDisplayCmd = 0x04, clearDefaultDisplayCmd = 0x06;
constexpr std::uint8_t setParamNameCmd = 0x07, setParamValueCmd = 0x08;
constexpr std::uint8_t potDisplayFirst = 0x38;   // 56..63
constexpr std::uint8_t faderDisplayFirst = 0x50; // 80..88 (88 = master fader)
constexpr int displayColumns = 16;
} // namespace proto

// Modes the surface reports (and the host may select) — values from "Mode report and select".
enum class PadMode : std::uint8_t
{
    custom0 = 0, drum = 1, session = 2, scaleChords = 3, userChords = 4,
    customMode0 = 5, customMode1 = 6, customMode2 = 7, customMode3 = 8,
    deviceSelect = 9, navigation = 10
};
enum class PotMode : std::uint8_t
{
    custom0 = 0, volume = 1, device = 2, pan = 3, sendA = 4, sendB = 5,
    customMode0 = 6, customMode1 = 7, customMode2 = 8, customMode3 = 9
};
enum class FaderMode : std::uint8_t
{
    custom0 = 0, volume = 1, device = 2, sendA = 4, sendB = 5,
    customMode0 = 6, customMode1 = 7, customMode2 = 8, customMode3 = 9
};

// Buttons that report as CC (value 127 = pressed, 0 = released).
enum class Button : std::uint8_t
{
    play, stop, record, loop,
    captureMidi, quantise, click, undo,
    trackLeft, trackRight, sceneUp, sceneDown,
    sceneLaunch, stopSoloMute, shift,
    deviceSelect, deviceLock, armSelect,
    faderButton, // `index` 0..7
};

// One decoded message from the surface.
struct Event
{
    enum class Type : std::uint8_t
    {
        none,          // not a DAW-mode surface message (or an echo we deliberately ignore)
        button,        // button, index (fader buttons), pressed
        pot,           // index 0..7, value 0..127
        fader,         // index 0..8 (8 = master), value 0..127
        potTouch,      // index, pressed = touched (needs Touch events enabled)
        faderTouch,    // index, pressed = touched
        pad,           // index 0..15 (0..7 top row, 8..15 bottom row), value = velocity, pressed (Session layout)
        padPressure,   // index, value (Session layout, polyphonic aftertouch)
        drumPad,       // index 0..15, note, value = velocity, pressed (Drum layout, channel 10)
        drumPressure,  // index, note, value
        deviceSelectPad, // index 0..15, value, pressed (Device Select layout, while Device Select is held)
        padModeReport,
        potModeReport,
        faderModeReport,
        deviceInquiryReply, // value = device type (0x34..0x37), see modelName()
    };

    Type type = Type::none;
    Button button = Button::play;
    int index = 0;
    int value = 0;
    int note = 0;
    bool pressed = false;
    PadMode padMode = PadMode::session;
    PotMode potMode = PotMode::pan;
    FaderMode faderMode = FaderMode::volume;

    bool operator== (const Event&) const = default;
};

// Decodes one complete MIDI message (running status already resolved; SysEx including F0 .. F7).
Event decode (const std::uint8_t* data, std::size_t size);
inline Event decode (const Bytes& message) { return decode (message.data(), message.size()); }

// Pad index (0..15; 0..7 top row left to right, 8..15 bottom row) <-> note per layout. -1 / nullopt when outside.
int sessionPadIndex (int note) noexcept;
int deviceSelectPadIndex (int note) noexcept;
int drumPadIndex (int note) noexcept;
std::uint8_t sessionPadNote (int index) noexcept;
std::uint8_t deviceSelectPadNote (int index) noexcept;

// --- LED colours ("Colouring the surface") ---
enum class Lighting : std::uint8_t
{
    stationary = 0, // channel 1 (drum layout: 10)
    flashing = 1,   // channel 2 (11): flashes between the stationary / pulsing colour and this one, one beat period
    pulsing = 2,    // channel 3 (12): dark <-> full, two-beat period
};

// A LED on the surface, addressed the way the manual does: a pad note in a layout, or a CC index.
struct Led
{
    enum class Kind : std::uint8_t { sessionPad, drumPad, deviceSelectPad, cc };
    Kind kind = Kind::cc;
    std::uint8_t number = 0; // note (pads) or CC index (buttons)
    std::uint8_t ccChannel = proto::ch1; // which channel the CC LED reports on (ch1 or ch16), used for grayscale only

    bool operator== (const Led&) const = default;
    auto operator<=> (const Led&) const = default;
};

namespace encode
{
Bytes dawMode (bool on);
Bytes touchEvents (bool on);
Bytes potPickup (bool on);
Bytes padMode (PadMode mode);
Bytes potMode (PotMode mode);
Bytes faderMode (FaderMode mode);
// Palette colour 0..127 (0 = off); for CC-addressed LEDs.
Bytes ledColour (const Led& led, std::uint8_t paletteIndex, Lighting lighting = Lighting::stationary);
// Stationary grayscale 0..127 on a CC-addressed LED (channel 16; "CC associated controls only").
Bytes ledGrayscale (std::uint8_t ccIndex, std::uint8_t level);
Bytes deviceInquiry();

// LCD. Text is converted to the device's character set (ASCII; other characters become '?', except '°' which the
// device implements via its ISO-8859-2 escape). Rows are 0 (top) / 1 (bottom); longer text is truncated to 16.
Bytes setDefaultDisplay (int row, std::string_view utf8Text);
Bytes clearDefaultDisplay();
// Control index: pots 0..7, faders 0..8 (8 = master).
Bytes setPotParameterName (int pot, std::string_view utf8Text);
Bytes setPotParameterValue (int pot, std::string_view utf8Text);
Bytes setFaderParameterName (int fader, std::string_view utf8Text);
Bytes setFaderParameterValue (int fader, std::string_view utf8Text);
// The byte string the device shows for `utf8Text` (exposed for tests).
Bytes displayText (std::string_view utf8Text, std::size_t maxColumns = proto::displayColumns);
} // namespace encode

// "Launchkey MK3 49" for device type 0x36, empty when unknown.
std::string modelName (int deviceType);

// Human-readable dump ("9F 0C 7F") for logs and test messages.
std::string toHex (const Bytes& bytes);
} // namespace lkmk3
