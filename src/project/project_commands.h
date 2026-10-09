// Commands of the project (M1-04): namespace "project". Thin layer over ProjectSession: GUI, shortcuts, Claude panel,
// MCP and CLI reach the project only through these commands.
#pragma once

#include "core/command_registry.h"
#include "project/project_session.h"

namespace tracklab::project
{

/** Registers the commands below (the session has to outlive the registry's use of them). Returns the first failure
    (e.g. duplicate_id if called twice on one registry). None of them is `undoable`: opening, saving and closing a
    project are not steps of the project's own undo history (and `project.new` / `project.open` have to work without an
    open project).

    Info = {"path": string (absolute, native path of the .tracklab file), "name": string,
            "format_version": integer, "modified": boolean}   all four required, no other keys.

    project.new       flags: none     params {"folder": string, "name": string, "template"?: "empty"}
                      `folder` is the PARENT folder; the project lives in `<folder>/<name>/<name>.tracklab`.
                      Result Info of the new project (modified false). Errors: unsaved_changes, invalid_project_name,
                      project_exists.
    project.open      flags: none     params {"path": string}  (the .tracklab file) -> Info.
                      Errors: unsaved_changes, project_not_found, corrupt_project, project_too_new, migration_failed.
                      No `discard` parameter: dropping unsaved changes is only possible through project.close.
    project.save      flags: none     params {} -> Info (modified false). Errors: no_edit, save_inhibited, save_failed.
    project.save_as   flags: none     params {"folder": string, "name": string} -> Info of the new file. Errors: no_edit,
                      invalid_project_name, project_exists, save_inhibited, save_failed.
    project.close     flags: destructive  params {"discard"?: boolean (default false)} -> {"closed": true}.
                      Error unsaved_changes if there are unsaved changes and discard is not true.
    project.get_info  flags: readOnly     params {} -> Info. Error no_edit if no project is open.

    Parameters are checked by the registry (additionalProperties:false: an unknown key is invalid_params). */
[[nodiscard]] core::RegisterResult registerProjectCommands(core::CommandRegistry& registry, ProjectSession& session);

}  // namespace tracklab::project
