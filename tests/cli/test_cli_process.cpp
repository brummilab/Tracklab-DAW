// The real executable `tracklab-cli` (src/cli/main.cpp), started as a process: exit codes of the operating system,
// stdout carries exactly one JSON line. The behaviour itself is tested in-process in the other files of this folder;
// these tests cover what only a process can show (main() wiring, process exit status, stdout/stderr separation).
#include "cli/cli_test_support.h"

using namespace tracklab_test::cli;

TEST_SUITE("cli")
{
    TEST_CASE("process: an unknown command exits with status 2 and prints one JSON error line")
    {
        const auto run = runCliProcess({"frobnicate"});

        CHECK(run.exitCode == 2);
        CHECK(isSingleJsonLine(run));
        CHECK_FALSE(run.ok());
        CHECK(run.errorCode() == "usage");
    }

    TEST_CASE("process: analyze of a synthetic sine exits with status 0 and prints the loudness as JSON")
    {
        ScopedTempDir dir;
        const auto file = dir.dir().getChildFile("sine-20.wav");
        writeWav(file, makeSine(48000.0, 2, 997.0, {{10.0, 0.1}}), 48000.0, 24);

        const auto run = runCliProcess({"analyze", path(file), "--loudness", "--truepeak", "--lra", "--json"});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(isSingleJsonLine(run));
        CHECK(std::abs(run.number("integrated_lufs") - -20.0) <= 0.1);
        CHECK(std::abs(run.number("true_peak_dbtp") - -20.0) <= 0.1);
    }

    TEST_CASE("process: analyze of a missing file exits with status 1")
    {
        ScopedTempDir dir;

        const auto run = runCliProcess({"analyze", path(dir.dir().getChildFile("missing.wav")), "--loudness"});

        CHECK(run.exitCode == 1);
        CHECK(isSingleJsonLine(run));
        CHECK_FALSE(run.ok());
    }

    TEST_CASE("process: export-tools writes files identical to the checked-in ones")
    {
        ScopedTempDir dir;
        const auto tools = dir.dir().getChildFile("tools.json");
        const auto docs = dir.dir().getChildFile("docs/commands.md");

        const auto run = runCliProcess({"export-tools", "--out", path(tools), "--docs", path(docs)});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        const fs::path root = TRACKLAB_SOURCE_DIR;
        CHECK(readText(fs::path(path(tools))) == readText(root / "tools.json"));
        CHECK(readText(fs::path(path(docs))) == readText(root / "docs" / "commands.md"));
    }

    TEST_CASE("process: run-commands then render of the saved project works across processes")
    {
        ScopedTempDir dir;
        const auto project = buildMiniProject(dir.dir(), "Muster");
        const auto commands = writeCommands(dir.dir(), Json::array({Json{{"id", "app.version"}}}));
        const auto saved = dir.dir().getChildFile("Kopie");
        const auto rendered = dir.dir().getChildFile("render.wav");

        const auto edit = runCliProcess({"run-commands", path(project), path(commands), "--save-as", path(saved)});
        REQUIRE_MESSAGE(edit.ok(), edit.out);
        CHECK(edit.exitCode == 0);

        const auto render = runCliProcess({"render", edit.text("saved"), "--out", path(rendered)});
        CHECK(render.exitCode == 0);
        REQUIRE_MESSAGE(render.ok(), render.out);
        CHECK(render.integer("length_samples") == 144000);
        CHECK(rendered.existsAsFile());
    }
}
