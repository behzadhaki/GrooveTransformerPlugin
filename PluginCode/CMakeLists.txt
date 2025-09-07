project(GrooveTransformerPlugin VERSION 0.0.1)

set (BaseTargetName GrooveTransformerPlugin)

add_definitions(-DPROJECT_NAME="${BaseTargetName}")

juce_add_plugin("${BaseTargetName}"
        COMPANY_NAME "MusicTechnologyGroup"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE MTG1   # 4-char code, matches Info.plist
        PLUGIN_CODE MNLM                # subtype
        FORMATS AU VST3 Standalone
        PRODUCT_NAME "GrooveTransformerPlugin")

#
## ONLY NEEDED FOR LINUX and MACOS (for installing libtorch in root directory)
## You don't need this if you are not planning to distribute the plugin
#set(SUDO_PASSWORD "your_password_here" CACHE STRING "Sudo password")

# DO NOT REMOVE THIS LINE
if (MSVC)
    include(../CMake/CMakeLists.MSVC.txt)
elseif (APPLE)
   include(../CMake/CMakeLists.ARM.txt)
   # include(../CMakeLists.UNIX.txt)
elseif (UNIX)
    include(../CMake/CMakeLists.UNIX.txt)
endif()

message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")