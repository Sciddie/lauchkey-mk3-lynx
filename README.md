# lauchkey-mk3-lynx

Novation **Launchkey MK3** (25 / 37 / 49 / 61) DAW integration for
[LYNX SHARE](https://github.com/Ka1-S/LYNX-SHARE), the open-source FL-Studio-style DAW (C++20 / JUCE 8, AGPL-3.0).

> **Status: alpha, not yet tested on real hardware.** Everything below is implemented against Novation's
> *Launchkey MK3 Programmer's reference manual* and LYNX SHARE's source, and is covered by unit tests that simulate the
> keyboard's DAW port — but no Launchkey has been plugged into it yet. See [What is untested](#what-is-untested).

Developed for a Launchkey MK3 49 on Fedora with PipeWire, but nothing in it is Fedora-specific.

## What it does

When LYNX SHARE starts (or the keyboard is plugged in later) the integration finds the keyboard's **`LKMK3 DAW`** MIDI
port, puts the keyboard into **DAW mode** and maps its surface. On exit it releases the LCD and returns the keyboard to
Standalone mode. The keys port (**`LKMK3 MIDI`**) keeps working as a normal MIDI input in LYNX SHARE (it plays the
selected Channel Rack channel).

| Control | Function in LYNX SHARE |
|---|---|
| **Play** | play / stop (Play honours LYNX's count-in and wait-for-input, like the toolbar) |
| **Stop** | stop |
| **Record** | record switch on / off |
| **Loop** | toggle PAT / SONG mode |
| **Click** | metronome on / off |
| **Undo** / **Shift + Undo** | undo / redo |
| **Track ◀ ▶** | select previous / next Channel Rack channel (keys and step pads follow it) |
| **Shift + Track ◀ ▶** | move the 8-strip mixer bank (inserts 1–8, 9–16, …) |
| **Faders 1–8** | mixer insert volume of the bank (*Volume* fader mode) or its send A / B (*Send-A* / *Send-B*) |
| **Fader 9** | master volume (always) |
| **Pots 1–8** | the pot mode chosen on the keyboard: *Volume*, *Pan* (default), *Send-A*, *Send-B* of the bank |
| **Fader buttons** | mute (lit in the track colour while audible); **Shift +** button: solo (pulsing yellow) |
| **Arm/Select** | switches the fader buttons to **record arm** of the insert (red) and back |
| **Stop/Solo/Mute LED** | transport state: dim = stopped, green = playing, red = record on, pulsing red = recording |
| **LCD** | top row `REC PLAY PAT CLK`; bottom row selected channel + mixer bank; name and value (dB, pan) of every pot / fader while you touch it |

Send A / B are the first / second insert-to-insert send of the track (LYNX's `MixerTrack::sends`); a track without that
send shows `SndA none` and ignores the control. Pots and faders use **soft takeover**: a control only takes over once it
reaches (or crosses) the parameter's current value, and lets go again when the value is changed elsewhere (mouse,
automation, undo).

The Launchkey MK3's Play / Stop / Record / Loop buttons have **no LEDs** (manual, *Colouring the surface*), which is why
transport feedback goes to the Stop/Solo/Mute button and the LCD.

## Architecture

```
 Launchkey MK3 ──"LKMK3 DAW" port──▶ lynx/LaunchkeyMk3Surface ──▶ lkmk3::Controller ──▶ lkmk3::DawHost
       ▲                          (JUCE MidiInput/Output,         (mapping layer,        ▲
       │                           hot-plug, 30 Hz timer)          src/controller.cpp)   │ implemented by
       └──────── LEDs / LCD ◀──────────── lkmk3 protocol layer ◀──────────┘     lynx/LynxDawHost (AudioEngine)
                                       (src/protocol.cpp, palette.cpp)
```

* **Protocol layer** (`include/lkmk3/protocol.hpp`, `src/protocol.cpp`, `src/palette.cpp`) — pure C++20, no JUCE, no
  LYNX: decodes the DAW-port MIDI into typed events and encodes DAW mode, mode selection, LED colours and LCD SysEx.
* **Mapping layer** (`include/lkmk3/controller.hpp`, `src/controller.cpp`) — pure C++20: turns events into calls on the
  abstract `lkmk3::DawHost` (`include/lkmk3/daw_host.hpp`) and computes *diffed* LED / LCD feedback. No I/O, no timer.
* **LYNX SHARE adapter** (`lynx/`) — the only code that includes LYNX SHARE and JUCE headers. It is compiled *into*
  LYNX SHARE through a small opt-in patch (`lynx/patches/`).

`lkmk3_core` (protocol + mapping) builds and is tested on its own; the adapter is built inside LYNX SHARE.

### Why a native module, not a bridge daemon

I looked for every extension point in LYNX SHARE's source (commit `caac540`, version 0.1.22) before choosing:

| Option | What LYNX SHARE actually has | Verdict |
|---|---|---|
| Control-surface / controller scripting API | None. The only scripting is FL's *VFX Script* on an embedded pocketpy interpreter (`src/core/VfxScript.cpp`, `CMakeLists.txt` "pocketpy"), which runs note-processing scripts inside a channel, with no access to transport, mixer or MIDI ports. `src/core/ControlSurface.h` is FL's *Control Surface* **plugin** (an on-screen knob panel), not hardware support. | not available |
| OSC / IPC | None: no `juce_osc`, no sockets, no named pipes or `InterprocessConnection` in `src/` (the only `ChildProcess` uses are the plugin sandbox, crash reporter and file-type helper). | not available |
| MIDI learn / generic controller mapping | Yes: `AudioEngine::learnMidiController` / `learnGlobalLink` (`src/engine/AudioEngine.h`), `Song::midiLinks`, applied in `AudioEngine::applyControllers` (`src/engine/AudioEngine.cpp`). But it is **CC → parameter only**: learned one control at a time, no feedback to the device, nothing for transport, channel selection, banks, step sequencing or the LCD. | partial at best |
| MIDI input in general | `AudioEngine::handleIncomingMidiMessage` (`AudioEngine.cpp`) opens **every** MIDI input and plays its notes on the selected channels; the only non-note handling is MIDI clock / start / stop for *External clock sync* (`MidiSyncIo::handleInput`) and the song-marker jump channel. | the DAW port would even be *played as notes* |
| Internal C++ hooks | Rich public `AudioEngine` API used by LYNX's own UI on the message thread: `startPlayback` / `stop` / `setRecording`, `getTransport()`, `editSong`, `Song::setTargetValue` / `AudioEngine::currentTargetValue` with `AutomationTarget` (`mixerVolume`, `mixerPan`, `mixerSend`, `channelParam`), `noteTweaked`, `setInsertArmed`, `setSelectedChannel`, `getMidiPrefs` / `setMidiPrefs`, `undo` / `redo`. | **everything needed** |

A standalone bridge daemon (option b) could only reach LYNX SHARE through a virtual MIDI port + MIDI learn: no DAW-mode
LED / LCD feedback of LYNX's state, no transport (short of abusing external clock sync, which also slaves the tempo),
no channel select, no step sequencer, and manual learning of every control. So the integration is **(a) a native
control-surface module** written against those public `AudioEngine` / `Song` interfaces, delivered as sources +
a 21-line opt-in patch to LYNX SHARE (`CMakeLists.txt` and `src/Main.cpp`), with the DAW-agnostic protocol and mapping
layers kept separate and testable.

How the adapter fits LYNX SHARE's rules (see `docs/ARCHITECTURE.md` there: the Song is touched only on the UI thread):

* the MIDI callback only queues bytes; all work happens in a `juce::Timer` on the message thread;
* parameter moves go through `editSong (…, undoable = false)` + `Song::setTargetValue` + `noteTweaked`, exactly like
  LYNX's own linked MIDI controllers (`applyControllers`), so automation recording and *Last tweaked* work;
  hosted plugin parameters through `PluginInstrument::setParameterFromUi`;
* mute / solo are undoable `editSong` edits like the Mixer's switches; PAT/SONG and the metronome are changed the way
  the toolbar changes them;
* the DAW port is switched **off** in LYNX's MIDI input settings for the session (`setMidiPrefs (…, save = false)`), so
  `handleIncomingMidiMessage` doesn't play the pads and buttons as notes; the surface opens the port itself.

## Repository layout

```
include/lkmk3/      public headers: protocol, palette, daw_host (interface), controller (mapping), version
src/                protocol.cpp, palette.cpp, controller.cpp, version.cpp
lynx/               LYNX SHARE adapter (LynxDawHost, LaunchkeyMk3Surface) + patches/ for LYNX SHARE
cmake/              Sources.cmake (core source list), LaunchkeyMk3Lynx.cmake (lkmk3_add_to_lynx)
tests/              unit tests: fake DAW (fake_host.hpp) + simulated Launchkey (sim_surface.hpp)
scripts/            check-lynx-adapter.sh: patch LYNX SHARE and compile the adapter against it
```

## Building and testing (no hardware needed)

Fedora:

```sh
sudo dnf install gcc-c++ cmake ninja-build git
git clone https://github.com/Sciddie/lauchkey-mk3-lynx.git
cd lauchkey-mk3-lynx
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure      # or ./build/tests/lkmk3_tests [name-filter]
```

Options: `-DLKMK3_WARNINGS_AS_ERRORS=ON`, `-DLKMK3_SANITIZE=ON` (ASan + UBSan).

The tests drive the mapping with a simulated Launchkey MK3 49 (`tests/sim_surface.hpp`): it sends what the keyboard's
controls send and interprets what comes back (DAW mode flag, LED colour and lighting per address, LCD rows, parameter
names / values), against an in-memory DAW (`tests/fake_host.hpp`).

To check the adapter against LYNX SHARE's real headers without building the whole DAW:

```sh
scripts/check-lynx-adapter.sh          # fetches LYNX SHARE (pinned commit) + JUCE 8.0.4, applies the patch, compiles
```

## Building LYNX SHARE with the integration

Fedora dependencies for LYNX SHARE (the Fedora names of the packages in LYNX SHARE's README):

```sh
sudo dnf install gcc-c++ cmake ninja-build git pkgconf-pkg-config alsa-lib-devel pipewire-jack-audio-connection-kit-devel \
  libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXext-devel libXcomposite-devel \
  freetype-devel fontconfig-devel mesa-libGL-devel
```

```sh
git clone https://github.com/Ka1-S/LYNX-SHARE.git
cd LYNX-SHARE
git apply ../lauchkey-mk3-lynx/lynx/patches/0001-Add-optional-Launchkey-MK3-control-surface.patch
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLYNX_LAUNCHKEY_MK3_DIR="$PWD/../lauchkey-mk3-lynx"
cmake --build build --target LYNX
./build/LYNX_artefacts/Release/LYNX
```

Without `-DLYNX_LAUNCHKEY_MK3_DIR` the patched LYNX SHARE builds exactly as before. The patch targets LYNX SHARE
`caac540` (0.1.22); `scripts/check-lynx-adapter.sh` pins that commit (override with `LYNX_REF=<sha>`).

## Testing on the hardware (Fedora / PipeWire)

1. **Ports.** Plug in the Launchkey and run `aconnect -l` (package `alsa-utils`) or `amidi -l`. You should see the two
   interfaces, e.g. `Launchkey MK3 49 LKMK3 MIDI` and `Launchkey MK3 49 LKMK3 DAW`. PipeWire exposes them through its
   ALSA sequencer bridge, which is what JUCE (and so LYNX SHARE) uses on Linux; no JACK setup is needed for MIDI.
2. **Handshake by hand** (optional, without LYNX): with `amidi -l` showing e.g. `hw:1,0,1  … LKMK3 DAW`:
   ```sh
   amidi -p hw:1,0,1 -S '9F 0C 7F'     # DAW mode: the pads change to the Session layout
   amidi -p hw:1,0,1 -S '90 70 05'     # lower-left pad red (the manual's example)
   amidi -p hw:1,0,1 -d                # watch what the controls send (Ctrl+C to stop)
   amidi -p hw:1,0,1 -S '9F 0C 00'     # back to Standalone mode
   ```
3. **In LYNX SHARE** (built as above): start it with the keyboard connected, or plug it in while running (it is picked
   up within ~2 s). Check:
   - the Stop/Solo/Mute button lights dim white and the LCD shows `STOP PAT`;
   - Play / Stop / Record / Loop / Click / Undo act on LYNX's toolbar, the LED and LCD follow;
   - faders 1–8 move inserts 1–8, fader 9 the master; pots follow the pot mode chosen on the keyboard (its Shift menu) and the LCD names them;
   - Shift + Track ◀ ▶ shows `9-16` etc. on the LCD's bottom row; Track ◀ ▶ changes the Channel Rack selection;
   - fader buttons mute (Shift: solo); Arm/Select then fader buttons arm inserts for recording;
   - the keys still play the selected channel and Settings › MIDI lists the `LKMK3 DAW` input as off;
   - quitting LYNX returns the keyboard to Standalone mode (pads back to its own layout, LCD cleared).
4. Debug builds print `Launchkey MK3 …: connected to …` via JUCE's `DBG`.

## What is untested

* **Everything on real hardware.** In particular: the exact port names JUCE reports on Fedora/PipeWire (matched by
  `lkmk3::isDawPortName`, which accepts `LKMK3 DAW` anywhere in the name), the CC numbers read from the manual's
  surface figure (Shift = CC 108 and Scene Launch / Stop-Solo-Mute = CC 104 / 105 on channel 1; buttons, pots, faders
  on channel 16), the colour palette approximation, LCD character handling, and that pulsing LEDs pulse.
* Whether two openers of the same ALSA sequencer port (LYNX's engine before it closes the port, and the surface) cause
  trouble during the switch-over at connect time.
* The full LYNX SHARE link with the integration: on the development box only the adapter and the patched `Main.cpp`
  were compiled against LYNX SHARE (`scripts/check-lynx-adapter.sh`); the box had no system packages for a full JUCE
  app build. CI's manual `lynx-full` job does the full build.
* Launchkey MK3 25 / 37 (no faders) and 61 — the protocol is shared, so they should work minus the faders.

## Limitations

* Parameter changes from the keyboard are, like LYNX's own MIDI-linked controllers, outside the undo history and do not
  mark the project as modified (`editSong (…, false)`); mute / solo are undoable.
* The DAW port is disabled in LYNX's MIDI input settings only for the session; if you save the MIDI settings page while
  connected, it stays disabled (which is what you want for this keyboard).
* The pots are absolute (not endless) on the MK3, hence soft takeover; the device's own *Pot Pickup* feature is left off.
* Capture MIDI, Quantise, Device Select / Lock and Scene ▲▼ / ► are not mapped yet (see roadmap).

## Roadmap

* Pads: Channel Rack steps of the selected channel with colour feedback and playhead; pattern selection.
* Device mode pots / faders: parameters of the selected channel's instrument or plugin.
* Capture MIDI → LYNX's *Dump score log to selected pattern*; Quantise → quick quantize of the selected pattern.
* Device Select pads → channel selection by pad.
* Settings page in LYNX SHARE (enable / disable, soft takeover on / off) once LYNX has a place for controller options.

## Versioning and contributing

[Semantic versioning](https://semver.org): the version lives in `include/lkmk3/version.hpp` and `project(VERSION)` in
`CMakeLists.txt` (a test checks they agree); releases are tagged `vX.Y.Z` and listed in [CHANGELOG.md](CHANGELOG.md).
Commits follow [Conventional Commits](https://www.conventionalcommits.org) (`feat(mapping): …`, `fix(protocol): …`).

## License

AGPL-3.0 (see [LICENSE](LICENSE)), the license of LYNX SHARE, which the adapter is compiled into.
"Launchkey" and "Novation" are trademarks of Focusrite Audio Engineering Ltd.; this project is not affiliated with them.
