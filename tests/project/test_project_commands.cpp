// The project.* commands as registry entries (M1-04): ids, flags, schemas, the undo contract for undoable commands, and
// the checked-in exports (tools.json, docs/commands.md).
#include "project/project_fixture.h"

#include "core/command_export.h"
#include "core/undo_contract.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

namespace
{

using namespace tracklab_test::project;
using tracklab::core::CommandFlags;
using tracklab::core::CommandRegistry;

/** id -> expected flags. Opening, saving and closing are not steps of the project's undo history, so none is undoable. */
const std::map<std::string, CommandFlags>& expectedCommands()
{
    static const std::map<std::string, CommandFlags> commands = {
        {"project.new", CommandFlags{}},
        {"project.open", CommandFlags{}},
        {"project.save", CommandFlags{}},
        {"project.save_as", CommandFlags{}},
        {"project.close", CommandFlags{.destructive = true}},
        {"project.get_info", CommandFlags{.readOnly = true}},
    };
    return commands;
}

std::string readTextFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("registerProjectCommands registers exactly the six project commands with their flags")
    {
        ProjectFixture f;

        for (const auto& [id, flags] : expectedCommands())
        {
            INFO("command " << id);
            const auto* command = f.registry.find(id);
            REQUIRE(command != nullptr);
            CHECK(command->flags == flags);
            CHECK_FALSE(command->titleDe.empty());
            CHECK_FALSE(command->descriptionEn.empty());
        }

        int projectCommands = 0;
        for (const auto* command : f.registry.list())
            if (command->id.rfind("project.", 0) == 0)
                ++projectCommands;
        CHECK(projectCommands == static_cast<int>(expectedCommands().size()));
    }

    TEST_CASE("only project.close is destructive and none of the project commands is undoable")
    {
        ProjectFixture f;

        int seen = 0;
        for (const auto* command : f.registry.list())
        {
            if (command->id.rfind("project.", 0) != 0)
                continue;
            ++seen;
            INFO("command " << command->id);
            CHECK_FALSE(command->flags.undoable);
            CHECK(command->flags.destructive == (command->id == "project.close"));
            CHECK(command->flags.readOnly == (command->id == "project.get_info"));
        }
        CHECK(seen == 6);  // the loop is not vacuous
    }

    TEST_CASE(
        "the tool names are project_new, project_open, project_save, project_save_as, project_close, project_get_info")
    {
        ProjectFixture f;

        CHECK(f.registry.toolNameForId("project.new") == "project_new");
        CHECK(f.registry.toolNameForId("project.open") == "project_open");
        CHECK(f.registry.toolNameForId("project.save") == "project_save");
        CHECK(f.registry.toolNameForId("project.save_as") == "project_save_as");
        CHECK(f.registry.toolNameForId("project.close") == "project_close");
        CHECK(f.registry.toolNameForId("project.get_info") == "project_get_info");
        CHECK(f.registry.idForToolName("project_save_as") == "project.save_as");
    }

    TEST_CASE("registering the project commands twice on one registry fails with duplicate_id")
    {
        ProjectFixture f;

        const auto outcome = tracklab::project::registerProjectCommands(f.registry, *f.session);

        CHECK_FALSE(outcome.ok);
        CHECK(outcome.error.code == "duplicate_id");
    }

    TEST_CASE("commands without parameters take none")
    {
        ProjectFixture f;
        f.newProject("Muster");

        CHECK(f.errorOf("project.save", Json{{"force", true}}) == "invalid_params");
        CHECK(f.errorOf("project.get_info", Json{{"verbose", true}}) == "invalid_params");
    }

    TEST_CASE("project.new, project.open and project.get_info work without an open project (they are not undoable)")
    {
        ProjectFixture f;

        CHECK(f.errorOf("project.get_info") == "no_edit");
        CHECK(f.tryRun("project.new", Json{{"folder", utf8(f.root())}, {"name", "Muster"}}).ok);
        const auto file = f.projectFile("Muster");
        f.run("project.close");
        CHECK(f.tryRun("project.open", Json{{"path", utf8(file)}}).ok);
    }

    TEST_CASE("get_info has exactly path, name, format_version and modified")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");

        const auto info = f.info();

        CHECK(info.size() == 4);
        CHECK(info["path"].is_string());
        CHECK(info["name"].is_string());
        CHECK(info["format_version"].is_number_integer());
        CHECK(info["modified"].is_boolean());
        CHECK(info["path"].get<std::string>() == utf8(file));
    }

    //==========================================================================
    // Undo
    TEST_CASE(
        "every registered undoable command honours the undo contract (the project commands bring no undoable one)")
    {
        ProjectFixture f;
        f.newProject("Muster");
        f.settleEdit();

        // No samples: a project command that is flagged undoable would be reported as "without a contract sample", and
        // so would a new undoable command of a later card that forgot its sample.
        const auto report = tracklab_test::undo::checkAllUndoableCommands(f.registry, f.edit(), {});

        tracklab_test::undo::requireContract(report);
    }

    TEST_CASE("the project commands add no step to the project's undo history")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.addSampleContent();
        f.settleEdit();
        const auto before = f.edit().getUndoManager().getUndoDescriptions();
        const auto canRedoBefore = f.edit().getUndoManager().canRedo();

        f.run("project.save");
        f.run("project.get_info");
        f.run("project.save_as", Json{{"folder", utf8(f.root())}, {"name", "Zweites"}});
        f.settleEdit();

        CHECK(f.edit().getUndoManager().getUndoDescriptions() == before);
        CHECK(f.edit().getUndoManager().canRedo() == canRedoBefore);
        CHECK(file.existsAsFile());
    }

    //==========================================================================
    // docs/commands.md and tools.json
    TEST_CASE("the checked-in tools.json lists every project tool exactly as the registry exports it")
    {
        ProjectFixture f;
        // The registry of the project commands alone: the other commands are checked by the core test.
        CommandRegistry onlyProject;
        REQUIRE(tracklab::project::registerProjectCommands(onlyProject, *f.session).ok);
        const auto exported = Json::parse(tracklab::core::exportToolsJson(onlyProject));
        REQUIRE(exported.is_array());
        REQUIRE(exported.size() == expectedCommands().size());

        const std::filesystem::path root = TRACKLAB_SOURCE_DIR;
        const auto checkedIn = Json::parse(readTextFile(root / "tools.json"), nullptr, false);
        REQUIRE(checkedIn.is_array());

        for (const auto& tool : exported)
        {
            INFO("tool " << tool["name"].get<std::string>());
            bool found = false;
            for (const auto& candidate : checkedIn)
                if (candidate == tool)
                    found = true;
            CHECK(found);
            CHECK(tool["input_schema"]["additionalProperties"] == false);
        }
    }

    TEST_CASE("the checked-in docs/commands.md has a row for every project command")
    {
        ProjectFixture f;
        CommandRegistry onlyProject;
        REQUIRE(tracklab::project::registerProjectCommands(onlyProject, *f.session).ok);
        const auto markdown = tracklab::core::exportCommandsMarkdown(onlyProject);
        const auto docs = readTextFile(std::filesystem::path(TRACKLAB_SOURCE_DIR) / "docs" / "commands.md");

        std::istringstream lines(markdown);
        std::string line;
        int rows = 0;
        while (std::getline(lines, line))
        {
            if (line.rfind("| `project.", 0) != 0)
                continue;
            ++rows;
            INFO("row: " << line);
            CHECK(docs.find(line + "\n") != std::string::npos);
        }
        CHECK(rows == static_cast<int>(expectedCommands().size()));
    }
}
