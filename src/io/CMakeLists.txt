# Module "io": audio devices (M1-06): the commands io.list_device_types / io.list_devices / io.get_device /
# io.set_device on top of the juce::AudioDeviceManager of the Tracktion Engine. Backends are compiled in through the
# definitions of cmake/TracklabDeps.cmake (JUCE_ALSA/JUCE_JACK on Linux, JUCE_ASIO/JUCE_WASAPI on Windows).
# JUCE/Tracktion are compiled once in tracklab_juce_tracktion; this library only adds our own sources.
file(GLOB TRACKLAB_IO_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(tracklab_io STATIC ${TRACKLAB_IO_SOURCES})
target_include_directories(tracklab_io PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(tracklab_io
  PUBLIC tracklab_core tracklab_juce_tracktion_api
  PRIVATE tracklab_warnings tracklab_config_flags)
