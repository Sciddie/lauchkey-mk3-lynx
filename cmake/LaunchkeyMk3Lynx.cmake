# Adds the Launchkey MK3 integration to a LYNX SHARE app target. Used from LYNX SHARE's CMakeLists.txt (see
# lynx/patches/), e.g.:
#
#   include(${LYNX_LAUNCHKEY_MK3_DIR}/cmake/LaunchkeyMk3Lynx.cmake)
#   lkmk3_add_to_lynx(LYNX)
#
# The target must already link LYNX's lynx_core and JUCE's juce_audio_devices (LYNX's app target does).

include(${CMAKE_CURRENT_LIST_DIR}/Sources.cmake)

function(lkmk3_add_to_lynx target)
    target_sources(${target} PRIVATE
        ${LKMK3_CORE_SOURCES}
        ${LKMK3_ROOT}/lynx/LynxDawHost.cpp
        ${LKMK3_ROOT}/lynx/LaunchkeyMk3Surface.cpp)
    target_include_directories(${target} PRIVATE ${LKMK3_ROOT}/include ${LKMK3_ROOT}/lynx)
    target_compile_definitions(${target} PRIVATE LYNX_WITH_LAUNCHKEY_MK3=1)
endfunction()
