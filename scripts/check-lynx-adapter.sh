#!/usr/bin/env bash
# Verifies the LYNX SHARE adapter against LYNX SHARE's real sources without building the whole DAW:
#   1. fetches LYNX SHARE (pinned commit, shallow) and JUCE 8.0.4 (checksum verified) into $WORK
#   2. applies lynx/patches/*.patch to LYNX SHARE (must apply cleanly)
#   3. compiles lynx/*.cpp and the patched src/Main.cpp to object files with LYNX's JUCE configuration
#
# Usage: scripts/check-lynx-adapter.sh [work-dir]     (env: CXX, LYNX_REF, LYNX_DIR, JUCE_DIR)
# Linking needs the full LYNX build (see README "Building LYNX SHARE with the integration").
set -euo pipefail

here="$(cd "$(dirname "$0")/.." && pwd)"
work="${1:-$here/build/lynx-check}"
lynx_ref="${LYNX_REF:-caac54077f233fc025c29262a283eaf652ab33ba}"
juce_version=8.0.4
juce_sha256=0effe9823a6fd8a4f98e5c08be7d308350e417b2d15dcf34011e778fefbbf005
cxx="${CXX:-c++}"
mkdir -p "$work"

lynx="${LYNX_DIR:-$work/LYNX-SHARE}"
if [[ ! -d "$lynx/.git" ]]; then
    git init -q "$lynx"
    git -C "$lynx" remote add origin https://github.com/Ka1-S/LYNX-SHARE.git
fi
if [[ -z "${LYNX_DIR:-}" ]]; then
    git -C "$lynx" fetch -q --depth 1 origin "$lynx_ref"
    git -C "$lynx" checkout -q --force FETCH_HEAD
fi
git -C "$lynx" checkout -q -- .
for p in "$here"/lynx/patches/*.patch; do
    git -C "$lynx" apply --check "$p"
    git -C "$lynx" apply "$p"
    echo "applied $(basename "$p")"
done

juce="${JUCE_DIR:-$work/juce}"
if [[ ! -f "$juce/modules/juce_core/juce_core.h" ]]; then
    curl -fsSL -o "$work/juce.tar.gz" "https://github.com/juce-framework/JUCE/archive/refs/tags/$juce_version.tar.gz"
    echo "$juce_sha256  $work/juce.tar.gz" | sha256sum -c -
    mkdir -p "$juce" && tar xzf "$work/juce.tar.gz" -C "$juce" --strip-components 1 && rm "$work/juce.tar.gz"
fi

mkdir -p "$work/gen" "$work/obj"
printf '#pragma once\n#define LYNX_GIT_COMMIT "%s"\n' "${lynx_ref:0:10}" > "$work/gen/lynx_build.h"

modules=(juce_core juce_events juce_data_structures juce_graphics juce_gui_basics juce_gui_extra juce_audio_basics
         juce_audio_devices juce_audio_formats juce_audio_processors juce_audio_utils juce_dsp juce_opengl)
defs=(-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_STANDALONE_APPLICATION=1 -DJUCE_WEB_BROWSER=0 -DJUCE_USE_CURL=0
      -DJUCE_PLUGINHOST_VST3=1 -DJUCE_ALSA=1 -DJUCE_JACK=1 -DJUCE_PLUGINHOST_LV2=1 -DLYNX_WITH_LAUNCHKEY_MK3=1
      '-DJUCE_APPLICATION_NAME_STRING="LYNX"' '-DJUCE_APPLICATION_VERSION_STRING="0.0.0"')
for m in "${modules[@]}"; do defs+=("-DJUCE_MODULE_AVAILABLE_$m=1"); done
incs=(-I"$juce/modules" -I"$lynx/src" -I"$lynx/third_party/sse2neon" -I"$lynx/third_party/clap/include"
      -I"$work/gen" -I"$here/include" -I"$here/lynx")

status=0
for f in "$here/lynx/LynxDawHost.cpp" "$here/lynx/LaunchkeyMk3Surface.cpp" "$lynx/src/Main.cpp"; do
    echo "compiling $(basename "$f")"
    if ! "$cxx" -std=c++20 -Wall -Wextra -c -o "$work/obj/$(basename "$f").o" "${defs[@]}" "${incs[@]}" "$f"; then
        status=1
    fi
done
[[ $status == 0 ]] && echo "OK: the adapter and the patched LYNX SHARE Main.cpp compile against LYNX SHARE ${lynx_ref:0:10}"
exit $status
