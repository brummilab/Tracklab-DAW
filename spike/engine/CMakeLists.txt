cmake_minimum_required(VERSION 3.25)

# MSVC: embed debug info in the object files (/Z7) so that sccache can cache MSVC builds (CMP0141),
# static runtime so that spike_cli.exe and SpikeGain.vst3 need no redistributable.
if(POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
endif()
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(TracklabEngineSpike VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SPIKE_WERROR "Treat warnings in spike/engine/src as errors" ON)

# Use ccache automatically if present and no launcher was requested (CI passes sccache/ccache itself).
find_program(SPIKE_CCACHE ccache)
if(SPIKE_CCACHE AND NOT CMAKE_CXX_COMPILER_LAUNCHER AND NOT MSVC)
  set(CMAKE_C_COMPILER_LAUNCHER "${SPIKE_CCACHE}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${SPIKE_CCACHE}")
endif()

include(CTest)
include(cmake/Deps.cmake)

# Warning flags for our own sources only (never for JUCE/Tracktion module code).
add_library(spike_warnings INTERFACE)
target_link_libraries(spike_warnings INTERFACE juce::juce_recommended_warning_flags)
if(SPIKE_WERROR)
  if(MSVC)
    target_compile_options(spike_warnings INTERFACE /W4 /WX /external:W0)
  else()
    target_compile_options(spike_warnings INTERFACE -Wall -Wextra -Werror)
  endif()
endif()

# JUCE's recommended config flags add /Zi on MSVC (its directory scope does not see CMP0141 NEW), which would
# defeat the /Z7 setting that sccache needs. CMake's own defaults (/Od, /O2, /EHsc) are enough on MSVC.
add_library(spike_config_flags INTERFACE)
if(NOT MSVC)
  target_link_libraries(spike_config_flags INTERFACE juce::juce_recommended_config_flags)
endif()

target_link_libraries(spike_modules PRIVATE spike_config_flags)

#==============================================================================
# spike_core: everything in src/ except the CLI main() and the plugin.
file(GLOB SPIKE_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER SPIKE_CORE_SOURCES EXCLUDE REGEX "/spike_cli\\.cpp$")
add_library(spike_core STATIC ${SPIKE_CORE_SOURCES})
target_include_directories(spike_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(spike_core PUBLIC spike_modules_api PRIVATE spike_warnings spike_config_flags)

add_executable(spike_cli src/spike_cli.cpp)
target_link_libraries(spike_cli PRIVATE spike_core spike_warnings)

#==============================================================================
# SpikeGain: our own VST3 test plugin (no third-party binary in the repo).
juce_add_plugin(SpikeGain
  PRODUCT_NAME "SpikeGain"
  COMPANY_NAME "Tracklab"
  PLUGIN_MANUFACTURER_CODE Trkl
  PLUGIN_CODE Spgn
  FORMATS VST3
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  NEEDS_CURL FALSE
  NEEDS_WEB_BROWSER FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE)
file(GLOB SPIKE_GAIN_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/SpikeGain/*.cpp")
target_sources(SpikeGain PRIVATE ${SPIKE_GAIN_SOURCES})
target_include_directories(SpikeGain PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(SpikeGain PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(SpikeGain
  PRIVATE
    juce::juce_audio_utils
    spike_defs
  PUBLIC
    spike_config_flags)
# The plugin target also compiles the JUCE module code, so warnings (-Werror) apply per source file.
get_target_property(SPIKE_WARNING_OPTIONS spike_warnings INTERFACE_COMPILE_OPTIONS)
get_target_property(SPIKE_JUCE_WARNING_OPTIONS juce::juce_recommended_warning_flags INTERFACE_COMPILE_OPTIONS)
list(APPEND SPIKE_WARNING_OPTIONS ${SPIKE_JUCE_WARNING_OPTIONS})
set_source_files_properties(${SPIKE_GAIN_SOURCES} PROPERTIES COMPILE_OPTIONS "${SPIKE_WARNING_OPTIONS}")

#==============================================================================
# Tests (doctest from Tracktion's modules/3rd_party, no new dependency).
file(GLOB SPIKE_TEST_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures-gen/*.cpp")
add_executable(spike_tests ${SPIKE_TEST_SOURCES})
target_include_directories(spike_tests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/tests"
  "${SPIKE_TRACKTION_DIR}/modules/3rd_party/doctest")
target_link_libraries(spike_tests PRIVATE spike_core spike_config_flags)
add_dependencies(spike_tests spike_cli SpikeGain_VST3)

set(SPIKE_TEST_ARGS
  "--spike-cli=$<TARGET_FILE:spike_cli>"
  "--spike-gain-vst3=$<TARGET_PROPERTY:SpikeGain_VST3,JUCE_PLUGIN_ARTEFACT_FILE>")

# One CTest entry per doctest suite. Exit code 77 means "skipped" (e.g. lame/ffmpeg missing), never "passed".
foreach(suite IN ITEMS import render record12 vst3 mp3 cli)
  add_test(NAME spike.${suite}
    COMMAND spike_tests --test-suite=${suite} ${SPIKE_TEST_ARGS})
  set_tests_properties(spike.${suite} PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 300
    LABELS "spike;${suite}")
endforeach()
