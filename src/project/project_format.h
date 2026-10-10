// Project format of Tracklab (M1-04, DESIGN Rev 3 section 3 "Projektformat", E40/E41): file layout, format version,
// error codes and the migration framework. No engine, no file access: this header is about the data only.
//
// A project is a folder `<folder>/<name>/` with the project file `<name>.tracklab` and the sub folders
// `Audio/`, `Renders/`, `Backups/`, `Peaks/`. The project file is the XML form of the state tree of a Tracktion Edit
// (root element EDIT), written by ProjectSession (project_session.h). The root element carries the attribute
// `tracklabFormatVersion`. Paths of media files are stored relative to the project file.
#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <array>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace tracklab::project
{

/** Format version this build writes and reads without migration. v0 = a file without the attribute. */
inline constexpr int currentFormatVersion = 1;

/** Attribute (ValueTree property) of the EDIT node that holds the format version (an integer). */
inline constexpr const char* formatVersionProperty = "tracklabFormatVersion";

/** Extension of the project file, with the dot. */
inline constexpr const char* fileExtension = ".tracklab";

/** M1-05 defaults (E41): autosave every 2 minutes, 10 rotating backups. */
inline constexpr int defaultAutosaveIntervalMs = 120000;
inline constexpr int defaultMaxBackups = 10;

/** Longest path (in UTF-16 code units, what MAX_PATH counts without the terminating NUL) that a project may need. */
inline constexpr int maxPathLength = 259;

/** Highest counter of a backup name that was made in the same second as another: `<name>.<stamp>-99.tracklab`. */
inline constexpr int maxBackupCounter = 99;

/** The longest path of any file that belongs to a project `<parent>/<name>/` (all lengths in UTF-16 code units): the
    temporary file of a backup with the highest counter,
    `<parent>/<name>/Backups/<name>.<YYYYMMDD-HHMMSS>-99_temp<8 hex>.tracklab`. The project file, the autosave and their
    temporary files are all shorter. */
inline constexpr std::size_t longestDerivedPathLength(std::size_t parentLength, std::size_t nameLength)
{
    constexpr std::size_t separators = 1 + 1 + 1;  // before <name>, before Backups, before the backup
    constexpr std::size_t backups = 7;             // "Backups"
    constexpr std::size_t dotAndStamp = 1 + 15;    // ".YYYYMMDD-HHMMSS"
    constexpr std::size_t counter = 3;             // "-99"
    constexpr std::size_t tempSuffix = 5 + 8;      // "_temp" + 8 hex digits
    constexpr std::size_t extension = 9;           // ".tracklab"
    return parentLength + separators + nameLength + backups + nameLength + dotAndStamp + counter + tempSuffix +
           extension;
}

/** Sub folders of a project folder, created by project.new / project.save_as. */
inline constexpr std::array<const char*, 4> subFolderNames = {"Audio", "Renders", "Backups", "Peaks"};

/** Stable machine-readable codes of CommandFailure / CommandError that the project commands report (besides the
    generic ones of core::error_code, e.g. no_edit, invalid_params). */
namespace error_code
{
inline constexpr std::string_view unsavedChanges = "unsaved_changes";  ///< open/new/close would drop unsaved changes
inline constexpr std::string_view corruptProject =
    "corrupt_project";  ///< the file exists but cannot be read as a project
inline constexpr std::string_view projectTooNew =
    "project_too_new";  ///< format version newer than currentFormatVersion
inline constexpr std::string_view projectNotFound = "project_not_found";  ///< the project file does not exist
inline constexpr std::string_view projectExists = "project_exists";       ///< new/save_as would overwrite a project
inline constexpr std::string_view invalidProjectName = "invalid_project_name";  ///< name is empty or not a plain name
inline constexpr std::string_view folderNotFound =
    "folder_not_found";  ///< new/save_as: the parent folder does not exist (it is never created)
inline constexpr std::string_view saveFailed = "save_failed";  ///< writing the project file failed, old file intact
inline constexpr std::string_view saveInhibited = "save_inhibited";  ///< Edit::isSaveInhibited(): nothing was written
inline constexpr std::string_view migrationFailed = "migration_failed";  ///< a migration step failed
inline constexpr std::string_view pathTooLong =
    "path_too_long";  ///< new/save_as: a file of the project would not fit into Windows' 259 character paths
inline constexpr std::string_view backupNotFound =
    "backup_not_found";  ///< project.restore_backup: no backup of that name in Backups/
inline constexpr std::string_view noAutosave =
    "no_autosave";  ///< project.restore_autosave: there is no autosave file of the open project
}  // namespace error_code

//==============================================================================
// Migration: a list of steps vN -> vN+1 on the state tree, run before the Edit is created.

/** One step: turns a state of version `fromVersion` into one of version `fromVersion + 1`. The framework sets the
    version attribute after the step, the step itself does not have to. */
struct MigrationStep
{
    int fromVersion = 0;
    std::function<void(juce::ValueTree& edit)> apply;  ///< `edit` is the EDIT node; may throw (migration_failed)
};

struct MigrationOutcome
{
    bool ok = false;
    std::string code;     ///< !ok: error_code::projectTooNew, error_code::migrationFailed or error_code::corruptProject
    std::string message;  ///< !ok: for people; names the versions involved
    int fromVersion = 0;  ///< version found in the state
    int toVersion = 0;    ///< version of the state afterwards (= fromVersion if nothing was done or !ok)
};

/** Version of an EDIT node: the integer attribute `tracklabFormatVersion`, 0 if it is missing. */
int formatVersionOf(const juce::ValueTree& edit);

/** The migration steps of the format, oldest first. Today: v0 -> v1 (a file without the attribute gets version 1; its
    content is kept as it is). */
const std::vector<MigrationStep>& builtInMigrationSteps();

/** Brings `edit` to `targetVersion`: runs, in order, the step of every version from formatVersionOf(edit) up to
    targetVersion - 1 (each exactly once; none if the state is already at targetVersion) and sets the version attribute
    to targetVersion.
    - state newer than targetVersion -> !ok, code project_too_new, message contains the found version number;
    - a missing step for a version in between -> !ok, code migration_failed;
    - a step that throws -> !ok, code migration_failed.
    All or nothing: when !ok, `edit` is exactly as before (no step is visible). */
MigrationOutcome migrateState(juce::ValueTree& edit, const std::vector<MigrationStep>& steps, int targetVersion);

}  // namespace tracklab::project
