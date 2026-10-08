// Registry export (M1-02): tools.json and docs/commands.md are deterministic, and the checked-in files are current.
// The CLI (`tracklab-cli export-tools`) follows in M1-07; until then
//   TRACKLAB_UPDATE_EXPORTS=1 ./tracklab_tests --test-case="*checked-in*"
// rewrites the two files in the source tree (never set by the gate).
#include "core/core_test_helpers.h"

#include "core/app_commands.h"
#include "core/command_export.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

using namespace tracklab::core;
using namespace tracklab_test::core_helpers;
namespace fs = std::filesystem;

Command exampleCommand(const std::string& id, const std::string& title, const std::string& description)
{
    Command command =
        makeCommand(id, objectSchema(Json::parse(R"({"name": {"type": "string"}})"), Json::array({"name"})));
    command.titleDe = title;
    command.descriptionEn = description;
    return command;
}

/** track.create (undoable, shortcut, menu), track.delete (undoable + destructive), app.version (readOnly). */
void fillExampleRegistry(CommandRegistry& registry, bool reverse = false)
{
    std::vector<Command> commands;
    {
        Command c = exampleCommand("track.create", "Spur anlegen", "Creates a track.");
        c.flags.undoable = true;
        c.shortcut = "Ctrl+T";
        c.menuPath = "Spur/Neu";
        commands.push_back(c);
    }
    {
        Command c = exampleCommand("track.delete", "Spur l\xC3\xB6schen", "Deletes a track.");
        c.flags.undoable = true;
        c.flags.destructive = true;
        commands.push_back(c);
    }
    {
        Command c = exampleCommand("app.version", "Version", "Returns the version.");
        c.flags.readOnly = true;
        commands.push_back(c);
    }
    if (reverse)
        std::reverse(commands.begin(), commands.end());
    for (const auto& c : commands)
        registerOrFail(registry, c);
}

std::string readFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

void writeFile(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

bool contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

std::vector<std::string> linesOf(const std::string& text)
{
    std::vector<std::string> lines;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);)
        lines.push_back(line);
    return lines;
}

}  // namespace

TEST_SUITE("core")
{
    //==========================================================================
    // tools.json
    TEST_CASE("tools.json of an empty registry is an empty array")
    {
        const CommandRegistry registry;
        CHECK(exportToolsJson(registry) == "[]\n");
    }

    TEST_CASE("tools.json lists name, description and input_schema per command, sorted by id")
    {
        CommandRegistry registry;
        fillExampleRegistry(registry);

        const std::string text = exportToolsJson(registry);
        const Json tools = Json::parse(text, nullptr, false);
        REQUIRE(tools.is_array());
        REQUIRE(tools.size() == 3);

        const char* ids[] = {"app.version", "track.create", "track.delete"};
        for (std::size_t i = 0; i < 3; ++i)
        {
            const Command* command = registry.find(ids[i]);
            REQUIRE(command != nullptr);
            CAPTURE(ids[i]);
            REQUIRE(tools[i].is_object());
            CHECK(tools[i].size() == 3);  // no other keys: the Claude API rejects unknown tool fields
            std::string expectedName = ids[i];
            std::replace(expectedName.begin(), expectedName.end(), '.', '_');
            CHECK(tools[i]["name"] == expectedName);
            CHECK(tools[i]["description"] == command->descriptionEn);
            CHECK(tools[i]["input_schema"] == command->paramsSchema);
        }
    }

    TEST_CASE("tools.json is pretty-printed with 2 spaces, ends with one newline and has no CR")
    {
        CommandRegistry registry;
        fillExampleRegistry(registry);
        const std::string text = exportToolsJson(registry);
        REQUIRE_FALSE(text.empty());
        CHECK(text.back() == '\n');
        CHECK((text.size() < 2 || text[text.size() - 2] != '\n'));
        CHECK_FALSE(contains(text, "\r"));
        CHECK(contains(text, "\n  {\n    \"description\": "));
    }

    TEST_CASE("tools.json is the same for every registration order and every call")
    {
        CommandRegistry forward;
        CommandRegistry backward;
        fillExampleRegistry(forward);
        fillExampleRegistry(backward, true);
        CHECK(exportToolsJson(forward) == exportToolsJson(backward));
        CHECK(exportToolsJson(forward) == exportToolsJson(forward));
    }

    //==========================================================================
    // docs/commands.md
    TEST_CASE("commands.md of an empty registry says that no command is registered")
    {
        const CommandRegistry registry;
        const std::string text = exportCommandsMarkdown(registry);
        CHECK(text.rfind("# Command-Referenz\n", 0) == 0);
        CHECK(contains(text, "Noch keine Commands registriert."));
        CHECK_FALSE(contains(text, "## "));
    }

    TEST_CASE("commands.md has a title, a note that it is generated, and one table per namespace in order")
    {
        CommandRegistry registry;
        fillExampleRegistry(registry);
        const std::string text = exportCommandsMarkdown(registry);

        CHECK(text.rfind("# Command-Referenz\n", 0) == 0);
        CHECK(contains(text, "export-tools"));
        CHECK(contains(text, "Nicht von Hand"));
        CHECK_FALSE(contains(text, "Noch keine Commands registriert."));

        const auto app = text.find("\n## app\n");
        const auto track = text.find("\n## track\n");
        REQUIRE(app != std::string::npos);
        REQUIRE(track != std::string::npos);
        CHECK(app < track);

        const std::string header = "| ID | Tool-Name | Titel | Beschreibung | Flags | Shortcut | Men\xC3\xBCpfad "
                                   "|\n|---|---|---|---|---|---|---|\n";
        const auto firstHeader = text.find(header);
        REQUIRE(firstHeader != std::string::npos);
        CHECK(text.find(header, firstHeader + 1) != std::string::npos);  // a header per namespace
    }

    TEST_CASE("commands.md rows: id, tool name, German title, English description, flags, shortcut, menu path")
    {
        CommandRegistry registry;
        fillExampleRegistry(registry);
        const auto lines = linesOf(exportCommandsMarkdown(registry));
        const auto has = [&](const std::string& row)
        { return std::find(lines.begin(), lines.end(), row) != lines.end(); };

        CHECK(has(
            "| `track.create` | `track_create` | Spur anlegen | Creates a track. | undoable | Ctrl+T | Spur/Neu |"));
        CHECK(has("| `track.delete` | `track_delete` | Spur l\xC3\xB6schen | Deletes a track. | undoable, destructive "
                  "| - | - |"));
        CHECK(has("| `app.version` | `app_version` | Version | Returns the version. | readOnly | - | - |"));
    }

    TEST_CASE("commands.md rows of a namespace are sorted by id")
    {
        CommandRegistry registry;
        fillExampleRegistry(registry);
        const std::string text = exportCommandsMarkdown(registry);
        const auto create = text.find("| `track.create` |");
        const auto del = text.find("| `track.delete` |");
        REQUIRE(create != std::string::npos);
        REQUIRE(del != std::string::npos);
        CHECK(create < del);
    }

    TEST_CASE("commands.md keeps the table intact: '|' is escaped and newlines become spaces")
    {
        CommandRegistry registry;
        Command command = exampleCommand("track.create", "Spur", "Creates a | track.\nSecond line.");
        registerOrFail(registry, command);
        const auto lines = linesOf(exportCommandsMarkdown(registry));
        const auto it = std::find_if(lines.begin(), lines.end(),
                                     [](const std::string& line) { return line.rfind("| `track.create` |", 0) == 0; });
        REQUIRE(it != lines.end());
        CHECK(contains(*it, "Creates a \\| track. Second line."));
    }

    TEST_CASE("commands.md is the same for every registration order and every call, ends with a newline, has no CR")
    {
        CommandRegistry forward;
        CommandRegistry backward;
        fillExampleRegistry(forward);
        fillExampleRegistry(backward, true);
        const std::string text = exportCommandsMarkdown(forward);
        CHECK(text == exportCommandsMarkdown(backward));
        CHECK(text == exportCommandsMarkdown(forward));
        REQUIRE_FALSE(text.empty());
        CHECK(text.back() == '\n');
        CHECK_FALSE(contains(text, "\r"));
    }

    //==========================================================================
    // Writing and the freshness check
    TEST_CASE("writeExportedFiles writes tools.json and docs/commands.md (docs/ is created)")
    {
        const tracklab_test::ScopedTempDir temp;
        const fs::path root = temp.dir().getFullPathName().toStdString();
        CommandRegistry registry;
        fillExampleRegistry(registry);

        REQUIRE(writeExportedFiles(registry, root));
        CHECK(readFile(root / "tools.json") == exportToolsJson(registry));
        CHECK(readFile(root / "docs" / "commands.md") == exportCommandsMarkdown(registry));
    }

    TEST_CASE("freshly written files are up to date")
    {
        const tracklab_test::ScopedTempDir temp;
        const fs::path root = temp.dir().getFullPathName().toStdString();
        CommandRegistry registry;
        fillExampleRegistry(registry);
        REQUIRE(writeExportedFiles(registry, root));

        const auto check = checkExportedFiles(registry, root);
        INFO(check.problem);
        CHECK(check.upToDate);
        CHECK(check.problem.empty());
    }

    TEST_CASE("a changed tools.json is reported as stale and named")
    {
        const tracklab_test::ScopedTempDir temp;
        const fs::path root = temp.dir().getFullPathName().toStdString();
        CommandRegistry registry;
        fillExampleRegistry(registry);
        REQUIRE(writeExportedFiles(registry, root));
        writeFile(root / "tools.json", readFile(root / "tools.json") + " ");

        const auto check = checkExportedFiles(registry, root);
        CHECK_FALSE(check.upToDate);
        CHECK(contains(check.problem, "tools.json"));
        CHECK_FALSE(contains(check.problem, "commands.md"));
    }

    TEST_CASE("a changed docs/commands.md is reported as stale and named")
    {
        const tracklab_test::ScopedTempDir temp;
        const fs::path root = temp.dir().getFullPathName().toStdString();
        CommandRegistry registry;
        fillExampleRegistry(registry);
        REQUIRE(writeExportedFiles(registry, root));
        writeFile(root / "docs" / "commands.md", "# Von Hand ver\xC3\xA4ndert\n");

        const auto check = checkExportedFiles(registry, root);
        CHECK_FALSE(check.upToDate);
        CHECK(contains(check.problem, "docs/commands.md"));
    }

    TEST_CASE("missing files are reported as not up to date")
    {
        const tracklab_test::ScopedTempDir temp;
        const fs::path root = temp.dir().getFullPathName().toStdString();
        CommandRegistry registry;
        fillExampleRegistry(registry);

        const auto check = checkExportedFiles(registry, root);
        CHECK_FALSE(check.upToDate);
        CHECK(contains(check.problem, "tools.json"));
        CHECK(contains(check.problem, "docs/commands.md"));
    }

    TEST_CASE("a command registered after the export makes both files stale")
    {
        const tracklab_test::ScopedTempDir temp;
        const fs::path root = temp.dir().getFullPathName().toStdString();
        CommandRegistry registry;
        fillExampleRegistry(registry);
        REQUIRE(writeExportedFiles(registry, root));
        registerOrFail(registry, exampleCommand("track.rename", "Spur umbenennen", "Renames a track."));

        const auto check = checkExportedFiles(registry, root);
        CHECK_FALSE(check.upToDate);
        CHECK(contains(check.problem, "tools.json"));
        CHECK(contains(check.problem, "docs/commands.md"));
    }

    //==========================================================================
    TEST_CASE("the checked-in tools.json and docs/commands.md match the registry (all built-in commands)")
    {
        // The registry as the app builds it: every registerXxxCommands() of src/core. Today: app.version.
        // The version is not part of the exports, so any string does.
        CommandRegistry registry;
        REQUIRE(registerAppCommands(registry, "0.0.0").ok);

        const fs::path root = TRACKLAB_SOURCE_DIR;
        const char* update = std::getenv("TRACKLAB_UPDATE_EXPORTS");
        if (update != nullptr && std::string(update) == "1")
            REQUIRE(writeExportedFiles(registry, root));

        const auto check = checkExportedFiles(registry, root);
        INFO(check.problem << " -- regenerate: TRACKLAB_UPDATE_EXPORTS=1 tracklab_tests --test-case=\"*checked-in*\"");
        CHECK(check.upToDate);
    }
}
