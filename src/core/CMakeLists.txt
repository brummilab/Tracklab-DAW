# Module "core": command registry (M1-02). JSON handling comes from tracklab_json (nlohmann/json + schema validator,
# cmake/TracklabDeps.cmake). The message-thread check of CommandRegistry::execute uses JUCE (juce_events), which is
# compiled once in tracklab_juce_tracktion.
file(GLOB TRACKLAB_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(tracklab_core STATIC ${TRACKLAB_CORE_SOURCES})
target_include_directories(tracklab_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(tracklab_core
  PUBLIC tracklab_json
  PRIVATE tracklab_juce_tracktion_api tracklab_warnings tracklab_config_flags)
