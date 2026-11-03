#pragma once

// An in-memory DAW for the mapping tests: records what the controller asked for.

#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "lkmk3/daw_host.hpp"

namespace lkmk3::test
{
struct FakeTrack
{
    std::string name;
    std::uint32_t colour = 0;
    float volume = 0.8f, pan = 0.5f;
    std::vector<float> sends; // levels of its sends, 0..1
    bool muted = false, soloed = false, armed = false;
};

struct FakeChannel
{
    std::string name;
    std::uint32_t colour = 0;
};

class FakeHost : public DawHost
{
public:
    FakeHost()
    {
        tracks.push_back ({ "Master" });
        for (int i = 1; i <= 20; ++i)
            tracks.push_back ({ "Insert " + std::to_string (i) });
        tracks[1].sends = { 0.5f };
        for (int i = 0; i < 4; ++i)
            channels.push_back ({ "Chan " + std::to_string (i) });
    }

    bool playing = false, recording = false, songMode = false, metronome = false;
    int undos = 0, redos = 0, playCalls = 0;
    std::vector<FakeTrack> tracks;
    std::vector<FakeChannel> channels;
    int selected = 0;
    int parameterWrites = 0;

    bool isPlaying() const override { return playing; }
    bool isRecording() const override { return recording; }
    bool isSongMode() const override { return songMode; }
    bool isMetronomeOn() const override { return metronome; }
    void play() override { playing = true; ++playCalls; }
    void stop() override { playing = false; }
    void setRecording (bool on) override { recording = on; }
    void setSongMode (bool on) override { songMode = on; }
    void setMetronome (bool on) override { metronome = on; }
    void undo() override { ++undos; }
    void redo() override { ++redos; }

    int mixerTrackCount() const override { return (int) tracks.size(); }
    std::string mixerTrackName (int t) const override { return valid (t) ? tracks[(std::size_t) t].name : std::string(); }
    std::uint32_t mixerTrackColour (int t) const override { return valid (t) ? tracks[(std::size_t) t].colour : 0; }
    bool isTrackMuted (int t) const override { return valid (t) && tracks[(std::size_t) t].muted; }
    void setTrackMuted (int t, bool m) override { if (valid (t)) tracks[(std::size_t) t].muted = m; }
    bool isTrackSoloed (int t) const override { return valid (t) && tracks[(std::size_t) t].soloed; }
    void setTrackSoloed (int t, bool s) override { if (valid (t)) tracks[(std::size_t) t].soloed = s; }
    bool isTrackArmed (int t) const override { return valid (t) && tracks[(std::size_t) t].armed; }
    void setTrackArmed (int t, bool a) override { if (valid (t)) tracks[(std::size_t) t].armed = a; }

    float* slot (const Parameter& p)
    {
        if (! valid (p.index))
            return nullptr;
        auto& t = tracks[(std::size_t) p.index];
        switch (p.kind)
        {
            case Parameter::Kind::mixerVolume: return &t.volume;
            case Parameter::Kind::mixerPan: return &t.pan;
            case Parameter::Kind::mixerSend: return p.sub >= 0 && p.sub < (int) t.sends.size() ? &t.sends[(std::size_t) p.sub] : nullptr;
            default: return nullptr;
        }
    }
    float parameterValue (const Parameter& p) const override
    {
        if (p.kind == Parameter::Kind::pluginParam)
        {
            if (p.sub < 0 || p.sub >= pluginParameterCount (p.index))
                return -1.0f;
            const auto it = pluginParams.find ({ p.index, p.sub });
            return it == pluginParams.end() ? 0.5f : it->second;
        }
        const auto* v = const_cast<FakeHost*> (this)->slot (p);
        return v != nullptr ? *v : -1.0f;
    }
    void setParameterValue (const Parameter& p, float value) override
    {
        if (p.kind == Parameter::Kind::pluginParam)
        {
            if (p.sub >= 0 && p.sub < pluginParameterCount (p.index))
            {
                pluginParams[{ p.index, p.sub }] = value;
                ++parameterWrites;
            }
            return;
        }
        if (auto* v = slot (p))
        {
            *v = value;
            ++parameterWrites;
        }
    }
    std::string parameterName (const Parameter& p) const override
    {
        const char* kinds[] = { "-", "Vol", "Pan", "Snd", "Prm" };
        return std::string (kinds[(int) p.kind]) + " " + std::to_string (p.index);
    }
    std::string parameterText (const Parameter& p) const override
    {
        const auto v = parameterValue (p);
        return v < 0 ? std::string ("n/a") : std::to_string ((int) std::lround (v * 100.0f)) + "%";
    }

    int channelCount() const override { return (int) channels.size(); }
    int selectedChannel() const override { return selected; }
    void selectChannel (int c) override { selected = c; }
    std::string channelName (int c) const override { return c >= 0 && c < channelCount() ? channels[(std::size_t) c].name : std::string(); }
    std::uint32_t channelColour (int c) const override { return c >= 0 && c < channelCount() ? channels[(std::size_t) c].colour : 0; }

    // --- stage 2 ---
    struct Note { int channel, key; float velocity; bool on; };
    std::vector<Note> notes;
    std::vector<std::vector<bool>> steps = std::vector<std::vector<bool>> (4, std::vector<bool> (16));
    int playhead = -1;
    std::vector<std::uint32_t> patternColours { 0xFF0000, 0x00FF00, 0 };
    int pattern = 0;
    std::vector<int> paramCounts { 0, 12, 0, 0 };
    std::map<std::pair<int, int>, float> pluginParams;

    void playNote (int c, int k, float v, bool on) override { notes.push_back ({ c, k, v, on }); }
    int pluginParameterCount (int c) const override { return c >= 0 && c < (int) paramCounts.size() ? paramCounts[(std::size_t) c] : 0; }
    int stepCount() const override { return steps.empty() ? 0 : (int) steps[0].size(); }
    bool hasStep (int c, int s) const override
    {
        return c >= 0 && c < (int) steps.size() && s >= 0 && s < stepCount() && steps[(std::size_t) c][(std::size_t) s];
    }
    void toggleStep (int c, int s) override
    {
        if (c >= 0 && c < (int) steps.size() && s >= 0 && s < stepCount())
            steps[(std::size_t) c][(std::size_t) s] = ! steps[(std::size_t) c][(std::size_t) s];
    }
    int playingStep() const override { return playhead; }
    int patternCount() const override { return (int) patternColours.size(); }
    int currentPattern() const override { return pattern; }
    void selectPattern (int p) override { pattern = p; }
    std::uint32_t patternColour (int p) const override { return p >= 0 && p < patternCount() ? patternColours[(std::size_t) p] : 0; }

private:
    bool valid (int t) const { return t >= 0 && t < (int) tracks.size(); }
};
} // namespace lkmk3::test
