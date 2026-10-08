# Module "engine": Tracktion Engine factory (createEngine). JUCE/Tracktion are compiled once in
# tracklab_juce_tracktion (cmake/TracklabDeps.cmake); this library only adds our own sources.
file(GLOB TRACKLAB_ENGINE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(tracklab_engine STATIC ${TRACKLAB_ENGINE_SOURCES})
target_include_directories(tracklab_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_definitions(tracklab_engine PRIVATE TRACKLAB_VERSION_STRING="${PROJECT_VERSION}")
target_link_libraries(tracklab_engine
  PUBLIC tracklab_core tracklab_juce_tracktion_api
  PRIVATE tracklab_warnings tracklab_config_flags)
