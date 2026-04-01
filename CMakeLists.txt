cmake_minimum_required(VERSION 3.22...4.0)
project(Jacquard VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Suppress CMake 4.0 FetchContent deprecation warnings from JUCE's own CMakeLists
cmake_policy(SET CMP0169 OLD)

include(FetchContent)

# ── JUCE 8 ───────────────────────────────────────────────────────────────────
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ── CLAP-JUCE extensions ──────────────────────────────────────────────────────
# Provides CLAP format support; JUCE 8 does not bundle CLAP natively.
FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(clap-juce-extensions)

# ── Icon binary data ─────────────────────────────────────────────────────────
juce_add_binary_data(JacquardIcons
    SOURCES
        static_files/icons/coarser.svg
        static_files/icons/finer.svg
        static_files/icons/triplet.svg
        static_files/icons/quintuplet.svg
        static_files/icons/reset.svg
)

# ── Plugin target ─────────────────────────────────────────────────────────────
juce_add_plugin(Jacquard
    COMPANY_NAME                    "Jacquard Audio"
    PLUGIN_MANUFACTURER_CODE        Jcqd
    PLUGIN_CODE                     Jcq1
    FORMATS                         VST3 AU AUv3 Standalone
    PRODUCT_NAME                    "Jacquard"
    BUNDLE_ID                       "com.jacquard-audio.jacquard"
    DESCRIPTION                     "Microtonal composition via consonance optimization"

    IS_SYNTH                        TRUE
    NEEDS_MIDI_INPUT                FALSE
    NEEDS_MIDI_OUTPUT               TRUE
    IS_MIDI_EFFECT                  FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS     TRUE

    VST3_CATEGORIES                 "Instrument" "Tools"
    AU_MAIN_TYPE                    kAudioUnitType_MusicDevice

    COPY_PLUGIN_AFTER_BUILD         FALSE
)

# ── Sources ───────────────────────────────────────────────────────────────────
target_sources(Jacquard PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp

    src/model/ChordTimeline.cpp

    src/engine/SetharesEngine.cpp
    src/engine/HeatmapData.cpp
    src/engine/SnapEngine.cpp

    src/ipc/IpcServer.cpp
    src/ipc/IpcClient.cpp
    src/ipc/SessionManager.cpp

    src/midi/MpeAllocator.cpp
    src/midi/MpeOutputEngine.cpp

    src/ui/HeatmapComponent.cpp
    src/ui/TimelineBar.cpp
    src/ui/OptionsPanel.cpp
    src/ui/TransportPanel.cpp
    # SineBank is header-only
)

target_include_directories(Jacquard PRIVATE src)

# ── Compile definitions ───────────────────────────────────────────────────────
target_compile_definitions(Jacquard
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

# ── Link libraries ────────────────────────────────────────────────────────────
target_link_libraries(Jacquard
    PRIVATE
        JacquardIcons
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_generate_juce_header(Jacquard)

# ── CLAP format (via clap-juce-extensions) ────────────────────────────────────
clap_juce_extensions_plugin(
    TARGET              Jacquard
    CLAP_ID             "com.jacquard-audio.jacquard"
    CLAP_FEATURES       instrument utility
    CLAP_MANUAL_URL     "https://github.com/jacquard-audio/jacquard"
    CLAP_SUPPORT_URL    "https://github.com/jacquard-audio/jacquard/issues"
)
