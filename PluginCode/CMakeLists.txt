project(GrooveTransformerPlugin VERSION 0.0.1)

set (BaseTargetName GrooveTransformerPlugin)

add_definitions(-DPROJECT_NAME="${BaseTargetName}")

juce_add_plugin("${BaseTargetName}"
        # VERSION ...                               # Set this if the plugin version is different to the project version
        # ICON_BIG ...                              # ICON_* arguments specify a path to an image file to use as an icon for the Standalone
        # ICON_SMALL ...
        COMPANY_NAME "MusicTechnologyGroup"                 # Replace with a tag identifying your name
        IS_SYNTH FALSE                               # There is no MIDI vst3 plugin format, so we are going to assume a midi instrument plugin
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        AU_MAIN_TYPE kAudioUnitType_MIDIProcessor
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE                 # copies the plugin to user plugins folder so as to easily load in DAW
        PLUGIN_MANUFACTURER_CODE Juce                #
        PLUGIN_CODE MNLM                             # MUST BE UNIQUE!! If similar to other plugins, conflicts will occur
        FORMATS AU VST3 Standalone
        PRODUCT_NAME "GrooveTransformerPlugin")           # Replace with your plugin title
#
## ONLY NEEDED FOR LINUX and MACOS (for installing libtorch in root directory)
## You don't need this if you are not planning to distribute the plugin
#set(SUDO_PASSWORD "your_password_here" CACHE STRING "Sudo password")

# DO NOT REMOVE THIS LINE
if (MSVS)
    include(../CMakeLists.MSVC.txt)
elseif (APPLE)
   include(../CMakeLists.ARM.txt)
   # include(../CMakeLists.UNIX.txt)
elseif (UNIX)
    include(../CMakeLists.UNIX.txt)
endif()
