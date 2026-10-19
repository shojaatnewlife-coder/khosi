cmake_minimum_required(VERSION 3.22)
project(NeonEQ6 VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(NEON_FORMATS VST3 AU Standalone)
else()
    set(NEON_FORMATS VST3 Standalone)
endif()

juce_add_plugin(NeonEQ6
    COMPANY_NAME "Neon"
    PLUGIN_MANUFACTURER_CODE Neon
    PLUGIN_CODE Nq06
    FORMATS ${NEON_FORMATS}
    PRODUCT_NAME "Neon EQ 6"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(NeonEQ6)

target_sources(NeonEQ6 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_features(NeonEQ6 PRIVATE cxx_std_17)

target_compile_definitions(NeonEQ6 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(NeonEQ6 PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
