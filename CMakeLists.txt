cmake_minimum_required(VERSION 3.22)
project(DiezelEinstein VERSION 1.0.0)

# JUCE 8 via FetchContent laden
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(DiezelEinstein
    COMPANY_NAME "Pfuschbude"
    PLUGIN_MANUFACTURER_CODE Pfsc
    PLUGIN_CODE DzEi
    FORMATS VST3 Standalone
    PRODUCT_NAME "Diezel Einstein Amp"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
)

target_sources(DiezelEinstein PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/DiezelDspEngine.h
)

target_compile_features(DiezelEinstein PRIVATE cxx_std_20)

target_link_libraries(DiezelEinstein PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
)
