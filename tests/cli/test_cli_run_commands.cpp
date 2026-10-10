// tracklab-cli run-commands <project> <commands.json> [--save-as <out>] (cli.h): the steps run through the registry as
// ONE undo transaction; a failing step rolls everything back and nothing is saved.
//
// The built-in commands that exist today (app.*, edit.*, io.*, project.*) are not undoable, so the transaction tests add
// two test-only commands through CliHooks::registerExtraCommands:
//   test.note {value: integer}  undoable, writes the property "cliNote" of the Edit through its UndoManager
//   test.fail {}                undoable, writes cliNote = 99, then fails with the code "test_failed"
// CliHooks::afterCommands looks at the Edit after the batch (undo history, property) before anything is saved.
#include "cli/cli_test_support.h"

#include "core/core_test_helpers.h"

using namespace tracklab_test::cli;
using tracklab::core::Command;
using tracklab::core::CommandFailure;
using tracklab::core::EditContext;

namespace
{

/** What afterCommands saw. */
struct Observed
{
    bool called = false;
    int undoSteps = -1;
    bool hasNote = false;
    int note = 0;
    juce::String undoDescription;
};

tracklab::cli::CliHooks hooksWithTestCommands(Observed& observed)
{
    tracklab::cli::CliHooks hooks;
    hooks.registerExtraCommands = [](tracklab::core::CommandRegistry& registry, EditContext& context)
    {
        using namespace tracklab_test::core_helpers;
        Command note = makeCommand(
            "test.note", objectSchema(Json::parse(R"({"value": {"type": "integer"}})"), Json::array({"value"})));
        note.flags.undoable = true;
        note.handler = [&context](const Json& params)
        {
            auto* edit = context.edit();
            if (edit == nullptr)
                throw CommandFailure("no_edit", "no project open");
            const int value = params.at("value").get<int>();
            edit->state.setProperty("cliNote", value, &edit->getUndoManager());
            return Json{{"value", value}};
        };
        registerOrFail(registry, note);

        Command fail = makeCommand("test.fail");
        fail.flags.undoable = true;
        fail.handler = [&context](const Json&) -> Json
        {
            auto* edit = context.edit();
            if (edit != nullptr)
                edit->state.setProperty("cliNote", 99, &edit->getUndoManager());
            throw CommandFailure("test_failed", "failed on purpose", "/");
        };
        registerOrFail(registry, fail);
    };
    hooks.afterCommands = [&observed](tracktion::Edit& edit)
    {
        observed.called = true;
        observed.undoSteps = edit.getUndoManager().getUndoDescriptions().size();
        observed.undoDescription = edit.getUndoManager().getUndoDescription();
        observed.hasNote = edit.state.hasProperty("cliNote");
        observed.note = static_cast<int>(edit.state.getProperty("cliNote", 0));
    };
    return hooks;
}

Json step(const std::string& id, Json params = Json::object())
{
    return Json{{"id", id}, {"params", std::move(params)}};
}

/** `<root>/<name>` : the value of --save-as; the project file inside is `<name>.tracklab`. */
juce::File saveAsFolder(const ScopedTempDir& dir, const char* name)
{
    return dir.dir().getChildFile(name);
}

juce::File savedProjectFile(const juce::File& folder)
{
    return folder.getChildFile(folder.getFileName() + ".tracklab");
}

}  // namespace

TEST_SUITE("cli")
{
    TEST_CASE("run-commands runs the steps in order, prints one result per step and saves as the new project")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir(), "Muster");
        const auto out = saveAsFolder(dir, "Kopie");
        const auto commands =
            writeCommands(dir.dir(), Json::array({step("app.version"), Json{{"id", "project.get_info"}}}));

        const auto run = runCli({"run-commands", path(project), path(commands), "--save-as", path(out)});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(isSingleJsonLine(run));
        CHECK(run.integer("commands") == 2);
        REQUIRE(run.json["results"].is_array());
        REQUIRE(run.json["results"].size() == 2);
        CHECK(run.json["results"][0]["version"].is_string());  // app.version
        CHECK(run.json["results"][1]["name"] == "Muster");     // project.get_info of the opened project
        CHECK(fs::path(run.text("saved")) == fs::path(path(savedProjectFile(out))));
        CHECK(savedProjectFile(out).existsAsFile());
    }

    TEST_CASE("run-commands leaves the opened project file untouched when --save-as is given")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir(), "Muster");
        const auto before = readText(fs::path(path(project)));
        const auto commands = writeCommands(dir.dir(), Json::array({step("app.version")}));

        const auto run =
            runCli({"run-commands", path(project), path(commands), "--save-as", path(saveAsFolder(dir, "Kopie"))});

        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(readText(fs::path(path(project))) == before);
    }

    TEST_CASE("run-commands: an empty command list is fine and still saves the project")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir(), "Muster");
        const auto out = saveAsFolder(dir, "Kopie");
        const auto commands = writeCommands(dir.dir(), Json::array());

        const auto run = runCli({"run-commands", path(project), path(commands), "--save-as", path(out)});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(run.integer("commands") == 0);
        CHECK(run.json["results"].empty());
        CHECK(savedProjectFile(out).existsAsFile());
    }

    //==========================================================================
    // One transaction, rollback

    TEST_CASE("run-commands: all steps are ONE undo transaction, and the saved project contains their changes")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir(), "Muster");
        const auto out = saveAsFolder(dir, "Kopie");
        const auto commands =
            writeCommands(dir.dir(), Json::array({step("test.note", {{"value", 1}}), step("test.note", {{"value", 2}}),
                                                  step("test.note", {{"value", 3}})}));
        Observed observed;

        const auto run = runCli({"run-commands", path(project), path(commands), "--save-as", path(out)},
                                hooksWithTestCommands(observed));

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        REQUIRE(observed.called);
        CHECK(observed.undoSteps == 1);  // three commands, one step
        CHECK(observed.undoDescription.isNotEmpty());
        CHECK(observed.hasNote);
        CHECK(observed.note == 3);

        const auto saved = tracklab_test::project::parseXml(savedProjectFile(out));
        REQUIRE(saved != nullptr);
        CHECK(saved->getIntAttribute("cliNote", -1) == 3);
    }

    TEST_CASE("run-commands: a failing step rolls the batch back, exits with 1 and saves nothing")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir(), "Muster");
        const auto before = readText(fs::path(path(project)));
        const auto out = saveAsFolder(dir, "Kopie");
        const auto commands = writeCommands(
            dir.dir(),
            Json::array({step("test.note", {{"value", 1}}), step("test.fail"), step("test.note", {{"value", 2}})}));
        Observed observed;

        const auto run = runCli({"run-commands", path(project), path(commands), "--save-as", path(out)},
                                hooksWithTestCommands(observed));

        CHECK(run.exitCode == 1);
        CHECK(isSingleJsonLine(run));
        CHECK_FALSE(run.ok());
        CHECK(run.errorCode() == "test_failed");
        CHECK(run.integer("failed_index") == 1);
        REQUIRE(observed.called);
        CHECK(observed.undoSteps == 0);  // rolled back: no undo entry
        CHECK_FALSE(observed.hasNote);   // test.note's write of step 0 is undone as well
        CHECK_FALSE(out.exists());
        CHECK(readText(fs::path(path(project))) == before);
    }

    TEST_CASE("run-commands: a step that fails at run time (test.fail) reports index and code, rolls back")
    {
        // Was project.open of a missing file; project.* commands are not allowed in a batch any more (only undoable or
        // readOnly commands are), so the run-time failure comes from the undoable test command of the hook.
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir(), "Muster");
        const auto before = readText(fs::path(path(project)));
        const auto out = saveAsFolder(dir, "Kopie");
        const auto commands = writeCommands(
            dir.dir(), Json::array({step("project.get_info"), step("test.note", {{"value", 1}}), step("test.fail")}));
        Observed observed;

        const auto run = runCli({"run-commands", path(project), path(commands), "--save-as", path(out)},
                                hooksWithTestCommands(observed));

        CHECK(run.exitCode == 1);
        CHECK(run.errorCode() == "test_failed");
        CHECK(run.integer("failed_index") == 2);
        REQUIRE(observed.called);
        CHECK(observed.undoSteps == 0);
        CHECK_FALSE(observed.hasNote);
        CHECK_FALSE(out.exists());
        CHECK(readText(fs::path(path(project))) == before);
    }

    TEST_CASE("run-commands: an unknown command is refused up front (exit 1, unknown_command, index of the step)")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir(), "Muster");
        const auto out = saveAsFolder(dir, "Kopie");
        const auto commands = writeCommands(dir.dir(), Json::array({step("app.version"), step("no.such_command")}));

        const auto run = runCli({"run-commands", path(project), path(commands), "--save-as", path(out)});

        CHECK(run.exitCode == 1);
        CHECK(isSingleJsonLine(run));
        CHECK(run.errorCode() == "unknown_command");
        CHECK(run.integer("failed_index") == 1);
        CHECK_FALSE(out.exists());
    }

    TEST_CASE("run-commands: invalid params are refused with invalid_params and the pointer of the field")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir(), "Muster");
        const auto out = saveAsFolder(dir, "Kopie");
        const auto commands = writeCommands(dir.dir(), Json::array({step("app.version", {{"surplus", 1}})}));

        const auto run = runCli({"run-commands", path(project), path(commands), "--save-as", path(out)});

        CHECK(run.exitCode == 1);
        CHECK(run.errorCode() == "invalid_params");
        CHECK(run.integer("failed_index") == 0);
        CHECK(run.json["error"].contains("pointer"));
        CHECK_FALSE(out.exists());
    }

    TEST_CASE("run-commands: --save-as to an existing project is refused (project_exists) and does not overwrite it")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir(), "Muster");
        const auto existing = makeEmptyProject(dir.dir(), "Kopie");
        const auto existingBefore = readText(fs::path(path(existing)));
        const auto commands = writeCommands(dir.dir(), Json::array({step("app.version")}));

        const auto run =
            runCli({"run-commands", path(project), path(commands), "--save-as", path(saveAsFolder(dir, "Kopie"))});

        CHECK(run.exitCode == 1);
        CHECK(run.errorCode() == "project_exists");
        CHECK(readText(fs::path(path(existing))) == existingBefore);
    }

    //==========================================================================
    // Input errors

    TEST_CASE("run-commands: a missing project exits with code 1 (project_not_found)")
    {
        ScopedTempDir dir;
        const auto commands = writeCommands(dir.dir(), Json::array());

        const auto run = runCli({"run-commands", path(dir.dir().getChildFile("Nope/Nope.tracklab")), path(commands),
                                 "--save-as", path(saveAsFolder(dir, "Kopie"))});

        CHECK(run.exitCode == 1);
        CHECK(isSingleJsonLine(run));
        CHECK(run.errorCode() == "project_not_found");
    }

    TEST_CASE("run-commands: a corrupt project exits with code 1 (corrupt_project)")
    {
        ScopedTempDir dir;
        const auto project = dir.dir().getChildFile("Kaputt/Kaputt.tracklab");
        writeText(fs::path(path(project)), "this is not xml");
        const auto commands = writeCommands(dir.dir(), Json::array());

        const auto run =
            runCli({"run-commands", path(project), path(commands), "--save-as", path(saveAsFolder(dir, "Kopie"))});

        CHECK(run.exitCode == 1);
        CHECK(run.errorCode() == "corrupt_project");
    }

    TEST_CASE("run-commands: a missing commands file exits with code 1")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir(), "Muster");

        const auto run = runCli({"run-commands", path(project), path(dir.dir().getChildFile("missing.json")),
                                 "--save-as", path(saveAsFolder(dir, "Kopie"))});

        CHECK(run.exitCode == 1);
        CHECK(isSingleJsonLine(run));
        CHECK_FALSE(run.errorCode().empty());
        CHECK_FALSE(saveAsFolder(dir, "Kopie").exists());
    }

    TEST_CASE(
        "run-commands: a commands file that is not valid JSON, not an array, or has a step without id exits with 1")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir(), "Muster");
        const char* bad[] = {"{ this is not json", R"({"id": "app.version"})", R"([{"params": {}}])", R"([42])"};
        int index = 0;
        for (const char* text : bad)
        {
            CAPTURE(text);
            const auto file = dir.dir().getChildFile("bad" + juce::String(index++) + ".json");
            writeText(fs::path(path(file)), text);
            const auto out = saveAsFolder(dir, ("Kopie" + std::to_string(index)).c_str());

            const auto run = runCli({"run-commands", path(project), path(file), "--save-as", path(out)});

            CHECK(run.exitCode == 1);
            CHECK(isSingleJsonLine(run));
            CHECK_FALSE(run.ok());
            CHECK_FALSE(run.errorCode().empty());
            CHECK_FALSE(out.exists());
        }
    }

    TEST_CASE("run-commands without arguments, with one argument, or with --save-as but no value is a usage error")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir(), "Muster");
        const auto commands = writeCommands(dir.dir(), Json::array());

        const auto none = runCli({"run-commands"});
        const auto one = runCli({"run-commands", path(project)});
        const auto noValue = runCli({"run-commands", path(project), path(commands), "--save-as"});

        for (const auto* run : {&none, &one, &noValue})
        {
            CHECK(run->exitCode == 2);
            CHECK(isSingleJsonLine(*run));
            CHECK(run->errorCode() == "usage");
        }
    }
}
