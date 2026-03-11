cmake_minimum_required(VERSION 3.22)
project(Zeitraum VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(lib/JUCE)

juce_add_plugin(Zeitraum
    COMPANY_NAME "Die stille Erde"
    BUNDLE_ID "com.diestilleerde.zeitraum"
    PLUGIN_MANUFACTURER_CODE DsEr
    PLUGIN_CODE ZtRm
    FORMATS VST3 AU
    PRODUCT_NAME "Zeitraum"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Delay
)

set(PLUGIN_SOURCES
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
)

target_sources(Zeitraum PRIVATE ${PLUGIN_SOURCES})
target_include_directories(Zeitraum PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_compile_definitions(Zeitraum PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    DONT_SET_USING_JUCE_NAMESPACE=1
)

target_link_libraries(Zeitraum
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_basics
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Testing (requires network on first configure to fetch Catch2)
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(ZeitraumTests
    test/PluginTests.cpp
    test/dsp/OnePoleSmoothTests.cpp
    test/dsp/CharacterProcessorTests.cpp
    test/dsp/TapReaderTests.cpp
    test/dsp/DelayEngineTests.cpp
    test/dsp/FeedbackMatrixTests.cpp
    test/dsp/FeedbackFilterTests.cpp
    test/dsp/FeedbackSaturatorTests.cpp
    test/RandomizerTests.cpp
    ${PLUGIN_SOURCES}
)
target_include_directories(ZeitraumTests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ZeitraumTests PRIVATE
    Catch2::Catch2WithMain
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_dsp
)
target_compile_definitions(ZeitraumTests PRIVATE
    $<TARGET_PROPERTY:Zeitraum,COMPILE_DEFINITIONS>
)

include(CTest)
add_test(NAME ZeitraumTests COMMAND ZeitraumTests)
