# Product code of Tracklab, one library per module (team/design/DESIGN.md section 3). Dependencies only point
# downwards: app -> cli -> engine/project/io -> core. Includes use the module folder:
#   #include "engine/engine_factory.h"
add_subdirectory(core)
add_subdirectory(io)
add_subdirectory(project)
add_subdirectory(engine)
add_subdirectory(cli)
add_subdirectory(app)
