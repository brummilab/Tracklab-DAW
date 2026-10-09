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

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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

/** What one autosave attempt did (M1-05). */
enum class AutosaveResult
{
    written,          ///< `<project>.tracklab.autosave` was (re)written
    notModified,      ///< nothing changed since the last save / autosave: nothing was written
    noProject,        ///< no project is open
    saveInhibited,    ///< Edit::isSaveInhibited(): nothing was written, the next attempt tries again
    recoveryPending,  ///< the project was opened with an offered recovery that is not decided yet: the old autosave file
                      ///< is not overwritten
    failed            ///< writing failed (the previous autosave file is intact); never throws
};

/** One file of `<project folder>/Backups/` that belongs to the open project: `<name>.<YYYYMMDD-HHMMSS>.tracklab`. */
struct BackupInfo
{
    juce::String name;  ///< the file name, e.g. "Muster.20261009-123456.tracklab"
    juce::Time time;    ///< the time in the name (local time of the clock the backup was made with)
    juce::int64 sizeBytes = 0;
};

/** The autosave file is newer than the project file of the project that was just opened. */
struct RecoveryInfo
{
    juce::Time projectTime;   ///< modification time of the project file
    juce::Time autosaveTime;  ///< modification time of the autosave file
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
    // M1-05: autosave, rotating backups, recovery (members, see below the operations).

    /** Where "now" comes from for the names of the backups (default: juce::Time::getCurrentTime()). Tests inject it. */
    using Clock = std::function<juce::Time()>;
    void setClock(Clock clock);

    /** Autosave interval (default defaultAutosaveIntervalMs); a message-thread timer calls autosaveNow() that often
        while a project is open. <= 0 switches the timer off. Changing it restarts the timer. */
    void setAutosaveInterval(std::chrono::milliseconds interval);
    std::chrono::milliseconds autosaveInterval() const;

    /** How many backups of the open project are kept (default defaultMaxBackups); older ones are deleted by the next
        save. */
    void setMaxBackups(int count);
    int maxBackups() const;

    /** What the autosave timer does every interval; public so that tests do not have to wait. Never throws. */
    AutosaveResult autosaveNow();

    /** The backups of the open project, newest first (empty if none is open). */
    std::vector<BackupInfo> listBackups() const;

    /** Set by openProject when the autosave was newer than the project file; cleared by restoreAutosave,
        discardAutosave and closing / opening another project. */
    std::optional<RecoveryInfo> pendingRecovery() const;

    /** Loads the autosave file as the state of the open project (the project file on disk is not touched, the autosave
        file stays until the next save; modified = true; undo history empty). no_autosave if there is none. */
    ProjectInfo restoreAutosave();

    /** Deletes the autosave file of the open project; the project in memory is not changed. */
    void discardAutosave();

    /** Makes a backup of the current state first, then loads backup `name` (a file name from listBackups) as the state
        of the open project (project file on disk not touched, modified = true, undo history empty). Errors: no_edit,
        backup_not_found (also for a name that is not a plain file name of the Backups folder). */
    ProjectInfo restoreBackup(const juce::String& name);

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
