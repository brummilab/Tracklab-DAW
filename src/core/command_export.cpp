#include "core/command_export.h"

#include "core/tool_names.h"

#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace tracklab::core
{

namespace
{

constexpr const char* kToolsFile = "tools.json";
constexpr const char* kCommandsFile = "docs/commands.md";

/** Table cell: "|" would end the cell, newlines would end the row. Empty becomes "-" where the layout asks for it. */
std::string cell(const std::string& text, bool dashIfEmpty = false)
{
    std::string out;
    for (const char c : text)
    {
        if (c == '|')
            out += "\\|";
        else if (c == '\n' || c == '\r')
            out += ' ';
        else
            out += c;
    }
    return out.empty() && dashIfEmpty ? "-" : out;
}

std::string flagsText(const CommandFlags& flags)
{
    std::string out;
    const auto add = [&out](bool set, const char* name)
    {
        if (!set)
            return;
        out += out.empty() ? "" : ", ";
        out += name;
    };
    add(flags.readOnly, "readOnly");
    add(flags.undoable, "undoable");
    add(flags.destructive, "destructive");
    add(flags.longRunning, "longRunning");
    return out;
}

std::optional<std::string> readFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool writeFile(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    out.flush();
    return static_cast<bool>(out);
}

}  // namespace

std::string exportToolsJson(const CommandRegistry& registry)
{
    // The Claude API takes exactly name, description and input_schema; nlohmann::json sorts the keys, so the text
    // does not depend on insertion order.
    Json tools = Json::array();
    for (const Command* command : registry.list())  // sorted by id
    {
        Json tool = Json::object();
        tool["name"] = toolNameFromId(command->id);
        tool["description"] = command->descriptionEn;
        tool["input_schema"] = command->paramsSchema;
        tools.push_back(std::move(tool));
    }
    return tools.dump(2) + "\n";
}

std::string exportCommandsMarkdown(const CommandRegistry& registry)
{
    std::string text = "# Command-Referenz\n\n"
                       "> Diese Datei wird automatisch aus der Command-Registry erzeugt (`tracklab-cli export-tools`) "
                       "und in der CI auf\n"
                       "> Aktualität geprüft. Nicht von Hand bearbeiten.\n";

    std::map<std::string, std::vector<const Command*>> byNamespace;  // sorted by namespace; rows keep id order
    for (const Command* command : registry.list())
        byNamespace[command->id.substr(0, command->id.find('.'))].push_back(command);

    if (byNamespace.empty())
        return text + "\nNoch keine Commands registriert.\n";

    for (const auto& [name, commands] : byNamespace)
    {
        text += "\n## " + name + "\n\n";
        text += "| ID | Tool-Name | Titel | Beschreibung | Flags | Shortcut | Menüpfad |\n"
                "|---|---|---|---|---|---|---|\n";
        for (const Command* command : commands)
            text += "| `" + command->id + "` | `" + toolNameFromId(command->id) + "` | " + cell(command->titleDe) +
                    " | " + cell(command->descriptionEn) + " | " + cell(flagsText(command->flags), true) + " | " +
                    cell(command->shortcut, true) + " | " + cell(command->menuPath, true) + " |\n";
    }
    return text;
}

bool writeExportedFiles(const CommandRegistry& registry, const std::filesystem::path& repoRoot)
{
    std::error_code error;
    std::filesystem::create_directories(repoRoot / "docs", error);
    if (error)
        return false;
    return writeFile(repoRoot / kToolsFile, exportToolsJson(registry)) &&
           writeFile(repoRoot / kCommandsFile, exportCommandsMarkdown(registry));
}

ExportCheck checkExportedFiles(const CommandRegistry& registry, const std::filesystem::path& repoRoot)
{
    ExportCheck check;
    const std::pair<const char*, std::string> expected[] = {{kToolsFile, exportToolsJson(registry)},
                                                            {kCommandsFile, exportCommandsMarkdown(registry)}};
    for (const auto& [file, text] : expected)
    {
        const auto current = readFile(repoRoot / file);
        if (current == text)
            continue;
        check.problem +=
            std::string(check.problem.empty() ? "" : "; ") + file + (current ? " is out of date" : " is missing");
    }
    check.upToDate = check.problem.empty();
    if (!check.upToDate)
        check.problem += " (regenerate with `tracklab-cli export-tools`)";
    return check;
}

}  // namespace tracklab::core
