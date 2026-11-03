# Sources of the JUCE-free core (protocol + mapping), relative to this repository's root.
set(LKMK3_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(LKMK3_CORE_SOURCES
    ${LKMK3_ROOT}/src/version.cpp
    ${LKMK3_ROOT}/src/protocol.cpp
    ${LKMK3_ROOT}/src/palette.cpp
    ${LKMK3_ROOT}/src/controller.cpp
)
