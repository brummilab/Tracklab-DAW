#include "cli/builtin_commands.h"

#include "core/app_commands.h"
#include "core/edit_commands.h"
#include "io/audio_devices.h"
#include "project/project_commands.h"

#include <utility>

namespace tracklab::cli
{

core::RegisterResult registerBuiltInCommands(core::CommandRegistry& registry, tracktion::Engine& engine,
                                             core::EditContext& context, project::ProjectSession& session,
                                             std::string appVersion)
{
    // Stops at the first failure: a half-filled registry is never used (the caller reports the error).
    if (auto result = core::registerAppCommands(registry, std::move(appVersion)); !result.ok)
        return result;
    if (auto result = core::registerEditCommands(registry, context); !result.ok)
        return result;
    if (auto result = io::registerIoCommands(registry, engine); !result.ok)
        return result;
    return project::registerProjectCommands(registry, session);
}

}  // namespace tracklab::cli
