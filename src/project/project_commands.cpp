#include "project/project_commands.h"

// STUB (test-writer, M1-04): registers nothing yet, the implementer replaces this file.
namespace tracklab::project
{

core::RegisterResult registerProjectCommands(core::CommandRegistry&, ProjectSession&)
{
    return core::RegisterResult{.ok = true, .error = {}};
}

}  // namespace tracklab::project
