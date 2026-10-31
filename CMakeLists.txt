cmake_minimum_required(VERSION 3.22)

project(QpasaReverb VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build universal (Apple Silicon + Intel) binaries on macOS, and support older macOS versions.
if(APPLE)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    endif()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

# Static MSVC runtime so the Windows plugin has no extra DLL dependencies.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE: use a local checkout if given (-DJUCE_DIR=...), otherwise download it.
set(JUCE_DIR "" CACHE PATH "Optional path to a local JUCE checkout")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(QpasaReverb
    PRODUCT_NAME "Qpasa Reverb"
    COMPANY_NAME "Qpasa Audio"
    BUNDLE_ID com.qpasaaudio.qpasareverb
    PLUGIN_MANUFACTURER_CODE Qpas
    PLUGIN_CODE Qprv
    FORMATS VST3 AU
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Reverb
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(QpasaReverb PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(QpasaReverb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(QpasaReverb
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Small offline test of the reverb engine (no JUCE needed).
option(QPASA_BUILD_TESTS "Build the engine test" OFF)
if(QPASA_BUILD_TESTS)
    add_executable(EngineTest tests/EngineTest.cpp)
    enable_testing()
    add_test(NAME EngineTest COMMAND EngineTest)
endif()
