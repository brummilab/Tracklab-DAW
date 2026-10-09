// tracklab-cli, decisions of the lead after the first tests (brief M1-07, "Lead-Entscheidungen nach Test-Writer"):
//   2  run-commands without --save-as saves the opened project in place
//   3  the undo transaction of run-commands is called "CLI: run-commands"
//   4  analyze without a measurement flag measures everything
//   6  --version prints the version; one registration function for the CLI and for the export test
#include "cli/builtin_commands.h"
#include "cli/cli_test_support.h"

#include "core/core_test_helpers.h"
#include "engine/engine_test_options.h"

using namespace tracklab_test::cli;
using tracklab::core::Command;
using tracklab::core::EditContext;

namespace
{

/** Hooks with one undoable test command (test.note {value}) that writes the property "cliNote" of the Edit, and an
    observer for the undo description the batch left. */
tracklab::cli::CliHooks hooksWithNote(juce::String& undoDescription)
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
            const int value = params.at("value").get<int>();
            context.edit()->state.setProperty("cliNote", value, &context.edit()->getUndoManager());
            return Json{{"value", value}};
        };
        registerOrFail(registry, note);
    };
    hooks.afterCommands = [&undoDescription](tracktion::Edit& edit)
    { undoDescription = edit.getUndoManager().getUndoDescription(); };
    return hooks;
}

Json noteStep(int value)
{
    return Json{{"id", "test.note"}, {"params", {{"value", value}}}};
}

}  // namespace

TEST_SUITE("cli")
{
    TEST_CASE("run-commands without --save-as saves the opened project in place and creates nothing else")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir(), "Muster");
        const auto commands = writeCommands(dir.dir(), Json::array({noteStep(7)}));
        juce::String description;

        const auto run = runCli({"run-commands", path(project), path(commands)}, hooksWithNote(description));

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(isSingleJsonLine(run));
        CHECK(fs::path(run.text("saved")) == fs::path(path(project)));
        const auto saved = tracklab_test::project::parseXml(project);
        REQUIRE(saved != nullptr);
        CHECK(saved->getIntAttribute("cliNote", -1) == 7);

        // Nothing but the project folder next to the commands file: no copy, no temporary file left behind.
        CHECK(dir.dir().getNumberOfChildFiles(juce::File::findFilesAndDirectories) == 2);  // Muster/, commands.json
        CHECK(project.getParentDirectory().getChildFile("Muster.tracklab").existsAsFile());
        CHECK(project.getParentDirectory().findChildFiles(juce::File::findFiles, false, "*.tracklab").size() == 1);
    }

    TEST_CASE("run-commands in place: a failing step leaves the project file untouched")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir(), "Muster");
        const auto before = readText(fs::path(path(project)));
        const auto commands = writeCommands(dir.dir(), Json::array({noteStep(7), Json{{"id", "no.such_command"}}}));
        juce::String description;

        const auto run = runCli({"run-commands", path(project), path(commands)}, hooksWithNote(description));

        CHECK(run.exitCode == 1);
        CHECK(run.errorCode() == "unknown_command");
        CHECK(readText(fs::path(path(project))) == before);
    }

    TEST_CASE("run-commands names its undo transaction \"CLI: run-commands\"")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir(), "Muster");
        const auto commands = writeCommands(dir.dir(), Json::array({noteStep(1), noteStep(2)}));
        juce::String description;

        const auto run = runCli({"run-commands", path(project), path(commands), "--save-as",
                                 path(dir.dir().getChildFile("Kopie"))},
                                hooksWithNote(description));

        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(description == "CLI: run-commands");
    }

    TEST_CASE("analyze without a measurement flag measures loudness, true peak and LRA")
    {
        ScopedTempDir dir;
        const auto file = dir.dir().getChildFile("sine-20.wav");
        writeWav(file, makeSine(48000.0, 2, 997.0, {{20.0, 0.1}}), 48000.0, 24);

        const auto run = runCli({"analyze", path(file)});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        REQUIRE(run.has("integrated_lufs"));
        REQUIRE(run.has("true_peak_dbtp"));
        REQUIRE(run.has("lra"));
        CHECK(std::abs(run.number("integrated_lufs") - -20.0) <= 0.1);
        CHECK(std::abs(run.number("true_peak_dbtp") - -20.0) <= 0.1);
        CHECK(std::abs(run.number("lra") - 0.0) <= 0.1);
    }

    TEST_CASE("--version prints the version of the app as one JSON line and exits with 0")
    {
        const auto run = runCli({"--version"});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(isSingleJsonLine(run));
        CHECK(run.text("version") == TRACKLAB_EXPECTED_VERSION);
    }

    TEST_CASE("--version is the version that app.version reports through run-commands")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir(), "Muster");
        const auto commands = writeCommands(dir.dir(), Json::array({Json{{"id", "app.version"}}}));

        const auto edit = runCli({"run-commands", path(project), path(commands)});
        const auto version = runCli({"--version"});

        REQUIRE_MESSAGE(edit.ok(), edit.out);
        CHECK(edit.json["results"][0]["version"] == version.text("version"));
    }

    TEST_CASE("--version with further arguments is a usage error")
    {
        const auto run = runCli({"--version", "render"});

        CHECK(run.exitCode == 2);
        CHECK(run.errorCode() == "usage");
    }

    TEST_CASE("registerBuiltInCommands registers the app, edit, io and project commands, and only once per registry")
    {
        auto engine = tracklab::engine::createEngine(tracklab_test::testOptions());
        EditContext context;
        tracklab::project::ProjectSession session(*engine, context);
        tracklab::core::CommandRegistry registry;

        const auto first = tracklab::cli::registerBuiltInCommands(registry, *engine, context, session, "1.2.3");
        const auto again = tracklab::cli::registerBuiltInCommands(registry, *engine, context, session, "1.2.3");

        REQUIRE_MESSAGE(first.ok, first.error.message);
        for (const char* id : {"app.version", "edit.undo", "io.list_devices", "project.open", "project.save_as"})
        {
            CAPTURE(id);
            CHECK(registry.contains(id));
        }
        CHECK_FALSE(again.ok);
        CHECK(again.error.code == "duplicate_id");
    }
}
