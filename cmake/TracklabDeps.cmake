# Shared dependencies of Tracklab (src/, tests/) and the engine spike (spike/engine): JUCE (first!) and Tracktion
# Engine, both as git submodules under third_party/. Included once from the root CMakeLists.txt.
#
# Pins (see docs/adr/ADR-001-tech-stack.md):
#   JUCE              tag 9.0.3   (be29c81)   fallback 8.0.15 (91ad83a)
#   Tracktion Engine  develop     (bb38617, 3.5.0)   fallback v3.2.0 (0a5f4e6)
# Tracktion's own nested submodule modules/juce is NOT initialised: Tracktion uses the JUCE of the
# parent project when the target juce::juce_core already exists.

set(TRACKLAB_THIRD_PARTY_DIR "${CMAKE_CURRENT_LIST_DIR}/../third_party")
set(TRACKLAB_JUCE_DIR "${TRACKLAB_THIRD_PARTY_DIR}/JUCE")
set(TRACKLAB_TRACKTION_DIR "${TRACKLAB_THIRD_PARTY_DIR}/tracktion_engine")

foreach(dir IN ITEMS "${TRACKLAB_JUCE_DIR}" "${TRACKLAB_TRACKTION_DIR}")
  if(NOT EXISTS "${dir}/CMakeLists.txt")
    message(FATAL_ERROR
      "Submodule missing: ${dir}\n"
      "Run: git submodule update --init third_party/JUCE third_party/tracktion_engine\n"
      "(do not use --recursive: Tracktion's nested JUCE submodule is not needed)")
  endif()
endforeach()

set(TE_ADD_EXAMPLES OFF CACHE BOOL "Tracktion examples are not built" FORCE)

# JUCE first, then Tracktion (which then finds juce::juce_core and skips its own JUCE).
add_subdirectory("${TRACKLAB_JUCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/juce" EXCLUDE_FROM_ALL)
add_subdirectory("${TRACKLAB_TRACKTION_DIR}" "${CMAKE_BINARY_DIR}/_deps/tracktion" EXCLUDE_FROM_ALL)

# Definitions for ALL targets (Brief M0-06).
add_library(tracklab_defs INTERFACE)
target_compile_definitions(tracklab_defs INTERFACE
  JUCE_USE_MP3AUDIOFORMAT=1       # explicit: default differs between JUCE 8 (0) and 9 (1)
  JUCE_USE_WINDOWS_MEDIA_FORMAT=0 # same MP3 decoder on every platform
  JUCE_PLUGINHOST_VST3=1
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0
  JUCE_MODAL_LOOPS_PERMITTED=1
  JUCE_STRICT_REFCOUNTEDPOINTER=1
  TRACKTION_ENABLE_SINGLETONS=0
  TRACKTION_LOG_DEVICES=0)
# Audio backends (M1-06, DESIGN Rev 2 E36). Set explicitly although some are JUCE defaults, so that a JUCE update
# cannot drop one silently.
#   Linux:   JACK API (through pipewire-jack, started with `pw-jack`) is the standard, ALSA the second option.
#   Windows: ASIO with the SDK bundled in JUCE (GPLv3 option of its dual licence, THIRD_PARTY_NOTICES.md), and WASAPI
#            (shared, exclusive and low-latency modes are created by JUCE).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions(tracklab_defs INTERFACE JUCE_ALSA=1 JUCE_JACK=1)
elseif(WIN32)
  # JUCE_ASIO_USE_EXTERNAL_SDK=0: the SDK bundled in JUCE (GPLv3 option), never a separately downloaded copy.
  target_compile_definitions(tracklab_defs INTERFACE JUCE_ASIO=1 JUCE_ASIO_USE_EXTERNAL_SDK=0 JUCE_WASAPI=1)
endif()

# JUCE's recommended config flags add /Zi on MSVC (its directory scope does not see CMP0141 NEW), which would
# defeat the /Z7 setting that sccache needs. CMake's own defaults (/Od, /O2, /EHsc) are enough on MSVC.
add_library(tracklab_config_flags INTERFACE)
if(NOT MSVC)
  target_link_libraries(tracklab_config_flags INTERFACE juce::juce_recommended_config_flags)
else()
  # Source files are UTF-8 (German text in string literals); without this MSVC reads them in the local code page.
  target_compile_options(tracklab_config_flags INTERFACE /utf-8)
endif()

# --- Module code is compiled exactly once ------------------------------------------------------
# JUCE modules are INTERFACE libraries whose .cpp files are added to EVERY target that links them.
# Linking them into tracklab_engine, spike_core and the test executables would compile Tracktion three times.
# Therefore: tracklab_juce_tracktion (static) links the modules PRIVATE and holds the only copy of the module
# code; tracklab_juce_tracktion_api re-exports include directories and compile definitions (without sources).
add_library(tracklab_juce_tracktion STATIC "${CMAKE_CURRENT_LIST_DIR}/tracklab_modules_anchor.cpp")
target_link_libraries(tracklab_juce_tracktion PRIVATE
  tracklab_defs
  tracktion::tracktion_engine
  tracktion::tracktion_graph
  tracktion::tracktion_core
  juce::juce_audio_utils
  juce::juce_audio_devices
  juce::juce_audio_formats
  juce::juce_audio_processors
  juce::juce_dsp)
set_target_properties(tracklab_juce_tracktion PROPERTIES
  C_VISIBILITY_PRESET hidden
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON)
target_link_libraries(tracklab_juce_tracktion PRIVATE tracklab_config_flags)
target_compile_features(tracklab_juce_tracktion PUBLIC cxx_std_20)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(tracklab_juce_tracktion PUBLIC atomic)
endif()

# Collect usage requirements (include dirs, compile definitions) of the module closure.
set(_tracklab_seen "")
set(_tracklab_incs "")
set(_tracklab_defs "")
function(_tracklab_collect tgt)
  if(NOT TARGET "${tgt}")
    return()
  endif()
  get_target_property(_aliased "${tgt}" ALIASED_TARGET)
  if(_aliased)
    set(tgt "${_aliased}")
  endif()
  if("${tgt}" IN_LIST _tracklab_seen)
    return()
  endif()
  list(APPEND _tracklab_seen "${tgt}")
  get_target_property(_type "${tgt}" TYPE)
  if(NOT _type STREQUAL "INTERFACE_LIBRARY")
    set(_tracklab_seen "${_tracklab_seen}" PARENT_SCOPE)
    return()
  endif()
  get_target_property(_i "${tgt}" INTERFACE_INCLUDE_DIRECTORIES)
  get_target_property(_d "${tgt}" INTERFACE_COMPILE_DEFINITIONS)
  if(_i)
    list(APPEND _tracklab_incs ${_i})
  endif()
  if(_d)
    list(APPEND _tracklab_defs ${_d})
  endif()
  get_target_property(_l "${tgt}" INTERFACE_LINK_LIBRARIES)
  foreach(dep IN LISTS _l)
    if(dep MATCHES "^\\$<LINK_ONLY:(.*)>$")
      continue()
    endif()
    _tracklab_collect("${dep}")
  endforeach()
  set(_tracklab_seen "${_tracklab_seen}" PARENT_SCOPE)
  set(_tracklab_incs "${_tracklab_incs}" PARENT_SCOPE)
  set(_tracklab_defs "${_tracklab_defs}" PARENT_SCOPE)
endfunction()

foreach(root IN ITEMS
    tracktion::tracktion_engine tracktion::tracktion_graph tracktion::tracktion_core
    juce::juce_audio_utils juce::juce_audio_devices juce::juce_audio_formats
    juce::juce_audio_processors juce::juce_dsp)
  _tracklab_collect("${root}")
endforeach()
list(REMOVE_DUPLICATES _tracklab_incs)
list(REMOVE_DUPLICATES _tracklab_defs)

add_library(tracklab_juce_tracktion_api INTERFACE)
target_link_libraries(tracklab_juce_tracktion_api INTERFACE tracklab_defs tracklab_juce_tracktion)
target_include_directories(tracklab_juce_tracktion_api SYSTEM INTERFACE ${_tracklab_incs})
target_compile_definitions(tracklab_juce_tracktion_api INTERFACE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 ${_tracklab_defs})

# --- Warning flags for our own sources (src/, tests/) -----------------------------------------
# Never applied to JUCE/Tracktion module code: link tracklab_warnings only from our own targets.
option(TRACKLAB_WERROR "Treat warnings in src/ as errors" ON)
add_library(tracklab_warnings INTERFACE)
target_link_libraries(tracklab_warnings INTERFACE juce::juce_recommended_warning_flags)
if(TRACKLAB_WERROR)
  if(MSVC)
    target_compile_options(tracklab_warnings INTERFACE /W4 /WX /external:W0)
  else()
    target_compile_options(tracklab_warnings INTERFACE -Wall -Wextra -Werror)
    # Clang >= 20 verifies [[clang::nonblocking]] functions at compile time (docs/realtime.md).
    target_compile_options(tracklab_warnings INTERFACE
      "$<$<AND:$<CXX_COMPILER_ID:Clang>,$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,20>>:-Wfunction-effects>")
  endif()
endif()

# --- JSON + schema validation (M1-02, DESIGN Rev 3 section 3) ------------------------------------
# nlohmann/json v3.12.0 and pboettch/json-schema-validator 2.4.0 (both MIT, JSON Schema draft 7), git submodules
# with fixed tags. Used by src/core (command registry). Their headers are SYSTEM includes so that -Werror only
# applies to our own code; the validator's own sources keep the warning flags of its CMakeLists (no -Werror).
set(TRACKLAB_NLOHMANN_JSON_DIR "${TRACKLAB_THIRD_PARTY_DIR}/nlohmann_json")
set(TRACKLAB_JSON_VALIDATOR_DIR "${TRACKLAB_THIRD_PARTY_DIR}/json-schema-validator")
foreach(dir IN ITEMS "${TRACKLAB_NLOHMANN_JSON_DIR}" "${TRACKLAB_JSON_VALIDATOR_DIR}")
  if(NOT EXISTS "${dir}/CMakeLists.txt")
    message(FATAL_ERROR
      "Submodule missing: ${dir}\n"
      "Run: git submodule update --init --depth 1 third_party/nlohmann_json third_party/json-schema-validator")
  endif()
endforeach()

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
set(JSON_SystemInclude ON CACHE BOOL "" FORCE)
set(JSON_VALIDATOR_INSTALL OFF CACHE BOOL "" FORCE)
set(JSON_VALIDATOR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(JSON_VALIDATOR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JSON_VALIDATOR_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory("${TRACKLAB_NLOHMANN_JSON_DIR}" "${CMAKE_BINARY_DIR}/_deps/nlohmann_json" EXCLUDE_FROM_ALL)
add_subdirectory("${TRACKLAB_JSON_VALIDATOR_DIR}" "${CMAKE_BINARY_DIR}/_deps/json-schema-validator" EXCLUDE_FROM_ALL)
set_target_properties(nlohmann_json_schema_validator PROPERTIES SYSTEM TRUE)

# One target for our code: `#include <nlohmann/json.hpp>` and `#include <nlohmann/json-schema.hpp>`.
add_library(tracklab_json INTERFACE)
target_link_libraries(tracklab_json INTERFACE nlohmann_json::nlohmann_json nlohmann_json_schema_validator)
