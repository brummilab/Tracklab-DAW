cmake_minimum_required(VERSION 3.25)

# Root build of Tracklab (M0-07). Until M1 adds the real application under src/, the C++ project that the gate
# builds and tests is the engine spike. Build through CMakePresets.json (or scripts/gate.sh / gate.ps1).

# MSVC: embed debug info in the object files (/Z7) so that sccache can cache MSVC builds (CMP0141),
# static runtime so that the executables and the VST3 plugin need no redistributable. Both variables have to be
# set before project().
if(POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
endif()
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(Tracklab VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Use ccache automatically if present and no launcher was requested (CI passes sccache/ccache itself).
find_program(TRACKLAB_CCACHE ccache)
if(TRACKLAB_CCACHE AND NOT CMAKE_CXX_COMPILER_LAUNCHER AND NOT MSVC)
  set(CMAKE_C_COMPILER_LAUNCHER "${TRACKLAB_CCACHE}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${TRACKLAB_CCACHE}")
endif()

# RealtimeSanitizer (Clang >= 20, Linux only, docs/realtime.md). It instruments every function marked
# [[clang::nonblocking]] and intercepts allocation/lock/IO calls made from inside such a function at run time.
# The runtime has to be linked into every executable; instrumenting all compilation units is harmless because only
# annotated functions are affected. Third-party findings are filtered by scripts/rtsan.supp at run time.
option(TRACKLAB_RTSAN "Build with -fsanitize=realtime (Clang >= 20, Linux)" OFF)
if(TRACKLAB_RTSAN)
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 20)
    message(FATAL_ERROR "TRACKLAB_RTSAN needs Clang >= 20 (found ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")
  endif()
  add_compile_options(-fsanitize=realtime -fno-omit-frame-pointer)
  add_link_options(-fsanitize=realtime)
endif()

include(CTest)

add_subdirectory(spike/engine)
