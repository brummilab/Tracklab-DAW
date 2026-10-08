// First real commands (M1-02): namespace "app".
#pragma once

#include "core/command_registry.h"

#include <string>

namespace tracklab::core
{

/** Registers `app.version` (readOnly): no params, result {"version": <appVersion>}. Returns the first failure
    (e.g. duplicate_id if called twice on one registry). */
[[nodiscard]] RegisterResult registerAppCommands(CommandRegistry& registry, std::string appVersion);

}  // namespace tracklab::core
