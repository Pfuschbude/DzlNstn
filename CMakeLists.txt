cmake_minimum_required(VERSION 3.22)
project(JoseModAmp VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE per CPM oder add_subdirectory einbinden
# Falls JUCE lokal liegt: add_subdirectory(JUCE)
# Hier vorbereitet für automatisches Clonen via FetchContent
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4 # Aktuelle stabile Version
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(JoseModAmp
    COMPANY_NAME "CustomAmps"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Cust
    PLUGIN_CODE Jmod
    FORMATS VST3 Standalone # CLAP kann via clap-juce-extensions hinzugefügt werden
    PRODUCT_NAME "Jose Modded Marshall 2203"
)

target_sources(JoseModAmp
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/JoseDspEngine.h
)

target_compile_definitions(JoseModAmp
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(JoseModAmp
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
