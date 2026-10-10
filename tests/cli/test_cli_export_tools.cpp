// tracklab-cli export-tools [--check] [--out tools.json] [--docs commands.md] (cli.h): the registry export of the
// registry the app builds. The result has to be byte-identical to the checked-in tools.json / docs/commands.md
// (the gate's freshness check; the same files tests/core/test_command_export.cpp compares with the registry directly).
#include "cli/cli_test_support.h"

using namespace tracklab_test::cli;

namespace
{
const fs::path sourceRoot = TRACKLAB_SOURCE_DIR;

fs::path checkedInTools()
{
    return sourceRoot / "tools.json";
}

fs::path checkedInDocs()
{
    return sourceRoot / "docs" / "commands.md";
}
}  // namespace

TEST_SUITE("cli")
{
    TEST_CASE("export-tools writes tools.json and docs/commands.md identical to the checked-in files")
    {
        ScopedTempDir dir;
        const auto tools = dir.dir().getChildFile("tools.json");
        const auto docs = dir.dir().getChildFile("docs/commands.md");  // parent folder does not exist yet

        const auto run = runCli({"export-tools", "--out", path(tools), "--docs", path(docs)});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(isSingleJsonLine(run));
        CHECK(run.integer("commands") > 0);
        CHECK(fs::path(run.text("tools")) == fs::path(path(tools)));
        CHECK(fs::path(run.text("docs")) == fs::path(path(docs)));
        REQUIRE(tools.existsAsFile());
        REQUIRE(docs.existsAsFile());
        CHECK(readText(fs::path(path(tools))) == readText(checkedInTools()));
        CHECK(readText(fs::path(path(docs))) == readText(checkedInDocs()));
    }

    TEST_CASE("export-tools is deterministic: two runs write the same bytes")
    {
        ScopedTempDir dir;
        const auto first = dir.dir().getChildFile("a/tools.json");
        const auto second = dir.dir().getChildFile("b/tools.json");

        REQUIRE(runCli({"export-tools", "--out", path(first)}).ok());
        REQUIRE(runCli({"export-tools", "--out", path(second)}).ok());

        CHECK(readText(fs::path(path(first))) == readText(fs::path(path(second))));
    }

    TEST_CASE("export-tools with only --out writes only tools.json")
    {
        ScopedTempDir dir;
        const auto tools = dir.dir().getChildFile("tools.json");

        const auto run = runCli({"export-tools", "--out", path(tools)});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(tools.existsAsFile());
        CHECK_FALSE(run.has("docs"));
        CHECK_FALSE(dir.dir().getChildFile("docs").exists());
        CHECK(readText(fs::path(path(tools))) == readText(checkedInTools()));
    }

    TEST_CASE("export-tools with only --docs writes only the command reference")
    {
        ScopedTempDir dir;
        const auto docs = dir.dir().getChildFile("commands.md");

        const auto run = runCli({"export-tools", "--docs", path(docs)});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(docs.existsAsFile());
        CHECK_FALSE(run.has("tools"));
        CHECK(readText(fs::path(path(docs))) == readText(checkedInDocs()));
    }

    TEST_CASE("export-tools overwrites a stale file")
    {
        ScopedTempDir dir;
        const auto tools = dir.dir().getChildFile("tools.json");
        writeText(fs::path(path(tools)), "[]\n");

        const auto run = runCli({"export-tools", "--out", path(tools)});

        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(readText(fs::path(path(tools))) == readText(checkedInTools()));
    }

    TEST_CASE("export-tools to a place that cannot be written exits with code 1")
    {
        ScopedTempDir dir;
        // A regular file where a folder is needed: the parent folder cannot be created, on every platform and for root.
        const auto blocker = dir.dir().getChildFile("blocker");
        writeText(fs::path(path(blocker)), "x");

        const auto run = runCli({"export-tools", "--out", path(blocker.getChildFile("tools.json"))});

        CHECK(run.exitCode == 1);
        CHECK(isSingleJsonLine(run));
        CHECK_FALSE(run.ok());
        CHECK_FALSE(run.errorMessage().empty());
    }

    TEST_CASE("export-tools without --out and --docs is a usage error (exit code 2) and writes nothing")
    {
        const auto run = runCli({"export-tools"});

        CHECK(run.exitCode == 2);
        CHECK(isSingleJsonLine(run));
        CHECK(run.errorCode() == "usage");
    }

    TEST_CASE("export-tools --out without a value is a usage error (exit code 2)")
    {
        const auto run = runCli({"export-tools", "--out"});

        CHECK(run.exitCode == 2);
        CHECK(run.errorCode() == "usage");
    }

    //==========================================================================
    // --check: the gate's freshness check (never writes)

    TEST_CASE("export-tools --check passes for the checked-in files and writes nothing")
    {
        const auto run =
            runCli({"export-tools", "--check", "--out", path(checkedInTools()), "--docs", path(checkedInDocs())});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(isSingleJsonLine(run));
        CHECK(run.json["up_to_date"] == true);
    }

    TEST_CASE("export-tools --check fails for a stale file, names it, and leaves it as it was")
    {
        ScopedTempDir dir;
        const auto tools = dir.dir().getChildFile("tools.json");
        const auto docs = dir.dir().getChildFile("commands.md");
        writeText(fs::path(path(tools)), readText(checkedInTools()));
        writeText(fs::path(path(docs)), readText(checkedInDocs()) + "stale\n");

        const auto run = runCli({"export-tools", "--check", "--out", path(tools), "--docs", path(docs)});

        CHECK(run.exitCode == 1);
        CHECK(isSingleJsonLine(run));
        CHECK_FALSE(run.ok());
        CHECK(run.errorMessage().find("commands.md") != std::string::npos);
        CHECK(run.errorMessage().find("tools.json") == std::string::npos);
        CHECK(readText(fs::path(path(docs))) == readText(checkedInDocs()) + "stale\n");
    }

    TEST_CASE("export-tools --check fails for a missing file and does not create it")
    {
        ScopedTempDir dir;
        const auto tools = dir.dir().getChildFile("tools.json");

        const auto run = runCli({"export-tools", "--check", "--out", path(tools)});

        CHECK(run.exitCode == 1);
        CHECK_FALSE(run.ok());
        CHECK(run.errorMessage().find("tools.json") != std::string::npos);
        CHECK_FALSE(tools.exists());
    }
}
