// tracklab-cli, dispatch and usage errors (cli.h): exit code 2, one JSON line with {"ok":false,"error":{...}}.
#include "cli/cli_test_support.h"

using namespace tracklab_test::cli;

TEST_SUITE("cli")
{
    TEST_CASE("no arguments: usage error, exit code 2, one JSON error line")
    {
        const auto run = runCli({});

        CHECK(run.exitCode == 2);
        CHECK(isSingleJsonLine(run));
        CHECK_FALSE(run.ok());
        CHECK(run.errorCode() == "usage");
        CHECK_FALSE(run.errorMessage().empty());
    }

    TEST_CASE("an unknown command: usage error, exit code 2, one JSON error line")
    {
        const auto run = runCli({"frobnicate"});

        CHECK(run.exitCode == 2);
        CHECK(isSingleJsonLine(run));
        CHECK_FALSE(run.ok());
        CHECK(run.errorCode() == "usage");
        CHECK_FALSE(run.errorMessage().empty());
    }

    TEST_CASE("an unknown option is a usage error for every command")
    {
        ScopedTempDir dir;
        for (const char* command : {"render", "analyze", "run-commands", "export-tools"})
        {
            CAPTURE(command);
            const auto run = runCli({command, "--no-such-option"});

            CHECK(run.exitCode == 2);
            CHECK(isSingleJsonLine(run));
            CHECK(run.errorCode() == "usage");
        }
    }

    TEST_CASE("--engine-temp-dir without a value is a usage error")
    {
        std::ostringstream out;
        std::ostringstream err;
        CliRun run;
        run.exitCode =
            tracklab::cli::runCli({"--engine-temp-dir"}, out, err);  // the helper would add the option itself
        run.out = out.str();
        finish(run);

        CHECK(run.exitCode == 2);
        CHECK(isSingleJsonLine(run));
        CHECK(run.errorCode() == "usage");
    }

    TEST_CASE("diagnostics go to stderr, never to stdout: stdout is exactly one JSON line")
    {
        // The usage error text is for people: it may be on stderr, but stdout stays machine-readable.
        const auto run = runCli({"frobnicate"});

        CHECK(isSingleJsonLine(run));
    }
}
