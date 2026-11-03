#pragma once

// What the Launchkey mapping needs from a DAW. The mapping layer (controller.hpp) only talks to this interface, so it
// can be unit-tested against a fake and adapted to LYNX SHARE (lynx/LynxDawHost.*) without either knowing the other.
//
// Threading: every call happens on the thread that drives the Controller (in LYNX SHARE: the message thread).
// Values of parameters are normalised 0..1 (the same convention as LYNX SHARE's AutomationTarget values).

#include <cstdint>
#include <string>

namespace lkmk3
{
// A continuous parameter a pot or fader can drive.
struct Parameter
{
    enum class Kind : std::uint8_t
    {
        none,
        mixerVolume, // index = mixer track (0 = master)
        mixerPan,    // index = mixer track
        mixerSend,   // index = mixer track, sub = which of that track's sends (0 = "A", 1 = "B")
        pluginParam, // index = Channel Rack channel, sub = parameter index of its instrument / plugin
    };

    Kind kind = Kind::none;
    int index = 0;
    int sub = 0;

    bool operator== (const Parameter&) const = default;
};

class DawHost
{
public:
    virtual ~DawHost() = default;

    // --- transport ---
    virtual bool isPlaying() const = 0;
    virtual bool isRecording() const = 0;   // the record switch (armed), playing or not
    virtual bool isSongMode() const = 0;    // false = pattern mode
    virtual bool isMetronomeOn() const = 0;
    virtual void play() = 0;                // start playback (honouring the host's count-in / wait-for-input)
    virtual void stop() = 0;
    virtual void setRecording (bool on) = 0;
    virtual void setSongMode (bool song) = 0;
    virtual void setMetronome (bool on) = 0;
    virtual void undo() = 0;
    virtual void redo() = 0;

    // --- mixer (track 0 = master, 1.. = inserts) ---
    virtual int mixerTrackCount() const = 0;
    virtual std::string mixerTrackName (int track) const = 0;
    virtual std::uint32_t mixerTrackColour (int track) const = 0; // 0xRRGGBB, 0 = no colour of its own
    virtual bool isTrackMuted (int track) const = 0;
    virtual void setTrackMuted (int track, bool muted) = 0;
    virtual bool isTrackSoloed (int track) const = 0;
    virtual void setTrackSoloed (int track, bool soloed) = 0;
    virtual bool isTrackArmed (int track) const = 0;
    virtual void setTrackArmed (int track, bool armed) = 0;

    // --- parameters ---
    // Current value 0..1, or < 0 when the parameter does not exist (no such send, no plugin...).
    virtual float parameterValue (const Parameter& p) const = 0;
    virtual void setParameterValue (const Parameter& p, float value) = 0;
    // Short name ("Vol Kick") and current value text ("-3.2 dB") for the LCD.
    virtual std::string parameterName (const Parameter& p) const = 0;
    virtual std::string parameterText (const Parameter& p) const = 0;

    // --- Channel Rack ---
    virtual int channelCount() const = 0;
    virtual int selectedChannel() const = 0; // -1 when none
    virtual void selectChannel (int channel) = 0;
    virtual std::string channelName (int channel) const = 0;
    virtual std::uint32_t channelColour (int channel) const = 0; // 0xRRGGBB, 0 = none
    // Plays a live note on a channel (drum pads), recorded like the keys when recording.
    virtual void playNote (int channel, int key, float velocity, bool on) = 0;
    // How many parameters the channel's instrument / plugin exposes (0 = none).
    virtual int pluginParameterCount (int channel) const = 0;

    // --- step sequencer (the current pattern; steps are 16th notes) ---
    virtual int stepCount() const = 0; // steps in the current pattern (0 without a pattern)
    virtual bool hasStep (int channel, int step) const = 0;
    virtual void toggleStep (int channel, int step) = 0;
    virtual int playingStep() const = 0; // the step under the playhead while the current pattern plays, else -1

    // --- patterns ---
    virtual int patternCount() const = 0;
    virtual int currentPattern() const = 0;
    virtual void selectPattern (int pattern) = 0;
    virtual std::uint32_t patternColour (int pattern) const = 0; // 0xRRGGBB, 0 = none
};
} // namespace lkmk3
