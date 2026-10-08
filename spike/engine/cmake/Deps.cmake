# Dependencies of the engine spike: JUCE (first!) and Tracktion Engine, both as git submodules.
#
# Pins (see docs/adr/ADR-001-tech-stack.md):
#   JUCE              tag 9.0.3   (be29c81)   fallback 8.0.15 (91ad83a)
#   Tracktion Engine  develop     (bb38617, 3.5.0)   fallback v3.2.0 (0a5f4e6)
# Tracktion's own nested submodule modules/juce is NOT initialised: Tracktion uses the JUCE of the
# parent project when the target juce::juce_core already exists.

set(SPIKE_THIRD_PARTY_DIR "${CMAKE_CURRENT_LIST_DIR}/../third_party")
set(SPIKE_JUCE_DIR "${SPIKE_THIRD_PARTY_DIR}/JUCE")
set(SPIKE_TRACKTION_DIR "${SPIKE_THIRD_PARTY_DIR}/tracktion_engine")

foreach(dir IN ITEMS "${SPIKE_JUCE_DIR}" "${SPIKE_TRACKTION_DIR}")
  if(NOT EXISTS "${dir}/CMakeLists.txt")
    message(FATAL_ERROR
      "Submodule missing: ${dir}\n"
      "Run: git submodule update --init spike/engine/third_party/JUCE spike/engine/third_party/tracktion_engine\n"
      "(do not use --recursive: Tracktion's nested JUCE submodule is not needed)")
  endif()
endforeach()

set(TE_ADD_EXAMPLES OFF CACHE BOOL "Tracktion examples are not built in the spike" FORCE)

# JUCE first, then Tracktion (which then finds juce::juce_core and skips its own JUCE).
add_subdirectory("${SPIKE_JUCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/juce" EXCLUDE_FROM_ALL)
add_subdirectory("${SPIKE_TRACKTION_DIR}" "${CMAKE_BINARY_DIR}/_deps/tracktion" EXCLUDE_FROM_ALL)

# Definitions for ALL targets (Brief M0-06).
add_library(spike_defs INTERFACE)
target_compile_definitions(spike_defs INTERFACE
  JUCE_USE_MP3AUDIOFORMAT=1       # explicit: default differs between JUCE 8 (0) and 9 (1)
  JUCE_USE_WINDOWS_MEDIA_FORMAT=0 # same MP3 decoder on every platform
  JUCE_PLUGINHOST_VST3=1
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0
  JUCE_MODAL_LOOPS_PERMITTED=1
  JUCE_STRICT_REFCOUNTEDPOINTER=1
  TRACKTION_ENABLE_SINGLETONS=0
  TRACKTION_LOG_DEVICES=0)

# --- Module code is compiled exactly once ------------------------------------------------------
# JUCE modules are INTERFACE libraries whose .cpp files are added to EVERY target that links them.
# Linking them into spike_core, spike_cli and spike_tests would compile Tracktion three times.
# Therefore: spike_modules (static) links the modules PRIVATE and holds the only copy of the module
# code; spike_modules_api re-exports include directories and compile definitions (without sources).
add_library(spike_modules STATIC "${CMAKE_CURRENT_LIST_DIR}/spike_modules_anchor.cpp")
target_link_libraries(spike_modules PRIVATE
  spike_defs
  tracktion::tracktion_engine
  tracktion::tracktion_graph
  tracktion::tracktion_core
  juce::juce_audio_utils
  juce::juce_audio_devices
  juce::juce_audio_formats
  juce::juce_audio_processors
  juce::juce_dsp)
set_target_properties(spike_modules PROPERTIES
  C_VISIBILITY_PRESET hidden
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON)
target_compile_features(spike_modules PUBLIC cxx_std_20)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(spike_modules PUBLIC atomic)
endif()

# Collect usage requirements (include dirs, compile definitions) of the module closure.
set(_spike_seen "")
set(_spike_incs "")
set(_spike_defs "")
function(_spike_collect tgt)
  if(NOT TARGET "${tgt}")
    return()
  endif()
  get_target_property(_aliased "${tgt}" ALIASED_TARGET)
  if(_aliased)
    set(tgt "${_aliased}")
  endif()
  if("${tgt}" IN_LIST _spike_seen)
    return()
  endif()
  list(APPEND _spike_seen "${tgt}")
  get_target_property(_type "${tgt}" TYPE)
  if(NOT _type STREQUAL "INTERFACE_LIBRARY")
    set(_spike_seen "${_spike_seen}" PARENT_SCOPE)
    return()
  endif()
  get_target_property(_i "${tgt}" INTERFACE_INCLUDE_DIRECTORIES)
  get_target_property(_d "${tgt}" INTERFACE_COMPILE_DEFINITIONS)
  if(_i)
    list(APPEND _spike_incs ${_i})
  endif()
  if(_d)
    list(APPEND _spike_defs ${_d})
  endif()
  get_target_property(_l "${tgt}" INTERFACE_LINK_LIBRARIES)
  foreach(dep IN LISTS _l)
    if(dep MATCHES "^\\$<LINK_ONLY:(.*)>$")
      continue()
    endif()
    _spike_collect("${dep}")
  endforeach()
  set(_spike_seen "${_spike_seen}" PARENT_SCOPE)
  set(_spike_incs "${_spike_incs}" PARENT_SCOPE)
  set(_spike_defs "${_spike_defs}" PARENT_SCOPE)
endfunction()

foreach(root IN ITEMS
    tracktion::tracktion_engine tracktion::tracktion_graph tracktion::tracktion_core
    juce::juce_audio_utils juce::juce_audio_devices juce::juce_audio_formats
    juce::juce_audio_processors juce::juce_dsp)
  _spike_collect("${root}")
endforeach()
list(REMOVE_DUPLICATES _spike_incs)
list(REMOVE_DUPLICATES _spike_defs)

add_library(spike_modules_api INTERFACE)
target_link_libraries(spike_modules_api INTERFACE spike_defs spike_modules)
target_include_directories(spike_modules_api SYSTEM INTERFACE ${_spike_incs})
target_compile_definitions(spike_modules_api INTERFACE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 ${_spike_defs})
