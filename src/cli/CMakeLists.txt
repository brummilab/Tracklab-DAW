# Module "cli": placeholder until a later M1 card adds sources. An INTERFACE library keeps the dependency graph
# (DESIGN section 3) in place without an empty translation unit.
add_library(tracklab_cli INTERFACE)
target_include_directories(tracklab_cli INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/..")
