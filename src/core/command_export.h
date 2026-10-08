// Export of the registry (M1-02): tools.json for the Claude API / MCP and docs/commands.md for people.
// The CLI (`tracklab-cli export-tools`) comes with M1-07; these functions are what it will call. Output is
// deterministic: sorted by command id, no timestamps, no absolute paths, "\n" line ends, final newline.
#pragma once

#include "core/command_registry.h"

#include <filesystem>
#include <string>

namespace tracklab::core
{

/** tools.json: a JSON array (2-space indent, keys sorted) with one object per command, sorted by id:
    {"description": descriptionEn, "input_schema": paramsSchema, "name": toolName}. Empty registry: "[]\n". */
std::string exportToolsJson(const CommandRegistry& registry);

/** docs/commands.md (German text). Layout:
    - "# Command-Referenz", a note that the file is generated (`tracklab-cli export-tools`, do not edit by hand),
    - one section "## <namespace>" per namespace (text before the first "."), sorted, each with the table
        | ID | Tool-Name | Titel | Beschreibung | Flags | Shortcut | Menüpfad |
        |---|---|---|---|---|---|---|
        | `track.create` | `track_create` | <titleDe> | <descriptionEn> | undoable | Ctrl+T | Spur/Neu |
      rows sorted by id; Flags = names (readOnly, undoable, destructive, longRunning in this order) joined by ", ";
      an empty flags/shortcut/menuPath cell is "-"; in cells "|" is written "\|" and newlines become a space,
    - an empty registry has the line "Noch keine Commands registriert." instead of sections. */
std::string exportCommandsMarkdown(const CommandRegistry& registry);

/** Writes <repoRoot>/tools.json and <repoRoot>/docs/commands.md (creates docs/). false on an IO error. */
bool writeExportedFiles(const CommandRegistry& registry, const std::filesystem::path& repoRoot);

struct ExportCheck
{
    bool upToDate = false;
    std::string problem;  ///< if !upToDate: names every stale or missing file ("tools.json", "docs/commands.md")
};

/** Compares <repoRoot>/tools.json and <repoRoot>/docs/commands.md byte for byte with the current export. */
ExportCheck checkExportedFiles(const CommandRegistry& registry, const std::filesystem::path& repoRoot);

}  // namespace tracklab::core
