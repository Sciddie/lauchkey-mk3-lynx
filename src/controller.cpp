#include "lkmk3/controller.hpp"

#include <algorithm>
#include <cmath>

#include "lkmk3/palette.hpp"

namespace lkmk3
{
namespace
{
// Display keys for the text cache: the two default rows, then pot / fader names and values.
constexpr int rowKey (int row) { return row; }
constexpr int potNameKey (int i) { return 100 + i; }
constexpr int potValueKey (int i) { return 200 + i; }
constexpr int faderNameKey (int i) { return 300 + i; }
constexpr int faderValueKey (int i) { return 400 + i; }

constexpr float takeoverWindow = 1.5f / 127.0f;

Led ccLed (std::uint8_t cc) { return { Led::Kind::cc, cc, proto::ch1 }; }

std::string fit (std::string s, std::size_t width)
{
    if (s.size() > width)
        s.resize (width);
    return s;
}
} // namespace

Controller::Controller (DawHost& h) : Controller (h, Options {}) {}
Controller::Controller (DawHost& h, Options o) : host (h), options (o) {}

Messages Controller::connect()
{
    connected = true;
    shift = false;
    pads = PadMode::session; // what the surface selects on entering DAW mode (manual: "Pads: Session")
    pots = PotMode::pan;     // ("Pots: Pan")
    faders = FaderMode::volume; // ("Faders: Volume")
    layer = PadLayer::steps;
    stepPageIndex = patternPageIndex = 0;
    sentLeds.clear();
    sentTexts.clear();
    for (auto& s : potSlots) s = {};
    for (auto& s : faderSlots) s = {};

    Messages out;
    out.push_back (encode::dawMode (true));
    out.push_back (encode::padMode (pads));
    out.push_back (encode::potMode (pots));
    out.push_back (encode::faderMode (faders));
    out.push_back (encode::deviceInquiry());
    auto feedback = refresh();
    out.insert (out.end(), feedback.begin(), feedback.end());
    return out;
}

Messages Controller::disconnect()
{
    Messages out;
    if (! connected)
        return out;
    connected = false;
    if (options.useDisplay)
        out.push_back (encode::clearDefaultDisplay());
    out.push_back (encode::dawMode (false)); // back to Standalone (MIDI) mode, which restores its own LEDs
    sentLeds.clear();
    sentTexts.clear();
    return out;
}

namespace
{
// Device mode: parameter `param` of the selected channel's instrument / plugin.
Parameter deviceParameter (const DawHost& host, int param)
{
    const auto channel = host.selectedChannel();
    if (channel < 0 || param >= host.pluginParameterCount (channel))
        return {};
    return { Parameter::Kind::pluginParam, channel, param };
}
} // namespace

Parameter Controller::potTarget (int i) const
{
    if (i < 0 || i >= potCount)
        return {};
    if (pots == PotMode::device)
        return deviceParameter (host, i);
    const auto track = bankStart + i;
    if (track >= host.mixerTrackCount())
        return {};
    switch (pots)
    {
        case PotMode::volume: return { Parameter::Kind::mixerVolume, track, 0 };
        case PotMode::pan: return { Parameter::Kind::mixerPan, track, 0 };
        case PotMode::sendA: return { Parameter::Kind::mixerSend, track, 0 };
        case PotMode::sendB: return { Parameter::Kind::mixerSend, track, 1 };
        default: return {};
    }
}

Parameter Controller::faderTarget (int i) const
{
    if (i == faderCount - 1)
        return host.mixerTrackCount() > 0 ? Parameter { Parameter::Kind::mixerVolume, 0, 0 } : Parameter {};
    if (i < 0 || i >= faderCount)
        return {};
    if (faders == FaderMode::device)
        return deviceParameter (host, potCount + i); // the 8 parameters after the pots' 8
    const auto track = bankStart + i;
    if (track >= host.mixerTrackCount())
        return {};
    switch (faders)
    {
        case FaderMode::volume: return { Parameter::Kind::mixerVolume, track, 0 };
        case FaderMode::sendA: return { Parameter::Kind::mixerSend, track, 0 };
        case FaderMode::sendB: return { Parameter::Kind::mixerSend, track, 1 };
        default: return {};
    }
}

void Controller::showTrack (int track)
{
    if (track < 1)
        return;
    if (track < bankStart || track >= bankStart + bankSize)
        bankStart = 1 + ((track - 1) / bankSize) * bankSize;
}

void Controller::moveBank (int delta)
{
    const auto count = host.mixerTrackCount();
    const auto last = std::max (1, count - 1); // the last insert
    const auto maxStart = 1 + ((last - 1) / bankSize) * bankSize;
    bankStart = std::clamp (bankStart + delta * bankSize, 1, maxStart);
}

void Controller::selectChannelBy (int delta)
{
    const auto count = host.channelCount();
    if (count <= 0)
        return;
    const auto current = host.selectedChannel();
    const auto next = std::clamp (current < 0 ? 0 : current + delta, 0, count - 1);
    if (next != current)
        host.selectChannel (next);
}

void Controller::onContinuous (Takeover& slot, const Parameter& target, int value)
{
    if (target.kind == Parameter::Kind::none)
        return;
    if (! (slot.target == target))
        slot = { target, false, -1, -1.0f };

    const auto current = host.parameterValue (target);
    if (current < 0.0f)
        return;
    const auto wanted = (float) value / 127.0f;

    if (options.softTakeover && ! slot.caught)
    {
        const auto near = std::abs (wanted - current) <= takeoverWindow;
        const auto crossed = slot.lastHardware >= 0 && ((float) slot.lastHardware / 127.0f - current) * (wanted - current) <= 0.0f;
        slot.caught = near || crossed;
    }
    slot.lastHardware = value;
    if (options.softTakeover && ! slot.caught)
        return;

    slot.caught = true;
    slot.lastSent = wanted;
    host.setParameterValue (target, wanted);
}

void Controller::onButton (const Event& e, Messages&)
{
    if (e.button == Button::shift)
    {
        shift = e.pressed;
        return;
    }
    if (! e.pressed)
        return;

    switch (e.button)
    {
        case Button::play:
            if (host.isPlaying())
                host.stop();
            else
                host.play();
            break;
        case Button::stop:
        case Button::stopSoloMute: host.stop(); break;
        case Button::record: host.setRecording (! host.isRecording()); break;
        case Button::loop: host.setSongMode (! host.isSongMode()); break;
        case Button::click: host.setMetronome (! host.isMetronomeOn()); break;
        case Button::undo:
            if (shift)
                host.redo();
            else
                host.undo();
            break;
        case Button::trackLeft:
            if (shift)
                moveBank (-1);
            else
                selectChannelBy (-1);
            break;
        case Button::trackRight:
            if (shift)
                moveBank (1);
            else
                selectChannelBy (1);
            break;
        case Button::armSelect: armMode = ! armMode; break;
        case Button::sceneLaunch:
            layer = layer == PadLayer::steps ? PadLayer::patterns : PadLayer::steps;
            break;
        case Button::sceneUp: movePage (-1); break;
        case Button::sceneDown: movePage (1); break;
        case Button::faderButton:
        {
            const auto track = bankStart + e.index;
            if (e.index < 0 || e.index >= bankSize || track >= host.mixerTrackCount())
                break;
            if (armMode)
                host.setTrackArmed (track, ! host.isTrackArmed (track));
            else if (shift)
                host.setTrackSoloed (track, ! host.isTrackSoloed (track));
            else
                host.setTrackMuted (track, ! host.isTrackMuted (track));
            break;
        }
        default: break; // capture MIDI, quantise, device lock: not mapped (yet); device select: see the pad mode
    }
}

Messages Controller::handle (const Bytes& message)
{
    Messages out;
    const auto e = decode (message);
    if (! connected && e.type != Event::Type::deviceInquiryReply)
        return out;

    switch (e.type)
    {
        case Event::Type::button: onButton (e, out); break;
        case Event::Type::pot: onContinuous (potSlots[(std::size_t) e.index], potTarget (e.index), e.value); break;
        case Event::Type::fader: onContinuous (faderSlots[(std::size_t) e.index], faderTarget (e.index), e.value); break;
        case Event::Type::pad:
        case Event::Type::drumPad:
        case Event::Type::deviceSelectPad: onPad (e); break;
        case Event::Type::padModeReport:
            if (pads != e.padMode)
                forgetPadLeds(); // the new layout gets all its colours
            pads = e.padMode;
            break;
        case Event::Type::potModeReport: pots = e.potMode; break;
        case Event::Type::faderModeReport: faders = e.faderMode; break;
        case Event::Type::deviceInquiryReply: model = e.value; break;
        default: break;
    }
    return out;
}

int Controller::pageCount() const
{
    const auto items = layer == PadLayer::steps ? host.stepCount() : host.patternCount();
    return std::max (1, (items + padCount - 1) / padCount);
}

int Controller::channelPage() const { return std::max (0, host.selectedChannel()) / padCount; }

void Controller::movePage (int delta)
{
    auto& page = layer == PadLayer::steps ? stepPageIndex : patternPageIndex;
    page = std::clamp (page + delta, 0, pageCount() - 1);
}

void Controller::forgetPadLeds()
{
    for (auto it = sentLeds.begin(); it != sentLeds.end();)
        it = it->first.kind == Led::Kind::cc ? std::next (it) : sentLeds.erase (it);
}

void Controller::onPad (const Event& e)
{
    if (e.type == Event::Type::drumPad)
    {
        // Drum layout: pad i plays Channel Rack channel i of the selected channel's page of 16, like FL's pads
        const auto channel = channelPage() * padCount + e.index;
        if (channel < host.channelCount())
            host.playNote (channel, drumKey, e.pressed ? (float) e.value / 127.0f : 0.0f, e.pressed);
        return;
    }
    if (! e.pressed)
        return;
    if (e.type == Event::Type::deviceSelectPad)
    {
        // Device Select held: pads choose a channel of the selected channel's page of 16
        const auto channel = channelPage() * padCount + e.index;
        if (channel < host.channelCount())
            host.selectChannel (channel);
        return;
    }
    if (layer == PadLayer::steps)
    {
        stepPageIndex = std::clamp (stepPageIndex, 0, pageCount() - 1);
        const auto channel = host.selectedChannel();
        const auto step = stepPageIndex * padCount + e.index;
        if (channel >= 0 && step < host.stepCount())
            host.toggleStep (channel, step);
    }
    else
    {
        const auto pattern = patternPageIndex * padCount + e.index;
        if (pattern < host.patternCount())
            host.selectPattern (pattern);
    }
}

std::uint8_t Controller::channelColourIndex (int channel, std::uint8_t fallback) const
{
    const auto rgb = host.channelColour (channel);
    const auto index = rgb == 0 ? fallback : palette::nearest (rgb);
    return index == palette::off ? fallback : index;
}

std::uint8_t Controller::patternColourIndex (int pattern) const
{
    // LYNX / FL patterns without a colour of their own: a fixed cycle of distinct hues by index
    static constexpr std::uint8_t cycle[] = { palette::red, palette::amber, palette::yellow, palette::green,
                                              palette::cyan, palette::blue, palette::purple, 57 };
    const auto rgb = host.patternColour (pattern);
    const auto index = rgb == 0 ? cycle[pattern % 8] : palette::nearest (rgb);
    return index == palette::off ? palette::dimWhite : index;
}

void Controller::addPadLeds (std::map<Led, LedState>& leds) const
{
    if (pads == PadMode::session)
    {
        if (layer == PadLayer::steps)
        {
            const auto channel = host.selectedChannel();
            const auto count = host.stepCount();
            const auto page = std::clamp (stepPageIndex, 0, pageCount() - 1);
            const auto onColour = channel >= 0 ? channelColourIndex (channel, palette::amber) : palette::amber;
            const auto playing = host.playingStep();
            for (int i = 0; i < padCount; ++i)
            {
                const auto step = page * padCount + i;
                LedState s;
                if (channel >= 0 && step < count)
                {
                    if (step == playing)
                        s.colour = palette::white;
                    else if (host.hasStep (channel, step))
                        s.colour = onColour;
                    else
                        s.colour = step % 4 == 0 ? palette::dimWhite : palette::off; // beat grid
                }
                leds[{ Led::Kind::sessionPad, sessionPadNote (i) }] = s;
            }
        }
        else
        {
            const auto count = host.patternCount();
            const auto current = host.currentPattern();
            for (int i = 0; i < padCount; ++i)
            {
                const auto pattern = patternPageIndex * padCount + i;
                LedState s;
                if (pattern < count)
                {
                    s.colour = patternColourIndex (pattern);
                    if (pattern == current)
                        s.lighting = Lighting::pulsing;
                    else
                        s.colour = palette::dimmed (s.colour);
                }
                leds[{ Led::Kind::sessionPad, sessionPadNote (i) }] = s;
            }
        }
    }
    else if (pads == PadMode::deviceSelect || pads == PadMode::drum)
    {
        const auto first = channelPage() * padCount;
        const auto selected = host.selectedChannel();
        for (int i = 0; i < padCount; ++i)
        {
            const auto channel = first + i;
            LedState s;
            if (channel < host.channelCount())
                s.colour = channel == selected ? palette::white : channelColourIndex (channel, palette::dimGreen);
            const auto kind = pads == PadMode::drum ? Led::Kind::drumPad : Led::Kind::deviceSelectPad;
            const auto note = pads == PadMode::drum ? proto::drumNotes[(std::size_t) i] : deviceSelectPadNote (i);
            leds[{ kind, note }] = s;
        }
    }
}

std::uint8_t Controller::trackColour (int track) const
{
    const auto rgb = host.mixerTrackColour (track);
    return rgb == 0 ? palette::green : palette::nearest (rgb);
}

std::uint8_t Controller::transportColour (Lighting& lighting) const
{
    lighting = Lighting::stationary;
    if (host.isRecording())
    {
        if (host.isPlaying())
            lighting = Lighting::pulsing;
        return palette::red;
    }
    return host.isPlaying() ? palette::green : palette::dimWhite;
}

std::map<Led, Controller::LedState> Controller::desiredLeds() const
{
    std::map<Led, LedState> leds;

    // Play / Stop / Record / Loop have no LEDs on the MK3 (manual, "Colouring the surface"), so the transport state
    // is shown on the Stop/Solo/Mute button next to the pads: dim = stopped, green = playing, red = record armed
    // (pulsing while recording).
    LedState transport;
    transport.colour = transportColour (transport.lighting);
    leds[ccLed (proto::stopSoloMuteCc)] = transport;

    // Fader buttons: Select mode (default) = mute (lit in the track colour when audible), Arm mode = record arm.
    for (int i = 0; i < bankSize; ++i)
    {
        const auto track = bankStart + i;
        LedState s;
        if (track < host.mixerTrackCount())
        {
            if (armMode)
                s.colour = host.isTrackArmed (track) ? palette::red : palette::dimRed;
            else if (host.isTrackSoloed (track))
            {
                s.colour = palette::yellow;
                s.lighting = Lighting::pulsing;
            }
            else
                s.colour = host.isTrackMuted (track) ? palette::off : trackColour (track);
        }
        leds[ccLed ((std::uint8_t) (proto::faderButtonFirstCc + i))] = s;
    }
    // Arm/Select has a white LED: bright in Arm mode, dim in Select mode.
    leds[ccLed (proto::armSelectCc)] = { armMode ? palette::white : palette::dimWhite, Lighting::stationary };

    // Scene Launch ► shows the pad layer (amber = steps, cyan = patterns); Scene ▲ ▼ light when there is a page
    const auto session = pads == PadMode::session;
    leds[ccLed (proto::sceneLaunchCc)] = { session ? (layer == PadLayer::steps ? palette::amber : palette::cyan) : palette::off,
                                            Lighting::stationary };
    const auto page = layer == PadLayer::steps ? stepPageIndex : patternPageIndex;
    leds[ccLed (proto::sceneUpCc)] = { session && page > 0 ? palette::white : palette::off, Lighting::stationary };
    leds[ccLed (proto::sceneDownCc)] = { session && page + 1 < pageCount() ? palette::white : palette::off, Lighting::stationary };

    addPadLeds (leds);
    return leds;
}

std::map<int, std::string> Controller::desiredTexts() const
{
    std::map<int, std::string> texts;
    if (! options.useDisplay)
        return texts;

    std::string row0 = host.isRecording() ? "REC " : "";
    row0 += host.isPlaying() ? "PLAY" : "STOP";
    row0 += host.isSongMode() ? " SONG" : " PAT";
    if (host.isMetronomeOn())
        row0 += " CLK";
    texts[rowKey (0)] = fit (row0, proto::displayColumns);

    const auto channel = host.selectedChannel();
    const auto last = std::min (bankStart + bankSize - 1, host.mixerTrackCount() - 1);
    const auto bank = std::to_string (bankStart) + "-" + std::to_string (std::max (bankStart, last));
    const auto name = channel >= 0 ? host.channelName (channel) : std::string ("-");
    const auto room = proto::displayColumns - bank.size() - 1;
    auto row1 = fit (name, room);
    row1.append (room - row1.size() + 1, ' ');
    texts[rowKey (1)] = row1 + bank;

    for (int i = 0; i < potCount; ++i)
    {
        const auto p = potTarget (i);
        texts[potNameKey (i)] = p.kind == Parameter::Kind::none ? std::string ("-") : host.parameterName (p);
        texts[potValueKey (i)] = p.kind == Parameter::Kind::none ? std::string() : host.parameterText (p);
    }
    for (int i = 0; i < faderCount; ++i)
    {
        const auto p = faderTarget (i);
        texts[faderNameKey (i)] = p.kind == Parameter::Kind::none ? std::string ("-") : host.parameterName (p);
        texts[faderValueKey (i)] = p.kind == Parameter::Kind::none ? std::string() : host.parameterText (p);
    }
    return texts;
}

Messages Controller::refresh()
{
    Messages out;
    if (! connected)
        return out;

    // a parameter moved by someone else (mouse, automation, undo): the control has to pick it up again
    auto release = [this] (Takeover& slot) {
        if (slot.caught && slot.lastSent >= 0.0f)
        {
            const auto now = host.parameterValue (slot.target);
            if (now >= 0.0f && std::abs (now - slot.lastSent) > 2.0f / 127.0f)
                slot.caught = false;
        }
    };
    for (auto& s : potSlots) release (s);
    for (auto& s : faderSlots) release (s);

    for (const auto& [led, state] : desiredLeds())
    {
        const auto it = sentLeds.find (led);
        if (it != sentLeds.end() && it->second == state)
            continue;
        out.push_back (encode::ledColour (led, state.colour, state.lighting));
        sentLeds[led] = state;
    }

    for (const auto& [key, text] : desiredTexts())
    {
        const auto it = sentTexts.find (key);
        if (it != sentTexts.end() && it->second == text)
            continue;
        if (key < 100)
            out.push_back (encode::setDefaultDisplay (key, text));
        else if (key < 200)
            out.push_back (encode::setPotParameterName (key - 100, text));
        else if (key < 300)
            out.push_back (encode::setPotParameterValue (key - 200, text));
        else if (key < 400)
            out.push_back (encode::setFaderParameterName (key - 300, text));
        else
            out.push_back (encode::setFaderParameterValue (key - 400, text));
        sentTexts[key] = text;
    }
    return out;
}
} // namespace lkmk3
