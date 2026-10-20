cmake_minimum_required(VERSION 3.22)
project(Manipolo VERSION 0.2.1)
set(CMAKE_CXX_STANDARD 17)

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt)
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 7.0.12 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

juce_add_plugin(Manipolo
    COMPANY_NAME "Emilio Falciatori"
    BUNDLE_ID com.emiliofalciatori.manipolo
    PRODUCT_NAME "Manipolo"
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_AUTO_MANIFEST FALSE
    PLUGIN_MANUFACTURER_CODE Efal
    PLUGIN_CODE Mnpl
    VST3_CATEGORIES Instrument Drum)

target_sources(Manipolo PRIVATE
    Source/Params.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Manipolo PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_FLAC=1
    JUCE_USE_OGGVORBIS=1)

target_link_libraries(Manipolo
    PRIVATE juce::juce_audio_utils juce::juce_audio_formats
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

option(MANIPOLO_TESTS "Compila i test" OFF)
if (MANIPOLO_TESTS)
    juce_add_console_app(ManipoloTest PRODUCT_NAME "ManipoloTest")
    target_sources(ManipoloTest PRIVATE Tests/proc_test.cpp Source/Params.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(ManipoloTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Manipolo")
    target_link_libraries(ManipoloTest PRIVATE juce::juce_audio_utils juce::juce_audio_formats juce::juce_recommended_config_flags)
endif()
