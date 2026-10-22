cmake_minimum_required(VERSION 3.22)
project(Drummatic VERSION 0.2.4)
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
    BUNDLE_ID com.emiliofalciatori.drummatic
    PRODUCT_NAME "Drummatic"
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
    target_compile_definitions(ManipoloTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Drummatic")
    target_link_libraries(ManipoloTest PRIVATE juce::juce_audio_utils juce::juce_audio_formats juce::juce_recommended_config_flags)
endif()

if (MANIPOLO_TESTS)
    juce_add_console_app(DrummaticDemoKit PRODUCT_NAME "DrummaticDemoKit")
    target_sources(DrummaticDemoKit PRIVATE Tests/demo_kit.cpp Source/Params.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(DrummaticDemoKit PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Drummatic")
    target_link_libraries(DrummaticDemoKit PRIVATE juce::juce_audio_utils juce::juce_audio_formats juce::juce_recommended_config_flags)
endif()

# Drummatic hardware (Raspberry Pi 5): app a schermo intero + test del pannello
option(DRUMMATIC_HW "Compila l'app per la macchina hardware (Linux)" OFF)
if (DRUMMATIC_HW)
    juce_add_gui_app(DrummaticHW PRODUCT_NAME "drummatic-hw" VERSION 0.3.0)
    target_sources(DrummaticHW PRIVATE Source/HW/HardwareMain.cpp Source/Params.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(DrummaticHW PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1 JUCE_USE_FLAC=1
        JUCE_USE_OGGVORBIS=1 JUCE_JACK=0 JucePlugin_Name="Drummatic" JUCE_APPLICATION_NAME_STRING="Drummatic" JUCE_APPLICATION_VERSION_STRING="0.3.0")
    target_link_libraries(DrummaticHW PRIVATE juce::juce_audio_utils juce::juce_audio_formats juce::juce_recommended_config_flags)

    juce_add_console_app(DrummaticHWTest PRODUCT_NAME "DrummaticHWTest")
    target_sources(DrummaticHWTest PRIVATE Tests/hw_test.cpp Source/Params.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(DrummaticHWTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Drummatic")
    target_link_libraries(DrummaticHWTest PRIVATE juce::juce_audio_utils juce::juce_audio_formats juce::juce_recommended_config_flags)
endif()

if (MANIPOLO_TESTS)
    # il plugin intero per minuti, con random e parametri girati a caso: cerca i silenzi improvvisi
    juce_add_console_app(DrummaticSilenceTest PRODUCT_NAME "DrummaticSilenceTest")
    target_sources(DrummaticSilenceTest PRIVATE Tests/silence_proc_test.cpp Source/Params.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(DrummaticSilenceTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Drummatic")
    target_link_libraries(DrummaticSilenceTest PRIVATE juce::juce_audio_utils juce::juce_audio_formats juce::juce_recommended_config_flags)
endif()
