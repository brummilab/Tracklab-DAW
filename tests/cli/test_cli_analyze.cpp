// tracklab-cli analyze <file|project> --loudness --truepeak --lra --json (cli.h): values against analytically known
// synthetic signals (ITU-R BS.1770-4, EBU Tech 3341/3342; the same reference values as the engine spike's render tests).
//  - Stereo sine of 997 Hz with the same peak A on both channels: Integrated Loudness 20*log10(A) LUFS (K-weighting
//    gain at 997 Hz +0.691 dB, offset -0.691 dB), True Peak 20*log10(A) dBTP, LRA 0 LU.
//  - 30 s at -20 dBFS followed by 30 s at -30 dBFS: gated power mean 10*log10(0.5*0.01 + 0.5*0.001) = -22.60 LUFS,
//    short-term plateaus at -20 and -30 LUFS: LRA = 10 LU.
//  - 8 kHz sine (fs/6) with a start phase of 60 degrees, A = 0.5, 10 ms fades: samples are only 0 and +-A*sin(60 deg),
//    the continuous waveform reaches A: True Peak 20*log10(A) = -6.02 dBTP.
#include "cli/cli_test_support.h"

using namespace tracklab_test::cli;

namespace
{
constexpr double freq = 997.0;
const double amp20 = 0.1;                   // -20 dBFS peak
const double amp30 = 0.031622776601683794;  // -30 dBFS peak

#define CLI_CHECK_NEAR(actual, expected, tolerance) CHECK(std::abs((actual) - (expected)) <= (tolerance))

juce::File stereoWav(const ScopedTempDir& dir, const char* name, const std::vector<Segment>& segments)
{
    const auto file = dir.dir().getChildFile(name);
    writeWav(file, makeSine(48000.0, 2, freq, segments), 48000.0, 24);
    return file;
}
}  // namespace

TEST_SUITE("cli")
{
    TEST_CASE("analyze: a -20 dBFS stereo sine measures -20 LUFS, -20 dBTP and 0 LU (tolerance 0.1)")
    {
        ScopedTempDir dir;
        const auto file = stereoWav(dir, "sine-20.wav", {{20.0, amp20}});

        const auto run = runCli({"analyze", path(file), "--loudness", "--truepeak", "--lra", "--json"});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(isSingleJsonLine(run));
        CLI_CHECK_NEAR(run.number("integrated_lufs"), -20.0, 0.1);
        CLI_CHECK_NEAR(run.number("true_peak_dbtp"), -20.0, 0.1);
        CLI_CHECK_NEAR(run.number("lra"), 0.0, 0.1);
        CHECK(fs::path(run.text("source")) == fs::path(path(file)));
    }

    TEST_CASE("analyze: -30 dBFS measures 10 LU lower (the level scale is linear)")
    {
        ScopedTempDir dir;
        const auto file = stereoWav(dir, "sine-30.wav", {{20.0, amp30}});

        const auto run = runCli({"analyze", path(file), "--loudness", "--truepeak", "--json"});

        REQUIRE_MESSAGE(run.ok(), run.out);
        CLI_CHECK_NEAR(run.number("integrated_lufs"), -30.0, 0.1);
        CLI_CHECK_NEAR(run.number("true_peak_dbtp"), -30.0, 0.1);
    }

    TEST_CASE("analyze: 30 s at -20 dBFS then 30 s at -30 dBFS has LRA 10 LU and Integrated -22.6 LUFS")
    {
        ScopedTempDir dir;
        const auto file = stereoWav(dir, "two-level.wav", {{30.0, amp20}, {30.0, amp30}});

        const auto run = runCli({"analyze", path(file), "--loudness", "--lra", "--json"});

        REQUIRE_MESSAGE(run.ok(), run.out);
        CLI_CHECK_NEAR(run.number("lra"), 10.0, 0.3);
        CLI_CHECK_NEAR(run.number("integrated_lufs"), 10.0 * std::log10(0.5 * 0.01 + 0.5 * 0.001), 0.1);
    }

    TEST_CASE("analyze: the true peak of an fs/6 sine is -6.02 dBTP although its sample peak is 1.25 dB lower")
    {
        ScopedTempDir dir;
        auto signal = makeSine(48000.0, 2, 8000.0, {{5.0, 0.5}}, pi / 3.0);
        signal.applyGainRamp(0, 480, 0.0f, 1.0f);  // 10 ms fades: a hard edge would overshoot (Gibbs)
        signal.applyGainRamp(signal.getNumSamples() - 480, 480, 1.0f, 0.0f);
        const auto file = dir.dir().getChildFile("fs6.wav");
        writeWav(file, signal, 48000.0, 24);

        const auto run = runCli({"analyze", path(file), "--truepeak", "--json"});

        REQUIRE_MESSAGE(run.ok(), run.out);
        CLI_CHECK_NEAR(run.number("true_peak_dbtp"), amplitudeToDb(0.5), 0.1);
    }

    TEST_CASE("analyze: only the requested measurements are in the result")
    {
        ScopedTempDir dir;
        const auto file = stereoWav(dir, "sine-20.wav", {{10.0, amp20}});

        const auto loudnessOnly = runCli({"analyze", path(file), "--loudness", "--json"});
        const auto peakOnly = runCli({"analyze", path(file), "--truepeak", "--json"});
        const auto lraOnly = runCli({"analyze", path(file), "--lra", "--json"});

        REQUIRE_MESSAGE(loudnessOnly.ok(), loudnessOnly.out);
        CHECK(loudnessOnly.has("integrated_lufs"));
        CHECK_FALSE(loudnessOnly.has("true_peak_dbtp"));
        CHECK_FALSE(loudnessOnly.has("lra"));

        REQUIRE_MESSAGE(peakOnly.ok(), peakOnly.out);
        CHECK(peakOnly.has("true_peak_dbtp"));
        CHECK_FALSE(peakOnly.has("integrated_lufs"));
        CHECK_FALSE(peakOnly.has("lra"));

        REQUIRE_MESSAGE(lraOnly.ok(), lraOnly.out);
        CHECK(lraOnly.has("lra"));
        CHECK_FALSE(lraOnly.has("integrated_lufs"));
        CHECK_FALSE(lraOnly.has("true_peak_dbtp"));
    }

    TEST_CASE("analyze: --json is accepted but the output is JSON either way")
    {
        ScopedTempDir dir;
        const auto file = stereoWav(dir, "sine-20.wav", {{10.0, amp20}});

        const auto withFlag = runCli({"analyze", path(file), "--loudness", "--json"});
        const auto withoutFlag = runCli({"analyze", path(file), "--loudness"});

        CHECK(withFlag.exitCode == 0);
        CHECK(withoutFlag.exitCode == 0);
        CHECK(isSingleJsonLine(withFlag));
        CHECK(isSingleJsonLine(withoutFlag));
    }

    TEST_CASE("analyze of a project measures its render (same values as analyzing the rendered file)")
    {
        ScopedTempDir dir;
        const auto project = buildMiniProject(dir.dir());
        const auto rendered = dir.dir().getChildFile("render.wav");
        REQUIRE(runCli({"render", path(project), "--out", path(rendered)}).ok());

        const auto ofProject = runCli({"analyze", path(project), "--loudness", "--truepeak", "--lra", "--json"});
        const auto ofFile = runCli({"analyze", path(rendered), "--loudness", "--truepeak", "--lra", "--json"});

        CHECK(ofProject.exitCode == 0);
        REQUIRE_MESSAGE(ofProject.ok(), ofProject.out);
        REQUIRE_MESSAGE(ofFile.ok(), ofFile.out);
        CLI_CHECK_NEAR(ofProject.number("integrated_lufs"), ofFile.number("integrated_lufs"), 0.05);
        CLI_CHECK_NEAR(ofProject.number("true_peak_dbtp"), ofFile.number("true_peak_dbtp"), 0.05);
        CLI_CHECK_NEAR(ofProject.number("lra"), ofFile.number("lra"), 0.05);
    }

    TEST_CASE("analyze of a missing file exits with code 1 and prints a JSON error")
    {
        ScopedTempDir dir;

        const auto run = runCli({"analyze", path(dir.dir().getChildFile("missing.wav")), "--loudness", "--json"});

        CHECK(run.exitCode == 1);
        CHECK(isSingleJsonLine(run));
        CHECK_FALSE(run.ok());
        CHECK_FALSE(run.errorCode().empty());
        CHECK_FALSE(run.errorMessage().empty());
    }

    TEST_CASE("analyze of a file that is not audio exits with code 1")
    {
        ScopedTempDir dir;
        const auto file = dir.dir().getChildFile("notes.wav");
        writeText(fs::path(path(file)), "this is not a WAV file");

        const auto run = runCli({"analyze", path(file), "--loudness", "--json"});

        CHECK(run.exitCode == 1);
        CHECK(isSingleJsonLine(run));
        CHECK_FALSE(run.ok());
    }

    TEST_CASE("analyze of a missing project exits with code 1 (project_not_found)")
    {
        ScopedTempDir dir;

        const auto run =
            runCli({"analyze", path(dir.dir().getChildFile("Nope/Nope.tracklab")), "--loudness", "--json"});

        CHECK(run.exitCode == 1);
        CHECK(run.errorCode() == "project_not_found");
    }

    TEST_CASE("analyze without a file argument is a usage error (exit code 2)")
    {
        const auto run = runCli({"analyze", "--loudness", "--json"});

        CHECK(run.exitCode == 2);
        CHECK(isSingleJsonLine(run));
        CHECK(run.errorCode() == "usage");
    }

    TEST_CASE("analyze with two file arguments is a usage error (exit code 2)")
    {
        ScopedTempDir dir;

        const auto run = runCli({"analyze", path(dir.dir().getChildFile("a.wav")),
                                 path(dir.dir().getChildFile("b.wav")), "--loudness", "--json"});

        CHECK(run.exitCode == 2);
        CHECK(run.errorCode() == "usage");
    }
}
