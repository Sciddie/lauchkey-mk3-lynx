#pragma once

// lkmk3::DawHost on top of LYNX SHARE's AudioEngine (src/engine/AudioEngine.h). Message thread only, like every Song
// edit in LYNX SHARE.
//
// Parameters map onto LYNX's own AutomationTarget (src/core/Song.h): mixerVolume / mixerPan / mixerSend /
// channelParam, written with Song::setTargetValue inside AudioEngine::editSong (..., undoable = false) and reported with
// AudioEngine::noteTweaked — the same path LYNX's MIDI-linked controllers take (AudioEngine::applyControllers).

#include <optional>

#include "lkmk3/daw_host.hpp"

namespace lynx
{
class AudioEngine;
struct AutomationTarget;
}

namespace lkmk3::lynxshare
{
class LynxDawHost final : public DawHost
{
public:
    explicit LynxDawHost (lynx::AudioEngine& engine) : engine (engine) {}

    bool isPlaying() const override;
    bool isRecording() const override;
    bool isSongMode() const override;
    bool isMetronomeOn() const override;
    void play() override;
    void stop() override;
    void setRecording (bool on) override;
    void setSongMode (bool song) override;
    void setMetronome (bool on) override;
    void undo() override;
    void redo() override;

    int mixerTrackCount() const override;
    std::string mixerTrackName (int track) const override;
    std::uint32_t mixerTrackColour (int track) const override;
    bool isTrackMuted (int track) const override;
    void setTrackMuted (int track, bool muted) override;
    bool isTrackSoloed (int track) const override;
    void setTrackSoloed (int track, bool soloed) override;
    bool isTrackArmed (int track) const override;
    void setTrackArmed (int track, bool armed) override;

    float parameterValue (const Parameter& p) const override;
    void setParameterValue (const Parameter& p, float value) override;
    std::string parameterName (const Parameter& p) const override;
    std::string parameterText (const Parameter& p) const override;

    int channelCount() const override;
    int selectedChannel() const override;
    void selectChannel (int channel) override;
    std::string channelName (int channel) const override;
    std::uint32_t channelColour (int channel) const override;
    void playNote (int channel, int key, float velocity, bool on) override;
    int pluginParameterCount (int channel) const override;

    int stepCount() const override;
    bool hasStep (int channel, int step) const override;
    void toggleStep (int channel, int step) override;
    int playingStep() const override;

    int patternCount() const override;
    int currentPattern() const override;
    void selectPattern (int pattern) override;
    std::uint32_t patternColour (int pattern) const override;

    // The LYNX automation target a Parameter stands for (nullopt: none, e.g. a send the track does not have).
    std::optional<lynx::AutomationTarget> targetOf (const Parameter& p) const;

private:
    lynx::AudioEngine& engine;
};
} // namespace lkmk3::lynxshare
