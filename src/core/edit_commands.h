// Commands of the undo history (M1-03): namespace "edit".
#pragma once

#include "core/command_registry.h"
#include "core/edit_context.h"

namespace tracklab::core
{

/** Registers, all working on `context.edit()` (the context has to outlive the registry):
    - `edit.undo`  (no flags: it must not open a transaction itself): no params, result
      {"done": bool, "description": string}. Rolls back the last undo step. Without history: ok, done=false,
      description "" and nothing changes (no error: a shortcut on an empty history is not a mistake).
      description = name of the undone step.
    - `edit.redo`  (no flags): same for the last undone step.
    - `edit.get_undo_state` (readOnly): no params, result {"can_undo": bool, "can_redo": bool,
      "undo_description": string, "redo_description": string}; the descriptions are the names of the next undo/redo
      step ("" if there is none).
    All three: error no_edit if the context has no Edit. Returns the first failure
    (e.g. duplicate_id if called twice on one registry). */
[[nodiscard]] RegisterResult registerEditCommands(CommandRegistry& registry, EditContext& context);

}  // namespace tracklab::core
