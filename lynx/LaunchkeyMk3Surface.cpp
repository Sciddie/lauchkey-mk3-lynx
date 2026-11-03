#include "LaunchkeyMk3Surface.h"

#include <optional>

#include "LynxDawHost.h"
#include "engine/AudioEngine.h"
#include "lkmk3/version.hpp"

namespace lkmk3::lynxshare
{
namespace
{
constexpr int timerHz = 30;          // input handling + LED / LCD feedback
constexpr int rescanTicks = 2 * timerHz; // hot-plug check every 2 s, like AudioEngine's own MIDI check

template <typename DeviceList>
std::optional<juce::MidiDeviceInfo> findDawPort (const DeviceList& devices)
{
    for (const auto& d : devices)
        if (isDawPortName (d.name.toStdString()))
            return d;
    return std::nullopt;
}
} // namespace

LaunchkeyMk3Surface::LaunchkeyMk3Surface (lynx::AudioEngine& e)
    : engine (e), host (std::make_unique<LynxDawHost> (e)), controller (std::make_unique<Controller> (*host))
{
    rescan();
    startTimerHz (timerHz);
}

LaunchkeyMk3Surface::~LaunchkeyMk3Surface()
{
    stopTimer();
    if (isConnected())
        send (controller->disconnect()); // back to Standalone mode
    closePorts();
}

void LaunchkeyMk3Surface::rescan()
{
    const auto ins = juce::MidiInput::getAvailableDevices();
    const auto outs = juce::MidiOutput::getAvailableDevices();
    const auto in = findDawPort (ins);
    const auto out = findDawPort (outs);

    if (isConnected())
    {
        const auto stillThere = in && out && in->identifier == input->getIdentifier() && out->identifier == outputId;
        if (! stillThere) // unplugged (nothing to send any more)
        {
            controller->disconnect();
            closePorts();
            DBG ("Launchkey MK3: disconnected");
        }
        return;
    }
    if (in && out)
        connectTo (*in, *out);
}

void LaunchkeyMk3Surface::keepDawPortAwayFromNotes (const juce::String& name)
{
    auto prefs = engine.getMidiPrefs();
    auto device = prefs.input (name.toStdString());
    if (! device.enabled)
        return;
    device.enabled = false; // AudioEngine closes it (enableAllMidiInputs) and ignores it (handleIncomingMidiMessage)
    prefs.setInput (device);
    engine.setMidiPrefs (prefs, false); // not saved: the user's MIDI settings file stays as it was
}

void LaunchkeyMk3Surface::connectTo (const juce::MidiDeviceInfo& in, const juce::MidiDeviceInfo& out)
{
    keepDawPortAwayFromNotes (in.name);

    input = juce::MidiInput::openDevice (in.identifier, this);
    output = juce::MidiOutput::openDevice (out.identifier);
    if (input == nullptr || output == nullptr)
    {
        closePorts();
        return;
    }
    portName = in.name;
    outputId = out.identifier;
    {
        const std::lock_guard<std::mutex> lock (inboxLock);
        inbox.clear();
    }
    input->start();
    send (controller->connect());
    DBG ("Launchkey MK3 " << versionString << ": connected to " << in.name << " / " << out.name);
}

void LaunchkeyMk3Surface::closePorts()
{
    if (input != nullptr)
        input->stop();
    input = nullptr;
    output = nullptr;
    portName = {};
    outputId = {};
}

void LaunchkeyMk3Surface::send (const Messages& messages)
{
    if (output == nullptr)
        return;
    for (const auto& m : messages)
        if (! m.empty())
            output->sendMessageNow (juce::MidiMessage (m.data(), (int) m.size()));
}

void LaunchkeyMk3Surface::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    // MIDI thread: copy and hand over; all LYNX work happens on the message thread
    const auto* data = message.getRawData();
    Bytes bytes (data, data + message.getRawDataSize());
    const std::lock_guard<std::mutex> lock (inboxLock);
    if (inbox.size() < 4096) // (a stuck message thread must not grow this forever)
        inbox.push_back (std::move (bytes));
}

void LaunchkeyMk3Surface::timerCallback()
{
    if (++ticks >= rescanTicks)
    {
        ticks = 0;
        rescan();
    }
    if (! isConnected())
        return;

    std::vector<Bytes> pending;
    {
        const std::lock_guard<std::mutex> lock (inboxLock);
        pending.swap (inbox);
    }
    for (const auto& m : pending)
        send (controller->handle (m));
    send (controller->refresh());
}
} // namespace lkmk3::lynxshare
