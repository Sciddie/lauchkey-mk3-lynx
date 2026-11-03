#pragma once

// The Launchkey MK3 control surface inside LYNX SHARE: finds the keyboard's "LKMK3 DAW" MIDI port (hot-plug), opens it
// itself, runs the DAW-mode handshake and drives lkmk3::Controller from the message thread.
//
// The keys port ("LKMK3 MIDI") is left to LYNX's normal MIDI input handling (it plays the selected channel). The DAW
// port is switched off in LYNX's MIDI input settings (for this session) so its pad / button messages are not played
// as notes by AudioEngine::handleIncomingMidiMessage.

#include <memory>
#include <mutex>
#include <vector>

#include <juce_audio_devices/juce_audio_devices.h>

#include "lkmk3/controller.hpp"

namespace lynx
{
class AudioEngine;
}

namespace lkmk3::lynxshare
{
class LynxDawHost;

class LaunchkeyMk3Surface final : private juce::Timer, private juce::MidiInputCallback
{
public:
    explicit LaunchkeyMk3Surface (lynx::AudioEngine& engine);
    ~LaunchkeyMk3Surface() override; // leaves DAW mode on the device

    bool isConnected() const noexcept { return input != nullptr && output != nullptr; }
    juce::String getConnectedPortName() const { return portName; }

    // Call when the device list changed (the surface also polls every 2 s).
    void rescan();

private:
    void timerCallback() override;
    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

    void connectTo (const juce::MidiDeviceInfo& in, const juce::MidiDeviceInfo& out);
    void closePorts();
    void send (const Messages& messages);
    void keepDawPortAwayFromNotes (const juce::String& name);

    lynx::AudioEngine& engine;
    std::unique_ptr<LynxDawHost> host;
    std::unique_ptr<Controller> controller;
    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<juce::MidiOutput> output;
    juce::String portName, outputId;
    int ticks = 0;

    std::mutex inboxLock; // MIDI thread -> message thread
    std::vector<Bytes> inbox;
};
} // namespace lkmk3::lynxshare
