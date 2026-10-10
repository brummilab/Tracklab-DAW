# Module "cli" (M1-07): the library tracklab_cli (runCli, testable in-process) and the executable tracklab-cli
# (src/cli/main.cpp, a thin main). Dependencies: engine, project, io, core (DESIGN section 3).
file(GLOB TRACKLAB_CLI_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(FILTER TRACKLAB_CLI_SOURCES EXCLUDE REGEX "/main\\.cpp$")
add_library(tracklab_cli STATIC ${TRACKLAB_CLI_SOURCES})
target_include_directories(tracklab_cli PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
# Result of app.version and of `tracklab-cli --version`: the CMake project version.
target_compile_definitions(tracklab_cli PRIVATE TRACKLAB_VERSION_STRING="${PROJECT_VERSION}")
target_link_libraries(tracklab_cli
  PUBLIC tracklab_core tracklab_engine tracklab_project tracklab_io tracklab_juce_tracktion_api
  PRIVATE tracklab_warnings tracklab_config_flags)

add_executable(tracklab-cli main.cpp)
target_link_libraries(tracklab-cli PRIVATE tracklab_cli tracklab_warnings tracklab_config_flags)
if(WIN32)
  target_link_libraries(tracklab-cli PRIVATE shell32)  # CommandLineToArgvW (UTF-8 arguments, main.cpp)
endif()
