# CTest wrapper for the RTSan negative test (spike.rtsan_negative).
#
# Runs TEST_EXE with TEST_ARGS and passes only if RealtimeSanitizer stopped the process: non-zero exit code AND an
# "unsafe-library-call" report whose stack contains the deliberately allocating callback. Usage:
#   cmake -DTEST_EXE=<exe> "-DTEST_ARGS=<arg>;<arg>" -P rtsan_expect_failure.cmake
execute_process(
  COMMAND "${TEST_EXE}" ${TEST_ARGS}
  RESULT_VARIABLE exit_code
  OUTPUT_VARIABLE output
  ERROR_VARIABLE output
  ECHO_OUTPUT_VARIABLE
  ECHO_ERROR_VARIABLE)

if(exit_code EQUAL 0)
  message(FATAL_ERROR "RTSan negative test: the allocating callback ran without a finding (exit code 0). "
                      "RealtimeSanitizer is not active or the callback is suppressed.")
endif()
if(NOT output MATCHES "RealtimeSanitizer: unsafe-library-call")
  message(FATAL_ERROR "RTSan negative test: exit code ${exit_code}, but no RealtimeSanitizer finding in the output.")
endif()
if(NOT output MATCHES "allocatingCallback")
  message(FATAL_ERROR "RTSan negative test: finding does not point at the test callback (allocatingCallback).")
endif()
message(STATUS "RTSan negative test OK: the allocating callback was stopped by RealtimeSanitizer (exit code ${exit_code}).")
