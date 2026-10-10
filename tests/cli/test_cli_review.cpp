// tracklab-cli, review round 1 of M1-07:
//   - render takes audio tracks at any depth (plain folders) and submix folders into the render
//   - analyze of digital silence: "no measurement" is JSON null, not -100
//   - run-commands only runs commands that are undoable or readOnly (command_not_allowed)
#include "cli/cli_test_support.h"

#include "core/core_test_helpers.h"

#include <cstdint>

using namespace tracklab_test::cli;
using tracklab::core::Command;
using tracklab::core::EditContext;

namespace
{

namespace te = tracktion;

enum class Nesting : std::uint8_t
{
    none,           ///< the track at the top level
    plainFolder,    ///< in an ordinary folder track
    nestedFolders,  ///< in an ordinary folder inside an ordinary folder
    submix          ///< in a submix folder (the folder plays its children through its own plugin chain)
};

/** A project with ONE clip (220 Hz sine, 0.25 peak, 1 s, mono 48 kHz / 24 bit) on one audio track, placed as
    `nesting` says. The same clip in every variant, so that their renders have to be the same signal. */
juce::File buildOneClipProject(const juce::File& root, Nesting nesting)
{
    REQUIRE(root.createDirectory().wasOk());  // project.new never creates the parent folder
    tracklab_test::project::ProjectFixture fx;
    const auto info = fx.run("project.new", Json{{"folder", path(root)}, {"name", "Muster"}});
    fx.settleEdit();
    const auto projectFile = tracklab_test::project::fileFromUtf8(info["path"].get<std::string>());
    const auto stem = projectFile.getParentDirectory().getChildFile("Audio/stem.wav");
    writeWav(stem, makeSine(48000.0, 1, 220.0, {{1.0, 0.25}}), 48000.0, 24);

    auto& edit = fx.edit();
    te::TrackInsertPoint where = te::TrackInsertPoint::getEndOfTracks(edit);
    if (nesting == Nesting::plainFolder || nesting == Nesting::nestedFolders || nesting == Nesting::submix)
    {
        auto outer = edit.insertNewFolderTrack(where, nullptr, nesting == Nesting::submix);
        REQUIRE(outer != nullptr);
        where = te::TrackInsertPoint(outer.get(), nullptr);
        if (nesting == Nesting::nestedFolders)
        {
            auto inner = edit.insertNewFolderTrack(where, nullptr, false);
            REQUIRE(inner != nullptr);
            where = te::TrackInsertPoint(inner.get(), nullptr);
        }
    }
    auto track = edit.insertNewAudioTrack(where, nullptr);
    REQUIRE(track != nullptr);
    auto clip = te::insertWaveClip(
        *track, "Stem", stem,
        te::ClipPosition{.time = te::TimeRange(te::TimePosition::fromSeconds(0.0), te::TimePosition::fromSeconds(1.0))},
        te::DeleteExistingClips::no);
    REQUIRE(clip != nullptr);
    clip->setUsesProxy(false);

    tracklab_test::project::settle(edit);
    tracklab_test::project::ProjectFixture::letPluginTimersRunOut(edit);
    fx.run("project.save");
    return projectFile;
}

AudioData renderOf(const ScopedTempDir& dir, const juce::File& project, const char* name)
{
    const auto out = dir.dir().getChildFile(name);
    const auto run = runCli({"render", path(project), "--out", path(out)});
    REQUIRE_MESSAGE(run.ok(), run.out);
    return readAudio(out);
}

/** One undoable test command, and an observer that tells whether the batch ran at all. */
tracklab::cli::CliHooks hooksWithNote(bool& batchRan)
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
    hooks.afterCommands = [&batchRan](tracktion::Edit&) { batchRan = true; };
    return hooks;
}

Json step(const std::string& id, Json params = Json::object())
{
    return Json{{"id", id}, {"params", std::move(params)}};
}

}  // namespace

TEST_SUITE("cli")
{
    //==========================================================================
    // render: tracks at any depth

    TEST_CASE(
        "render: a clip on a track in a plain folder (also nested) renders like the same clip on a top-level track")
    {
        ScopedTempDir dir;
        const auto reference =
            renderOf(dir, buildOneClipProject(dir.dir().getChildFile("a"), Nesting::none), "none.wav");
        REQUIRE(reference.valid);
        REQUIRE(peakDbfs(reference) > -20.0);  // the clip is in the render, not silence

        for (const auto nesting : {Nesting::plainFolder, Nesting::nestedFolders})
        {
            CAPTURE(static_cast<int>(nesting));
            const auto project =
                buildOneClipProject(dir.dir().getChildFile("b" + juce::String(static_cast<int>(nesting))), nesting);
            const auto audio =
                renderOf(dir, project, ("folder" + std::to_string(static_cast<int>(nesting)) + ".wav").c_str());

            REQUIRE(audio.valid);
            CHECK(peakDbfs(audio) > -20.0);
            CHECK(residualDbfs(reference, audio) < -90.0);
        }
    }

    TEST_CASE("render: a clip on a track in a submix folder renders like the same clip on a top-level track (once)")
    {
        ScopedTempDir dir;
        const auto reference =
            renderOf(dir, buildOneClipProject(dir.dir().getChildFile("a"), Nesting::none), "none.wav");
        const auto submix =
            renderOf(dir, buildOneClipProject(dir.dir().getChildFile("b"), Nesting::submix), "submix.wav");

        REQUIRE(reference.valid);
        REQUIRE(submix.valid);
        CHECK(peakDbfs(submix) > -20.0);
        CHECK(residualDbfs(reference, submix) < -90.0);  // played twice would be +6 dB
    }

    //==========================================================================
    // analyze: silence

    TEST_CASE("analyze of digital silence: exit 0, integrated loudness and true peak are null (no measurement)")
    {
        ScopedTempDir dir;
        const auto file = dir.dir().getChildFile("silence.wav");
        writeWav(file, makeSine(48000.0, 2, 997.0, {{5.0, 0.0}}), 48000.0, 24);

        const auto run = runCli({"analyze", path(file), "--loudness", "--truepeak", "--lra", "--json"});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(isSingleJsonLine(run));
        REQUIRE(run.has("integrated_lufs"));
        REQUIRE(run.has("true_peak_dbtp"));
        CHECK(run.json["integrated_lufs"].is_null());
        CHECK(run.json["true_peak_dbtp"].is_null());
    }

    //==========================================================================
    // run-commands: only undoable or readOnly commands

    TEST_CASE("run-commands refuses a command that is neither undoable nor readOnly before running anything")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir(), "Muster");
        const auto before = readText(fs::path(path(project)));
        const auto out = dir.dir().getChildFile("Kopie");
        const auto commands =
            writeCommands(dir.dir(), Json::array({step("test.note", {{"value", 1}}), step("project.save")}));
        bool batchRan = false;

        const auto run =
            runCli({"run-commands", path(project), path(commands), "--save-as", path(out)}, hooksWithNote(batchRan));

        CHECK(run.exitCode == 1);
        CHECK(isSingleJsonLine(run));
        CHECK_FALSE(run.ok());
        CHECK(run.errorCode() == "command_not_allowed");
        CHECK(run.integer("failed_index") == 1);
        CHECK_FALSE(run.errorMessage().empty());
        CHECK_FALSE(batchRan);  // checked up front: step 0 did not run either
        CHECK_FALSE(out.exists());
        CHECK(readText(fs::path(path(project))) == before);
    }

    TEST_CASE(
        "run-commands refuses every project.* command with a file effect and edit.undo, with the index of the step")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir(), "Muster");
        struct Case
        {
            const char* id;
            Json params;
        };
        const std::vector<Case> cases = {
            {"project.save", Json::object()},
            {"project.save_as", {{"folder", path(dir.dir())}, {"name", "Andere"}}},
            {"project.open", {{"path", path(project)}}},
            {"project.new", {{"folder", path(dir.dir())}, {"name", "Neu"}}},
            {"project.close", Json::object()},
            {"edit.undo", Json::object()},
        };
        for (const auto& c : cases)
        {
            CAPTURE(c.id);
            const auto commands = writeCommands(dir.dir(), Json::array({step("app.version"), step(c.id, c.params)}));

            const auto run = runCli({"run-commands", path(project), path(commands)});

            CHECK(run.exitCode == 1);
            CHECK(run.errorCode() == "command_not_allowed");
            CHECK(run.integer("failed_index") == 1);
        }
        CHECK_FALSE(dir.dir().getChildFile("Andere").exists());
        CHECK_FALSE(dir.dir().getChildFile("Neu").exists());
    }

    TEST_CASE("run-commands still runs undoable and readOnly commands")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir(), "Muster");
        const auto commands =
            writeCommands(dir.dir(), Json::array({step("app.version"), step("project.get_info"),
                                                  step("edit.get_undo_state"), step("test.note", {{"value", 5}})}));
        bool batchRan = false;

        const auto run = runCli({"run-commands", path(project), path(commands)}, hooksWithNote(batchRan));

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(batchRan);
    }
}
