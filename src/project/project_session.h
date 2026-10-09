// The open project (M1-04): owns the Tracktion Edit, creates / opens / saves / closes it and keeps the EditContext of the
// command registry (core/edit_context.h) pointed at it. Only `Edit` is used, never Tracktion's Project/ProjectManager and
// never EditFileOperations::save (DESIGN Rev 3 section 3, team/research/m1-fundament/NOTIZEN.md sections 1 and 3).
//
// All functions run on the JUCE message thread. Failures are thrown as core::CommandFailure with the codes of
// project_format.h (error_code::...) or core::error_code::noEdit, so that the project commands (project_commands.h)
// can hand them to the registry 1:1. A failed call leaves the session as it was: the open project stays open and
// unchanged (also its undo history), files on disk stay as they were.
#pragma once

#include "core/command.h"
#include "core/edit_context.h"
#include "project/project_format.h"

#include <juce_core/juce_core.h>
#include <tracktion_engine/tracktion_engine.h>

#include <functional>
#include <memory>
#include <string>

namespace tracklab::project
{

namespace te = tracktion;

/** What the commands report about the open project. For a short time after create / open / save, `modified` is decided
    by comparing the content with what was written or read, because Tracktion's own flag is unreliable then (see
    ProjectSession::Impl::SettleTimer in project_session.cpp). */
struct ProjectInfo
{
    juce::File file;        ///< the project file `<folder>/<name>/<name>.tracklab`, absolute
    std::string name;       ///< `<name>`: the project file's name without the extension
    int formatVersion = 0;  ///< format version of the state in memory (always currentFormatVersion once opened)
    bool modified = false;  ///< Edit::hasChangedSinceSaved(): changes since the last save / since opening
};

class ProjectSession
{
public:
    /** `engine` and `context` have to outlive the session. At most one project is open at a time. The session sets
        context.setEdit() to the open Edit (nullptr while none is open). The destructor closes without saving (the Edit
        is destroyed before the engine, the context is reset to nullptr). */
    ProjectSession(te::Engine& engine, core::EditContext& context);
    ~ProjectSession();
    ProjectSession(const ProjectSession&) = delete;
    ProjectSession& operator=(const ProjectSession&) = delete;

    /** The open Edit, nullptr if there is none. */
    te::Edit* edit() const noexcept;

    /** Test hook of the atomic save (never set in production). Called by every save after the temporary file next to
        the project file is completely written and flushed and BEFORE it replaces the project file. `temporary` is that
        file (same folder as `target`, a different name), `target` the project file. Returning false, or throwing, aborts
        the save: the project file is untouched, the temporary file is removed, the save fails with save_failed and
        the project stays `modified`. An empty function = no hook. */
    using BeforeReplaceHook = std::function<bool(const juce::File& temporary, const juce::File& target)>;
    void setBeforeReplaceHook(BeforeReplaceHook hook);

    //==========================================================================
    // Operations behind the project.* commands (see project_commands.h for the command side).

    /** New empty project (template "empty": Edit::createEdit with an empty state: no audio track, master at 0 dB;
        undo depth core::defaultUndoLevels = 200) in `<parentFolder>/<name>/`: creates the folder, `Audio/`, `Renders/`,
        `Backups/`, `Peaks/`, and writes
        `<name>.tracklab` right away (so the project is a valid file from the start; modified = false, empty undo
        history). The new project replaces the open one.
        - a project is open and modified -> unsaved_changes (nothing is created);
        - `name` empty, ".", "..", containing one of `<>:"/\|?*` or a control character, with blanks at the ends or a
          dot at the end, or a reserved Windows device name (CON, PRN, AUX, NUL, COM1-9, LPT1-9, any case, also with
          an extension) -> invalid_project_name (nothing is created); `#@,;&` and umlauts are fine;
        - `parentFolder` is not an existing folder -> folder_not_found (it is never created);
        - `<name>.tracklab` exists in the target folder -> project_exists (never overwritten). */
    ProjectInfo createProject(const juce::File& parentFolder, const juce::String& name);

    /** Opens the project file. Reading never writes anything to disk (also not for a migrated old file: that is only
        written by the next save). The state is migrated in memory to currentFormatVersion (migrateState with
        builtInMigrationSteps) before the Edit is created; the Edit gets editFileRetriever (so that relative media paths
        resolve against the project file), alwaysUseRelativePaths and the undo depth core::defaultUndoLevels; its
        undo history is empty, modified = false. Replaces the open project.
        - a project is open and modified -> unsaved_changes;
        - file missing -> project_not_found;
        - file empty, not XML, truncated, or the root element is not EDIT -> corrupt_project;
        - format version newer than currentFormatVersion -> project_too_new (message names the version);
        - a migration step fails -> migration_failed. */
    ProjectInfo openProject(const juce::File& file);

    /** Saves the open project to its file, atomically: Edit::flushState(), the XML is written to a juce::TemporaryFile
        in the folder of the project file, flushed, and only then replaces the project file
        (overwriteTargetFileWithTemporary / rename); then Edit::resetChangedStatus(). The attribute
        tracklabFormatVersion is currentFormatVersion. Always writes, also when the project is not modified.
        - none open -> no_edit;
        - Edit::isSaveInhibited() -> save_inhibited, nothing is written;
        - writing, flushing or replacing fails, or the BeforeReplaceHook refuses -> save_failed, the project file is
          byte-identical to before, no temporary file stays behind, modified is unchanged. */
    ProjectInfo save();

    /** Writes the open project as a new project `<parentFolder>/<name>/<name>.tracklab` (same rules for the name and the
        folders as createProject; atomically like save) and makes it the open project's file. The previous project file
        is not touched. Errors: no_edit, invalid_project_name, folder_not_found, project_exists, save_inhibited,
        save_failed.

        Media are not copied. Every stored media path (clip sources) is re-written relative to the new project file, so
        that media outside the new project folder keep being found; nothing of this goes through the undo manager. A
        failed save_as puts the paths and the file back. */
    ProjectInfo saveAs(const juce::File& parentFolder, const juce::String& name);

    /** Closes the open project (the context's Edit becomes nullptr). With `discard` = false and unsaved changes:
        unsaved_changes (the project stays open). With `discard` = true the changes are dropped, nothing is written.
        Closing when none is open is allowed and does nothing. */
    void closeProject(bool discard);

    /** Info of the open project; no_edit if none is open. */
    ProjectInfo info() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

}  // namespace tracklab::project
