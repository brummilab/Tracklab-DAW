# Module "project": project format (M1-04): create / open / save / close of `.tracklab` projects on top of a Tracktion
# Edit, plus the project.* commands. JUCE/Tracktion are compiled once in tracklab_juce_tracktion
# (cmake/TracklabDeps.cmake); this library only adds our own sources.
file(GLOB TRACKLAB_PROJECT_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(tracklab_project STATIC ${TRACKLAB_PROJECT_SOURCES})
target_include_directories(tracklab_project PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(tracklab_project
  PUBLIC tracklab_core tracklab_juce_tracktion_api
  PRIVATE tracklab_warnings tracklab_config_flags)
