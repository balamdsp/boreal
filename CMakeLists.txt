cmake_minimum_required(VERSION 3.22)

cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(Boreal VERSION 0.1.0)

# Optimized builds by default (single-config dev builds otherwise link -O0).
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set (CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.1
)
FetchContent_MakeAvailable(JUCE)

FetchContent_Declare(
    loris
    GIT_REPOSITORY https://github.com/madrona-labs/loris.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(loris)

file(GLOB LORIS_CXX_SOURCES "${loris_SOURCE_DIR}/src/*.C")
target_sources(loris PRIVATE ${LORIS_CXX_SOURCES})

if (MSVC)
    target_compile_definitions(loris PRIVATE _HAS_AUTO_PTR_ETC=1)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    target_compile_definitions(loris PRIVATE _LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION)
    target_compile_options(loris PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/source/Helpers/LorisStdCompat.h")
endif()

# CLAP wrapper (pinned development revision).
FetchContent_Declare(
    clap_juce_extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        c1a5ad0
)
FetchContent_MakeAvailable(clap_juce_extensions)

option(BOREAL_COPY_AFTER_BUILD "Copy built plugins into the system plugin folders" ON)

if (BOREAL_COPY_AFTER_BUILD)
    set_property(GLOBAL PROPERTY JUCE_COPY_PLUGIN_AFTER_BUILD TRUE)
endif()

juce_add_plugin(Boreal
    COMPANY_NAME "BalamDSP"
    PLUGIN_MANUFACTURER_CODE "Blam"
    PLUGIN_CODE "Brl1"
    FORMATS Standalone VST3 AU
    PRODUCT_NAME "Boreal"
    VERSION ${PROJECT_VERSION}
    PLUGIN_DESCRIPTION "Four-way Loris morphing synth"
    VST3_CATEGORIES "Instrument" "Synth"
    LV2URI "https://balamdsp.com/plugins/Boreal"
    AAX_CATEGORY "AAX_ePlugInCategory_SWGenerators"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
)

clap_juce_extensions_plugin(TARGET Boreal
    CLAP_ID "com.balamdsp.Boreal"
    CLAP_FEATURES "instrument" "synthesizer")

juce_generate_juce_header(Boreal)

target_compile_definitions(Boreal
    PUBLIC
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_WIN_PER_MONITOR_DPI_AWARE=0
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    target_compile_definitions(Boreal PRIVATE _LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION)
endif()

if (MSVC)
    target_compile_definitions(Boreal PRIVATE _HAS_AUTO_PTR_ETC=1)
endif()

target_sources(Boreal
    PRIVATE
        source/Components/AudioSettingsPanel.h
        source/Components/CollectionBrowser.h
        source/Components/BipolarFader.h
        source/Components/TransportIconButton.h
        source/Components/SquareFader.h
        source/Components/SquareKnob.h
        source/Components/CRTMath.h
        source/Components/CRTNoise.h
        source/Components/CRTScreen.h
        source/Components/PadXY.h
        source/Components/Slider.h
        source/Analysis/BorealAnalyzeQueue.h
        source/Analysis/BorealAnalyzer.h
        source/Analysis/BorealAnalyzer.cpp
        source/Helpers/BorealConstants.h
        source/Helpers/BorealParameters.h
        source/Helpers/BorealFrameExtents.cpp
        source/Helpers/CustomLookAndFeel.h
        source/Helpers/InterfaceDefines.h
        source/Managers/BorealPresetManager.h
        source/Managers/BorealPresetManager.cpp
        source/AboutWindow.h
        source/Panels/AnalyzerWindow.h
        source/Panels/BorealPanel.h
        source/Panels/CorePanel.h
        source/Panels/MainPanel.h
        source/Panels/MorphingPanel.h
        source/Panels/PartialsView.h
        source/Panels/PresetManagerPanel.h
        source/Panels/SettingsPanel.h
        source/Panels/SoundPanel.h
        source/Panels/TopPanel.h
        source/Panels/WaveformView.h
        source/MorphEngine.h
        source/MorphEngine.cpp
        source/PluginProcessor.h
        source/PluginProcessor.cpp
        source/PluginEditor.h
)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(Boreal PRIVATE X11)
endif()

target_include_directories(Boreal
    PRIVATE
        ${loris_SOURCE_DIR}/include
        ${loris_SOURCE_DIR}/src
)

juce_add_binary_data(Boreal_BinaryData
    HEADER_NAME "BinaryData.h"
    NAMESPACE "BinaryData"
    SOURCES
        source/fonts/VT323-Regular.ttf
)

target_link_libraries(Boreal PRIVATE Boreal_BinaryData)

target_compile_definitions(Boreal_Standalone PRIVATE
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
)
target_sources(Boreal_Standalone PRIVATE
    source/Standalone/CustomStandalone.cpp
)

target_link_libraries(Boreal
    PRIVATE
        loris
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_processors_headless
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_opengl
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
