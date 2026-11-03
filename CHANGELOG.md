# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses [Semantic Versioning](https://semver.org).

## [Unreleased]

## [0.2.0] - 2026-11-03

Pads and device mode. Still not tested on real hardware.

### Added
- Session-layout pads as a 16-step sequencer for the selected Channel Rack channel (current pattern) with channel
  colours, beat grid and playhead; Scene ▲ ▼ page through longer patterns.
- Scene Launch ► switches the pads to pattern selection (current pattern pulses, pattern colours).
- Drum layout pads play Channel Rack channels 1–16 at C5; holding Device Select lets the pads select a channel.
- *Device* pot / fader modes control parameters 1–16 of the selected channel's hosted plugin.
- LYNX SHARE adapter for steps (`Pattern::toggleStep`), playhead, pattern selection, live notes and plugin parameters.

### Changed
- `DawHost` has new pure virtual functions for notes, steps, patterns and plugin parameter counts (breaking for other
  hosts; allowed in 0.x).

## [0.1.0] - 2026-11-03

First milestone: handshake, transport and mixer. Not yet tested on real hardware.

### Added
- Protocol layer for the Launchkey MK3 DAW mode (Programmer's reference manual): DAW mode enable / disable, touch and
  pot-pickup switches, pad / pot / fader mode report and select, decoding of pots, faders (incl. master), fader buttons,
  transport, navigation, shift, touch, pads (session, drum, device select) and device inquiry; LED colour (stationary,
  flashing, pulsing, grayscale) and LCD SysEx encoding; DAW port name matching.
- 128-entry colour palette sampled from the manual with nearest-colour matching.
- `DawHost` interface and the `Controller` mapping: handshake on connect / disconnect, Play / Stop / Record / Loop
  (PAT/SONG) / Click / Undo / Redo, channel select (Track ◀ ▶), 8-strip mixer bank (Shift + Track ◀ ▶), faders to bank
  volume or sends and the master, pots to volume / pan / send A / send B per the reported pot mode, fader buttons to
  mute / solo / record arm, soft takeover, diffed LED feedback (transport on the Stop/Solo/Mute LED, track colours) and
  LCD text (transport state, channel and bank, parameter names and values).
- LYNX SHARE adapter (`lynx/`): `LynxDawHost` on `lynx::AudioEngine` and `LaunchkeyMk3Surface` (hot-plug, DAW port
  handling, message-thread timer), an opt-in patch for LYNX SHARE 0.1.22 (`caac540`) and
  `scripts/check-lynx-adapter.sh`.
- Unit tests with a simulated Launchkey MK3 49 and a fake DAW; GitHub Actions CI (GCC, Clang, ASan/UBSan, adapter
  compile check against LYNX SHARE, optional full LYNX build).

[Unreleased]: https://github.com/Sciddie/lauchkey-mk3-lynx/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/Sciddie/lauchkey-mk3-lynx/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/Sciddie/lauchkey-mk3-lynx/releases/tag/v0.1.0
