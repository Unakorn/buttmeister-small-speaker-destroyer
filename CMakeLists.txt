cmake_minimum_required(VERSION 3.22)

project(HoneyBadgerHolyGrail VERSION 0.3.0)

set(default_juce_dir "$ENV{JUCE_DIR}")
if (NOT default_juce_dir)
    set(default_juce_dir "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE")
endif()
set(JUCE_DIR "${default_juce_dir}" CACHE PATH "Path to JUCE")
if (NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Supply JUCE 8.0.12 using -DJUCE_DIR=path/to/JUCE or external/JUCE")
endif()
add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")

juce_add_plugin(HoneyBadgerHolyGrail
    COMPANY_NAME "Buttmeister Inc"
    BUNDLE_ID "com.buttmeisterinc.buttmeistersmallspeakerdestroyer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE ButM
    PLUGIN_CODE Bssd
    FORMATS VST3 Standalone
    PRODUCT_NAME "Buttmeister Small Speaker Destroyer")

juce_generate_juce_header(HoneyBadgerHolyGrail)

target_sources(HoneyBadgerHolyGrail
    PRIVATE
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp)

target_compile_features(HoneyBadgerHolyGrail PRIVATE cxx_std_17)

if (MSVC)
    target_compile_options(HoneyBadgerHolyGrail PRIVATE /FS)
endif()

target_compile_definitions(HoneyBadgerHolyGrail
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(HoneyBadgerHolyGrail
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
