cmake_minimum_required(VERSION 3.25)

# The compiler settings (C++20, MSVC debug info and runtime, ccache, RTSan) live in the root CMakeLists.txt:
# build through the root project (CMakePresets.json), not from this directory.
project(TracklabEngineSpike VERSION 0.1.0 LANGUAGES C CXX)

option(SPIKE_WERROR "Treat warnings in spike/engine/src as errors" ON)

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
  "--spike-gain-vst3=$<TARGET_GENEX_EVAL:SpikeGain_VST3,$<TARGET_PROPERTY:SpikeGain_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>")

# One CTest entry per doctest suite. Exit code 77 means "skipped" (e.g. lame/ffmpeg missing), never "passed".
foreach(suite IN ITEMS import render record12 vst3 mp3 cli)
  add_test(NAME spike.${suite}
    COMMAND spike_tests --test-suite=${suite} ${SPIKE_TEST_ARGS})
  set_tests_properties(spike.${suite} PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 300
    LABELS "spike;${suite}")
endforeach()

#==============================================================================
# RealtimeSanitizer (TRACKLAB_RTSAN, see the root CMakeLists.txt and docs/realtime.md).
if(TRACKLAB_RTSAN)
  # SpikeGain.so is a shared module: clang does not link the static RTSan runtime into shared libraries and JUCE
  # links plugins with --no-undefined. The runtime lives in the host process (spike_tests, spike_cli), which exports
  # its __rtsan_* symbols, so the plugin resolves them when the host loads it. That way the annotated
  # SpikeGain processBlock is checked like our other audio callbacks.
  target_link_options(SpikeGain_VST3 PRIVATE -Wl,-z,undefs)
  foreach(host IN ITEMS spike_tests spike_cli)
    target_link_options(${host} PRIVATE "-Wl,--export-dynamic-symbol=__rtsan_*")
  endforeach()

  # Every finding stops the test (halt_on_error). Third-party findings are filtered by scripts/rtsan.supp. Leak
  # detection is not the point of this run (JUCE keeps singletons until exit).
  set(SPIKE_RTSAN_OPTIONS "halt_on_error=true:detect_leaks=0:suppressions=${CMAKE_SOURCE_DIR}/scripts/rtsan.supp")
  if(TRACKLAB_RTSAN_SYMBOLIZER)
    string(APPEND SPIKE_RTSAN_OPTIONS ":external_symbolizer_path=${TRACKLAB_RTSAN_SYMBOLIZER}")
  endif()
  # Matching every report against the suppressions symbolizes its stack, which makes the record12 and cli suites
  # about 30 times slower (4 s -> 2 min on 4 cores): the timeout is raised for this preset.
  foreach(suite IN ITEMS import render record12 vst3 mp3 cli)
    set_property(TEST spike.${suite} APPEND PROPERTY ENVIRONMENT "RTSAN_OPTIONS=${SPIKE_RTSAN_OPTIONS}")
    set_tests_properties(spike.${suite} PROPERTIES TIMEOUT 1500)
  endforeach()

  # Negative test: a deliberately allocating callback (tests/test_rtsan.cpp) must stop the process with an RTSan
  # report. The test case is skipped unless --no-skip is given, so a normal run of all suites never aborts. The wrapper
  # script succeeds only if the process failed AND the output is an RTSan finding in the callback: a crash for any other
  # reason, or a missing/ineffective sanitizer, turns this test red. (CTest's WILL_FAIL cannot tell these cases apart.)
  add_test(NAME spike.rtsan_negative
    COMMAND "${CMAKE_COMMAND}"
      "-DTEST_EXE=$<TARGET_FILE:spike_tests>"
      "-DTEST_ARGS=--test-suite=rtsan_negative;--no-skip"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rtsan_expect_failure.cmake")
  set_tests_properties(spike.rtsan_negative PROPERTIES
    ENVIRONMENT "RTSAN_OPTIONS=${SPIKE_RTSAN_OPTIONS}"
    TIMEOUT 120
    LABELS "spike;rtsan_negative")
endif()
