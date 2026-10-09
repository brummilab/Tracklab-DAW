// tracklab-cli render <project> --out <file> [--format wav24] (cli.h).
#include "cli/cli_test_support.h"

using namespace tracklab_test::cli;

TEST_SUITE("cli")
{
    TEST_CASE("render writes a 48 kHz / 24 bit stereo WAV as long as the project and prints its format as JSON")
    {
        ScopedTempDir dir;
        const auto project = buildMiniProject(dir.dir());
        const auto out = dir.dir().getChildFile("render.wav");

        const auto run = runCli({"render", path(project), "--out", path(out)});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(isSingleJsonLine(run));
        CHECK(out.existsAsFile());
        CHECK(run.text("format") == "wav24");
        CHECK(fs::path(run.text("out")) == fs::path(path(out)));
        CHECK(run.number("sample_rate") == doctest::Approx(48000.0));
        CHECK(run.integer("channels") == 2);
        CHECK(run.integer("bits_per_sample") == 24);
        CHECK(run.integer("length_samples") == 144000);  // 0 s .. end of the last clip (3 s), no tail

        // The JSON tells the truth about the file.
        const auto audio = readAudio(out);
        REQUIRE(audio.valid);
        CHECK(audio.sampleRate == doctest::Approx(48000.0));
        CHECK(audio.channels == 2);
        CHECK(audio.bitsPerSample == 24);
        CHECK(audio.length == 144000);
        CHECK(peakDbfs(audio) > -40.0);  // the stems are in the render, not silence
    }

    TEST_CASE("render with --format wav24 is the same as without --format")
    {
        ScopedTempDir dir;
        const auto project = buildMiniProject(dir.dir());
        const auto plain = dir.dir().getChildFile("plain.wav");
        const auto explicitFormat = dir.dir().getChildFile("explicit.wav");

        const auto a = runCli({"render", path(project), "--out", path(plain)});
        const auto b = runCli({"render", path(project), "--out", path(explicitFormat), "--format", "wav24"});

        REQUIRE_MESSAGE(a.ok(), a.out);
        REQUIRE_MESSAGE(b.ok(), b.out);
        CHECK(residualDbfs(readAudio(plain), readAudio(explicitFormat)) < -90.0);
    }

    TEST_CASE("rendering twice gives the same signal (null test below -90 dBFS)")
    {
        ScopedTempDir dir;
        const auto project = buildMiniProject(dir.dir());
        const auto first = dir.dir().getChildFile("first.wav");
        const auto second = dir.dir().getChildFile("second.wav");

        REQUIRE(runCli({"render", path(project), "--out", path(first)}).ok());
        REQUIRE(runCli({"render", path(project), "--out", path(second)}).ok());

        CHECK(residualDbfs(readAudio(first), readAudio(second)) < -90.0);
    }

    TEST_CASE("render never changes the project file")
    {
        ScopedTempDir dir;
        const auto project = buildMiniProject(dir.dir());
        const auto before = readText(fs::path(path(project)));

        const auto run = runCli({"render", path(project), "--out", path(dir.dir().getChildFile("r.wav"))});

        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(readText(fs::path(path(project))) == before);
    }

    TEST_CASE("render of a missing project exits with code 1 and writes nothing")
    {
        ScopedTempDir dir;
        const auto out = dir.dir().getChildFile("r.wav");

        const auto run = runCli({"render", path(dir.dir().getChildFile("Nope/Nope.tracklab")), "--out", path(out)});

        CHECK(run.exitCode == 1);
        CHECK(isSingleJsonLine(run));
        CHECK_FALSE(run.ok());
        CHECK(run.errorCode() == "project_not_found");
        CHECK_FALSE(out.exists());
    }

    TEST_CASE("render of a corrupt project exits with code 1 (corrupt_project)")
    {
        ScopedTempDir dir;
        const auto project = dir.dir().getChildFile("Kaputt/Kaputt.tracklab");
        writeText(fs::path(path(project)), "this is not xml");
        const auto out = dir.dir().getChildFile("r.wav");

        const auto run = runCli({"render", path(project), "--out", path(out)});

        CHECK(run.exitCode == 1);
        CHECK(run.errorCode() == "corrupt_project");
        CHECK_FALSE(out.exists());
    }

    TEST_CASE("render of a project without any clip exits with code 1 and writes nothing")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir());
        const auto out = dir.dir().getChildFile("r.wav");

        const auto run = runCli({"render", path(project), "--out", path(out)});

        CHECK(run.exitCode == 1);
        CHECK(isSingleJsonLine(run));
        CHECK_FALSE(run.ok());
        CHECK_FALSE(run.errorMessage().empty());
        CHECK_FALSE(out.exists());
    }

    TEST_CASE("render without a project argument is a usage error (exit code 2)")
    {
        const auto run = runCli({"render"});

        CHECK(run.exitCode == 2);
        CHECK(isSingleJsonLine(run));
        CHECK(run.errorCode() == "usage");
    }

    TEST_CASE("render without --out is a usage error (exit code 2)")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir());

        const auto run = runCli({"render", path(project)});

        CHECK(run.exitCode == 2);
        CHECK(run.errorCode() == "usage");
    }

    TEST_CASE("render with --out but no value is a usage error (exit code 2)")
    {
        ScopedTempDir dir;
        const auto project = makeEmptyProject(dir.dir());

        const auto run = runCli({"render", path(project), "--out"});

        CHECK(run.exitCode == 2);
        CHECK(run.errorCode() == "usage");
    }

    TEST_CASE("render with an unknown --format is a usage error (exit code 2) and writes nothing")
    {
        ScopedTempDir dir;
        const auto project = buildMiniProject(dir.dir());
        const auto out = dir.dir().getChildFile("r.wav");

        const auto run = runCli({"render", path(project), "--out", path(out), "--format", "bogus"});

        CHECK(run.exitCode == 2);
        CHECK(run.errorCode() == "usage");
        CHECK_FALSE(out.exists());
    }
}
