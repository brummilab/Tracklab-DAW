#include "project/project_commands.h"

#include <string>
#include <utility>

namespace tracklab::project
{

namespace
{

using core::Command;
using core::Json;

const char* const noParamsSchema = R"({"type": "object", "properties": {}, "additionalProperties": false})";

const char* const infoSchema = R"({
    "type": "object",
    "properties": {
        "path": {"type": "string", "description": "Absolute path of the .tracklab project file."},
        "name": {"type": "string", "description": "Project name (the file name without the extension)."},
        "format_version": {"type": "integer", "description": "Version of the project file format."},
        "modified": {"type": "boolean", "description": "True if there are unsaved changes."}
    },
    "required": ["path", "name", "format_version", "modified"],
    "additionalProperties": false
})";

const char* const folderDescription =
    "Absolute path of the folder that holds the project folders (it must exist). The project lives in "
    "<folder>/<name>/<name>.tracklab.";

// project.open: Info, plus `recovery_available` when the autosave is newer than the project file (M1-05).
const char* const openResultSchema = R"({
    "type": "object",
    "properties": {
        "path": {"type": "string", "description": "Absolute path of the .tracklab project file."},
        "name": {"type": "string", "description": "Project name (the file name without the extension)."},
        "format_version": {"type": "integer", "description": "Version of the project file format."},
        "modified": {"type": "boolean", "description": "True if there are unsaved changes."},
        "recovery_available": {
            "type": "object",
            "description": "Only present if the autosave file is newer than the project file (the app probably crashed). The project was opened as the project file has it; decide with project.restore_autosave or project.discard_autosave.",
            "properties": {
                "project_time": {"type": "string", "description": "ISO 8601 time of the last change of the project file."},
                "autosave_time": {"type": "string", "description": "ISO 8601 time of the last change of the autosave file."}
            },
            "required": ["project_time", "autosave_time"],
            "additionalProperties": false
        }
    },
    "required": ["path", "name", "format_version", "modified"],
    "additionalProperties": false
})";

const char* const listBackupsResultSchema = R"({
    "type": "object",
    "properties": {
        "backups": {
            "type": "array",
            "description": "The backups of the open project, newest first.",
            "items": {
                "type": "object",
                "properties": {
                    "name": {"type": "string", "description": "File name in the Backups folder; pass it to project.restore_backup."},
                    "time": {"type": "string", "description": "ISO 8601 time the backup was made."},
                    "size_bytes": {"type": "integer"}
                },
                "required": ["name", "time", "size_bytes"],
                "additionalProperties": false
            }
        }
    },
    "required": ["backups"],
    "additionalProperties": false
})";

Json toJson(const ProjectInfo& info)
{
    return Json{{"path", info.file.getFullPathName().toStdString()},
                {"name", info.name},
                {"format_version", info.formatVersion},
                {"modified", info.modified}};
}

std::string isoTime(const juce::Time& time)
{
    return time.toISO8601(true).toStdString();
}

/** Paths come from outside (GUI dialog, Claude, MCP): only absolute ones, so that nothing depends on the working
    directory of the process. The folder release for Claude comes with M3. */
juce::File absoluteFile(const Json& params, const char* key)
{
    const auto text = juce::String::fromUTF8(params.at(key).get<std::string>().c_str());
    if (!juce::File::isAbsolutePath(text))
        throw core::CommandFailure(core::error_code::invalidParams,
                                   std::string("\"") + key + "\" must be an absolute path", std::string("/") + key);
    return juce::File(text);
}

juce::String projectName(const Json& params)
{
    return juce::String::fromUTF8(params.at("name").get<std::string>().c_str());
}

}  // namespace

core::RegisterResult registerProjectCommands(core::CommandRegistry& registry, ProjectSession& session)
{
    // Not captured by value: the session outlives the registry's use of the commands (see the header).
    Command create;
    create.id = "project.new";
    create.titleDe = "Neues Projekt";
    create.descriptionEn =
        "Creates a new project <folder>/<name>/<name>.tracklab with the sub folders Audio, Renders, Backups and Peaks "
        "and opens it. Fails with unsaved_changes if the open project has unsaved changes, with invalid_project_name "
        "(at most 120 bytes) or with path_too_long if the paths of the project would exceed 259 characters (use a "
        "shorter name or folder).";
    create.paramsSchema = Json::parse(std::string(R"({
        "type": "object",
        "properties": {
            "folder": {"type": "string", "description": ")") +
                                      folderDescription + R"("},
            "name": {"type": "string", "description": "Project name; a plain name without path separators."},
            "template": {"type": "string", "enum": ["empty"], "description": "Project template (default: empty)."}
        },
        "required": ["folder", "name"],
        "additionalProperties": false
    })");
    create.resultSchema = Json::parse(infoSchema);
    create.shortcut = "Ctrl+N";
    create.menuPath = "Datei";
    create.handler = [&session](const Json& params)
    { return toJson(session.createProject(absoluteFile(params, "folder"), projectName(params))); };
    if (auto outcome = registry.registerCommand(std::move(create)); !outcome.ok)
        return outcome;

    Command open;
    open.id = "project.open";
    open.titleDe = "Projekt \xC3\xB6"
                   "ffnen";
    open.descriptionEn =
        "Opens a .tracklab project file. Fails with unsaved_changes if the open project has unsaved changes, with "
        "project_not_found, corrupt_project (the message names the newest backup or autosave) or project_too_new if "
        "the "
        "file cannot be opened. If an autosave newer than the project file exists the result has recovery_available "
        "with both times: the project is opened as the file has it, decide with project.restore_autosave or "
        "project.discard_autosave.";
    open.paramsSchema = Json::parse(R"({
        "type": "object",
        "properties": {"path": {"type": "string", "description": "Absolute path of the .tracklab file."}},
        "required": ["path"],
        "additionalProperties": false
    })");
    open.resultSchema = Json::parse(openResultSchema);
    open.shortcut = "Ctrl+O";
    open.menuPath = "Datei";
    open.handler = [&session](const Json& params)
    {
        auto result = toJson(session.openProject(absoluteFile(params, "path")));
        if (const auto recovery = session.pendingRecovery())
            result["recovery_available"] = Json{{"project_time", isoTime(recovery->projectTime)},
                                                {"autosave_time", isoTime(recovery->autosaveTime)}};
        return result;
    };
    if (auto outcome = registry.registerCommand(std::move(open)); !outcome.ok)
        return outcome;

    Command save;
    save.id = "project.save";
    save.titleDe = "Projekt speichern";
    save.descriptionEn =
        "Saves the open project to its file (atomically: the old file stays intact if saving fails). Fails with "
        "no_edit if no project is open.";
    save.paramsSchema = Json::parse(noParamsSchema);
    save.resultSchema = Json::parse(infoSchema);
    save.shortcut = "Ctrl+S";
    save.menuPath = "Datei";
    save.handler = [&session](const Json&) { return toJson(session.save()); };
    if (auto outcome = registry.registerCommand(std::move(save)); !outcome.ok)
        return outcome;

    Command saveAs;
    saveAs.id = "project.save_as";
    saveAs.titleDe = "Projekt speichern unter";
    saveAs.descriptionEn =
        "Saves the open project as a new project <folder>/<name>/<name>.tracklab and continues in it. Media files "
        "are not copied. Never overwrites an existing project. Fails with path_too_long if the paths of the new "
        "project would exceed 259 characters.";
    saveAs.paramsSchema = Json::parse(std::string(R"({
        "type": "object",
        "properties": {
            "folder": {"type": "string", "description": ")") +
                                      folderDescription + R"("},
            "name": {"type": "string", "description": "Project name; a plain name without path separators."}
        },
        "required": ["folder", "name"],
        "additionalProperties": false
    })");
    saveAs.resultSchema = Json::parse(infoSchema);
    saveAs.shortcut = "Ctrl+Shift+S";
    saveAs.menuPath = "Datei";
    saveAs.handler = [&session](const Json& params)
    { return toJson(session.saveAs(absoluteFile(params, "folder"), projectName(params))); };
    if (auto outcome = registry.registerCommand(std::move(saveAs)); !outcome.ok)
        return outcome;

    Command close;
    close.id = "project.close";
    close.titleDe = "Projekt schlie\xC3\x9F"
                    "en";
    close.descriptionEn =
        "Closes the open project. Fails with unsaved_changes if there are unsaved changes, unless discard is true, "
        "which throws them away. Does nothing if no project is open.";
    close.paramsSchema = Json::parse(R"({
        "type": "object",
        "properties": {"discard": {"type": "boolean", "description": "Throw away unsaved changes (default: false)."}},
        "additionalProperties": false
    })");
    close.resultSchema = Json::parse(R"({
        "type": "object",
        "properties": {"closed": {"type": "boolean"}},
        "required": ["closed"],
        "additionalProperties": false
    })");
    close.flags.destructive = true;
    close.shortcut = "Ctrl+W";
    close.menuPath = "Datei";
    close.handler = [&session](const Json& params)
    {
        session.closeProject(params.value("discard", false));
        return Json{{"closed", true}};
    };
    if (auto outcome = registry.registerCommand(std::move(close)); !outcome.ok)
        return outcome;

    Command getInfo;
    getInfo.id = "project.get_info";
    getInfo.titleDe = "Projektinfo abfragen";
    getInfo.descriptionEn =
        "Returns path, name, format version and whether there are unsaved changes of the open project. Fails with "
        "no_edit if no project is open.";
    getInfo.paramsSchema = Json::parse(noParamsSchema);
    getInfo.resultSchema = Json::parse(infoSchema);
    getInfo.flags.readOnly = true;
    getInfo.handler = [&session](const Json&) { return toJson(session.info()); };
    if (auto outcome = registry.registerCommand(std::move(getInfo)); !outcome.ok)
        return outcome;

    //==========================================================================
    // M1-05: backups and recovery
    Command listBackups;
    listBackups.id = "project.list_backups";
    listBackups.titleDe = "Backups anzeigen";
    listBackups.descriptionEn =
        "Lists the rotating backups of the open project (one per save, the oldest are deleted), newest first. Fails "
        "with no_edit if no project is open.";
    listBackups.paramsSchema = Json::parse(noParamsSchema);
    listBackups.resultSchema = Json::parse(listBackupsResultSchema);
    listBackups.flags.readOnly = true;
    listBackups.handler = [&session](const Json&)
    {
        if (session.edit() == nullptr)
            throw core::CommandFailure(core::error_code::noEdit, "no project is open");
        auto backups = Json::array();
        for (const auto& backup : session.listBackups())
            backups.push_back(Json{
                {"name", backup.name.toStdString()}, {"time", isoTime(backup.time)}, {"size_bytes", backup.sizeBytes}});
        return Json{{"backups", std::move(backups)}};
    };
    if (auto outcome = registry.registerCommand(std::move(listBackups)); !outcome.ok)
        return outcome;

    Command restoreBackup;
    restoreBackup.id = "project.restore_backup";
    restoreBackup.titleDe = "Backup wiederherstellen";
    restoreBackup.descriptionEn =
        "Loads a backup of the open project as its current state, replacing the state in memory including unsaved "
        "changes. Makes a backup of the current state first (the chosen backup is never rotated away). The project "
        "file is only replaced by the next save. Fails with no_edit, invalid_params (name is not a plain file name), "
        "backup_not_found, or corrupt_project (the backup cannot be read; nothing changes).";
    restoreBackup.paramsSchema = Json::parse(R"({
        "type": "object",
        "properties": {"name": {"type": "string", "description": "Backup file name from project.list_backups."}},
        "required": ["name"],
        "additionalProperties": false
    })");
    restoreBackup.resultSchema = Json::parse(infoSchema);
    restoreBackup.flags.destructive = true;
    restoreBackup.menuPath = "Datei";
    restoreBackup.handler = [&session](const Json& params)
    { return toJson(session.restoreBackup(projectName(params))); };
    if (auto outcome = registry.registerCommand(std::move(restoreBackup)); !outcome.ok)
        return outcome;

    Command restoreAutosave;
    restoreAutosave.id = "project.restore_autosave";
    restoreAutosave.titleDe = "Autosave wiederherstellen";
    restoreAutosave.descriptionEn =
        "Loads the autosave file of the open project as its current state (recovery after a crash), replacing the "
        "state in memory. The project file is only replaced by the next save, which also deletes the autosave. Fails "
        "with no_autosave or no_edit.";
    restoreAutosave.paramsSchema = Json::parse(noParamsSchema);
    restoreAutosave.resultSchema = Json::parse(infoSchema);
    restoreAutosave.flags.destructive = true;
    restoreAutosave.handler = [&session](const Json&) { return toJson(session.restoreAutosave()); };
    if (auto outcome = registry.registerCommand(std::move(restoreAutosave)); !outcome.ok)
        return outcome;

    Command discardAutosave;
    discardAutosave.id = "project.discard_autosave";
    discardAutosave.titleDe = "Autosave verwerfen";
    discardAutosave.descriptionEn =
        "Deletes the autosave file of the open project and ends an offered recovery; the project in memory stays as "
        "it is. Fails with no_autosave or no_edit.";
    discardAutosave.paramsSchema = Json::parse(noParamsSchema);
    discardAutosave.resultSchema = Json::parse(R"({
        "type": "object",
        "properties": {"discarded": {"type": "boolean"}},
        "required": ["discarded"],
        "additionalProperties": false
    })");
    discardAutosave.flags.destructive = true;
    discardAutosave.handler = [&session](const Json&)
    {
        session.discardAutosave();
        return Json{{"discarded", true}};
    };
    return registry.registerCommand(std::move(discardAutosave));
}

}  // namespace tracklab::project
