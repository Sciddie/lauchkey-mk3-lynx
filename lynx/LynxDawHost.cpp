#include "LynxDawHost.h"

#include <cmath>
#include <cstdio>

#include "engine/AudioEngine.h"
#include "engine/PluginHost.h"

namespace lkmk3::lynxshare
{
namespace
{
std::string trackLabel (const lynx::Song& song, int track)
{
    if (track < 0 || track >= (int) song.mixer.size())
        return {};
    const auto& name = song.mixer[(std::size_t) track].name;
    if (! name.empty())
        return name;
    return track == 0 ? std::string ("Master") : "Insert " + std::to_string (track);
}

std::string decibels (float gain)
{
    if (gain <= 0.00001f)
        return "-inf dB";
    char buf[24];
    std::snprintf (buf, sizeof buf, "%+.1f dB", 20.0 * std::log10 ((double) gain));
    return buf;
}

bool validTrack (const lynx::Song& song, int track) { return track >= 0 && track < (int) song.mixer.size(); }
} // namespace

bool LynxDawHost::isPlaying() const { return engine.getTransport().playing.load(); }
bool LynxDawHost::isRecording() const { return engine.isRecording(); }
bool LynxDawHost::isSongMode() const { return engine.getSong().mode == lynx::PlayMode::song; }
bool LynxDawHost::isMetronomeOn() const { return engine.getTransport().metronome.load(); }
void LynxDawHost::play() { engine.startPlayback(); } // (the toolbar's Play: count-in, wait for input)
void LynxDawHost::stop() { engine.stop(); }
void LynxDawHost::setRecording (bool on) { engine.setRecording (on); }
void LynxDawHost::setSongMode (bool song)
{
    // as MainComponent's PAT / SONG switch: outside the undo history
    engine.editSong ([song] (lynx::Song& s) { s.mode = song ? lynx::PlayMode::song : lynx::PlayMode::pattern; }, false);
}
void LynxDawHost::setMetronome (bool on) { engine.getTransport().metronome.store (on); } // (the toolbar follows it)
void LynxDawHost::undo() { engine.undo(); }
void LynxDawHost::redo() { engine.redo(); }

int LynxDawHost::mixerTrackCount() const { return (int) engine.getSong().mixer.size(); }
std::string LynxDawHost::mixerTrackName (int track) const { return trackLabel (engine.getSong(), track); }
std::uint32_t LynxDawHost::mixerTrackColour (int track) const
{
    const auto& song = engine.getSong();
    return validTrack (song, track) ? song.mixer[(std::size_t) track].colour & 0xFFFFFFu : 0u;
}

bool LynxDawHost::isTrackMuted (int track) const
{
    const auto& song = engine.getSong();
    return validTrack (song, track) && song.mixer[(std::size_t) track].muted;
}
void LynxDawHost::setTrackMuted (int track, bool muted)
{
    if (validTrack (engine.getSong(), track) && isTrackMuted (track) != muted)
        engine.editSong ([track, muted] (lynx::Song& s) { s.mixer[(std::size_t) track].muted = muted; }); // undoable, as the Mixer's switch
}
bool LynxDawHost::isTrackSoloed (int track) const
{
    const auto& song = engine.getSong();
    return validTrack (song, track) && song.mixer[(std::size_t) track].solo;
}
void LynxDawHost::setTrackSoloed (int track, bool soloed)
{
    if (validTrack (engine.getSong(), track) && isTrackSoloed (track) != soloed)
        engine.editSong ([track, soloed] (lynx::Song& s) { s.mixer[(std::size_t) track].solo = soloed; });
}
bool LynxDawHost::isTrackArmed (int track) const { return track > 0 && engine.isInsertArmed (track); }
void LynxDawHost::setTrackArmed (int track, bool armed)
{
    if (track > 0 && validTrack (engine.getSong(), track))
        engine.setInsertArmed (track, armed); // (refuses a record-safe insert itself)
}

std::optional<lynx::AutomationTarget> LynxDawHost::targetOf (const Parameter& p) const
{
    using K = lynx::AutomationTargetKind;
    const auto& song = engine.getSong();
    lynx::AutomationTarget t;
    t.index = p.index;
    switch (p.kind)
    {
        case Parameter::Kind::mixerVolume:
            if (! validTrack (song, p.index)) return std::nullopt;
            t.kind = K::mixerVolume;
            return t;
        case Parameter::Kind::mixerPan:
            if (! validTrack (song, p.index)) return std::nullopt;
            t.kind = K::mixerPan;
            return t;
        case Parameter::Kind::mixerSend:
        {
            // Send A / B = the track's first / second insert-to-insert send (MixerTrack::sends; slot = destination)
            if (! validTrack (song, p.index)) return std::nullopt;
            const auto& sends = song.mixer[(std::size_t) p.index].sends;
            if (p.sub < 0 || p.sub >= (int) sends.size()) return std::nullopt;
            t.kind = K::mixerSend;
            t.slot = sends[(std::size_t) p.sub].target;
            return t;
        }
        case Parameter::Kind::pluginParam:
            if (p.index < 0 || p.index >= (int) song.channels.size() || p.sub < 0) return std::nullopt;
            t.kind = K::channelParam;
            t.param = p.sub;
            return t;
        case Parameter::Kind::none:
        default: return std::nullopt;
    }
}

float LynxDawHost::parameterValue (const Parameter& p) const
{
    const auto t = targetOf (p);
    return t ? engine.currentTargetValue (*t) : -1.0f;
}

void LynxDawHost::setParameterValue (const Parameter& p, float value)
{
    const auto t = targetOf (p);
    if (! t)
        return;
    bool held = false;
    // one snapshot, outside the undo history — like turning the knob / a linked controller (applyControllers)
    engine.editSong ([&] (lynx::Song& s) { held = s.setTargetValue (*t, value); }, false);
    if (! held && t->kind == lynx::AutomationTargetKind::channelParam)
        if (auto* plugin = dynamic_cast<lynx::PluginInstrument*> (engine.getChannelInstrument (t->index).get()))
            plugin->setParameterFromUi (t->param, value); // a hosted plugin keeps its own parameters
    engine.noteTweaked (*t); // Last tweaked + automation recording, as a linked controller's move
}

std::string LynxDawHost::parameterName (const Parameter& p) const
{
    const auto& song = engine.getSong();
    switch (p.kind)
    {
        case Parameter::Kind::mixerVolume: return "Vol " + trackLabel (song, p.index);
        case Parameter::Kind::mixerPan: return "Pan " + trackLabel (song, p.index);
        case Parameter::Kind::mixerSend:
        {
            const auto prefix = std::string (p.sub == 0 ? "SndA " : "SndB ");
            if (const auto t = targetOf (p))
                return prefix + ">" + trackLabel (song, t->slot);
            return prefix + "none";
        }
        case Parameter::Kind::pluginParam:
        {
            if (auto* plugin = dynamic_cast<lynx::PluginInstrument*> (engine.getChannelInstrument (p.index).get()))
            {
                const auto& params = plugin->getInstance().getParameters();
                if (p.sub < params.size())
                    return params[p.sub]->getName (16).toStdString();
                return "-";
            }
            if (const auto t = targetOf (p); t && engine.nameAutomation)
                return engine.nameAutomation (*t);
            return "Param " + std::to_string (p.sub + 1);
        }
        default: return "-";
    }
}

std::string LynxDawHost::parameterText (const Parameter& p) const
{
    const auto v = parameterValue (p);
    if (v < 0.0f)
        return "n/a";
    switch (p.kind)
    {
        case Parameter::Kind::mixerVolume:
        case Parameter::Kind::mixerSend: return decibels (lynx::faderGain (v)); // FL's fader curve, 0 dB at 80 %
        case Parameter::Kind::mixerPan:
        {
            const auto pan = (int) std::lround ((v * 2.0f - 1.0f) * 100.0f);
            return pan == 0 ? std::string ("C") : (pan < 0 ? "L" + std::to_string (-pan) : "R" + std::to_string (pan));
        }
        case Parameter::Kind::pluginParam:
            if (auto* plugin = dynamic_cast<lynx::PluginInstrument*> (engine.getChannelInstrument (p.index).get()))
            {
                const auto& params = plugin->getInstance().getParameters();
                if (p.sub < params.size())
                    return params[p.sub]->getCurrentValueAsText().toStdString();
            }
            return std::to_string ((int) std::lround (v * 100.0f)) + "%";
        default: return {};
    }
}

int LynxDawHost::channelCount() const { return (int) engine.getSong().channels.size(); }
int LynxDawHost::selectedChannel() const { return engine.getSelectedChannel(); }
void LynxDawHost::selectChannel (int channel)
{
    if (channel >= 0 && channel < channelCount())
        engine.setSelectedChannel (channel);
}
std::string LynxDawHost::channelName (int channel) const
{
    const auto& song = engine.getSong();
    if (channel < 0 || channel >= (int) song.channels.size())
        return {};
    const auto& name = song.channels[(std::size_t) channel].name;
    return name.empty() ? "Channel " + std::to_string (channel + 1) : name;
}
std::uint32_t LynxDawHost::channelColour (int channel) const
{
    const auto& song = engine.getSong();
    return channel >= 0 && channel < (int) song.channels.size() ? song.channels[(std::size_t) channel].colour & 0xFFFFFFu : 0u;
}
} // namespace lkmk3::lynxshare
