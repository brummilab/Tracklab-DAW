# Module "app": placeholder until a later M1 card adds sources. An INTERFACE library keeps the dependency graph
# (DESIGN section 3) in place without an empty translation unit.
add_library(tracklab_app INTERFACE)
target_include_directories(tracklab_app INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/..")
