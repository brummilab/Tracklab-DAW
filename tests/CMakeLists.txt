# Tests of Tracklab: one executable (tracklab_tests), one doctest suite per module, one CTest entry per suite.
# doctest comes from Tracktion's modules/3rd_party (no new dependency). Exit code 77 = skipped, never passed.
file(GLOB_RECURSE TRACKLAB_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(tracklab_tests ${TRACKLAB_TEST_SOURCES})
target_include_directories(tracklab_tests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${TRACKLAB_TRACKTION_DIR}/modules/3rd_party/doctest")
# The version the tests expect from the engine is the CMake project version, independent of the engine sources.
target_compile_definitions(tracklab_tests PRIVATE
  TRACKLAB_EXPECTED_VERSION="${PROJECT_VERSION}"
  TRACKLAB_SOURCE_DIR="${CMAKE_SOURCE_DIR}"  # checked-in tools.json / docs/commands.md (tests/core), golden.wav (tests/cli)
  TRACKLAB_CLI_EXE="$<TARGET_FILE:tracklab-cli>")  # the real executable, started by a few tests in tests/cli
target_link_libraries(tracklab_tests PRIVATE tracklab_core tracklab_io tracklab_engine tracklab_project tracklab_cli
  tracklab_config_flags)
# The CLI tests start the executable: it has to be built before the tests run.
add_dependencies(tracklab_tests tracklab-cli)

# Suite name per module. engine_readonly needs a file system that enforces folder permissions (not as root):
# it is skipped (exit 77) otherwise, in its own CTest entry so that the other suites stay meaningful.
set(TRACKLAB_TEST_SUITES core io engine engine_readonly project cli cli_golden)
foreach(suite IN LISTS TRACKLAB_TEST_SUITES)
  add_test(NAME tracklab.${suite} COMMAND tracklab_tests --test-suite=${suite})
  set_tests_properties(tracklab.${suite} PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 300
    LABELS "tracklab;${suite}")
endforeach()

if(TRACKLAB_RTSAN)
  # Same options as the spike tests (spike/engine/CMakeLists.txt): every finding stops the test, third-party
  # findings are filtered by scripts/rtsan.supp.
  set(TRACKLAB_RTSAN_OPTIONS "halt_on_error=true:detect_leaks=0:suppressions=${CMAKE_SOURCE_DIR}/scripts/rtsan.supp")
  if(TRACKLAB_RTSAN_SYMBOLIZER)
    string(APPEND TRACKLAB_RTSAN_OPTIONS ":external_symbolizer_path=${TRACKLAB_RTSAN_SYMBOLIZER}")
  endif()
  foreach(suite IN LISTS TRACKLAB_TEST_SUITES)
    set_property(TEST tracklab.${suite} APPEND PROPERTY ENVIRONMENT "RTSAN_OPTIONS=${TRACKLAB_RTSAN_OPTIONS}")
    set_tests_properties(tracklab.${suite} PROPERTIES TIMEOUT 1500)
  endforeach()
endif()

# Golden update (M1-07): the ONLY way to change tests/fixtures/m1-mini/golden.wav. Needs a reason in the environment:
#   TRACKLAB_GOLDEN_REASON="why the render legitimately changed" cmake --build <dir> --target tracklab_update_golden
# The test case then renders the fixture, replaces golden.wav and appends the reason to
# tests/fixtures/m1-mini/golden-update.log. Never part of the gate or of "all".
add_custom_target(tracklab_update_golden
  COMMAND ${CMAKE_COMMAND} -E env TRACKLAB_UPDATE_GOLDEN=1 $<TARGET_FILE:tracklab_tests> --test-suite=cli_golden
  DEPENDS tracklab_tests tracklab-cli
  USES_TERMINAL
  COMMENT "Regenerating tests/fixtures/m1-mini/golden.wav (needs TRACKLAB_GOLDEN_REASON)")
