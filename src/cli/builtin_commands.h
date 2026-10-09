// The registry of the app (M1-07): ONE function that registers every built-in command (app.*, edit.*, io.*, project.*).
// tracklab-cli builds its registry with it, and so does the test that compares tools.json / docs/commands.md with the
// registry; a command that is registered here shows up in the CLI, in the exports and in the freshness check of the
// gate at once. There is no second list of commands.
#pragma once

#include "core/command_registry.h"
#include "core/edit_context.h"
#include "project/project_session.h"

#include <tracktion_engine/tracktion_engine.h>

#include <string>

namespace tracklab::cli
{

/** Registers all built-in commands on `registry`. The engine, the context and the session have to outlive the use of
    the registry (the handlers keep references to them). `appVersion` is the result of app.version; it is not part of
    the exports. Returns the first failure (e.g. duplicate_id if called twice on one registry).
    Does NOT call registry.setEditContext(): the caller decides which context the registry's undo handling uses. */
[[nodiscard]] core::RegisterResult registerBuiltInCommands(core::CommandRegistry& registry, tracktion::Engine& engine,
                                                           core::EditContext& context, project::ProjectSession& session,
                                                           std::string appVersion);

}  // namespace tracklab::cli
