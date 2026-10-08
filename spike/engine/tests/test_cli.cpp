// The spike_cli contract (spike_common.h, runCli): one JSON object per call, exit codes 0 / 1 / 2, JSON keys of
// the four Minimal-Ziele of the brief (import, render-region, record-12, load-vst3) plus dump-pcm.
#include <algorithm>

#include "test_support.h"

using namespace spike_test;

namespace
{
const double kAmp20 = 0.1;

/** Output must be exactly one line of JSON terminated by '\n'. */
bool isSingleJsonLine(const CliRun& run)
{
    return !run.out.empty() && run.out.back() == '\n' && std::count(run.out.begin(), run.out.end(), '\n') == 1 &&
           run.json.isObject();
}

std::filesystem::path makeStereoWav(const TempDir& dir, const char* name, double seconds, double amplitude)
{
    const auto file = dir.file(name);
    writeWav(file, makeSine(48000.0, 2, 997.0, {{seconds, amplitude}}), 48000.0, 24);
    return file;
}
}  // namespace

TEST_SUITE("cli")
{
    TEST_CASE("an unknown command exits with code 2 and prints a JSON error")
    {
        const auto run = runCli({"frobnicate"});

        CHECK(run.exitCode == 2);
        CHECK(isSingleJsonLine(run));
        CHECK_FALSE(run.ok());
        CHECK(run.text("error") != spike::kNotImplemented);
        CHECK_FALSE(run.text("error").empty());
    }

    TEST_CASE("no arguments exit with code 2 and print a JSON error")
    {
        const auto run = runCli({});

        CHECK(run.exitCode == 2);
        CHECK(isSingleJsonLine(run));
        CHECK_FALSE(run.ok());
        CHECK(run.text("error") != spike::kNotImplemented);
    }

    TEST_CASE("import prints sample rate, channels and length as one JSON line")
    {
        TempDir dir;
        const auto file = makeStereoWav(dir, "in.wav", 2.0, kAmp20);

        const auto run = runCli({"import", file.string()});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(isSingleJsonLine(run));
        SPIKE_CHECK_NEAR(run.number("sample_rate"), 48000.0, 0.5);
        CHECK(static_cast<int>(run.get("channels")) == 2);
        CHECK(run.integer("length_samples") == 96000);
    }

    TEST_CASE("import of a missing file exits with code 1 and prints a JSON error")
    {
        TempDir dir;

        const auto run = runCli({"import", dir.file("missing.wav").string()});

        CHECK(run.exitCode == 1);
        CHECK(isSingleJsonLine(run));
        CHECK_FALSE(run.ok());
        CHECK(run.text("error") != spike::kNotImplemented);
        CHECK_FALSE(run.text("error").empty());
    }

    TEST_CASE("import without a file argument is a usage error (exit code 2)")
    {
        const auto run = runCli({"import"});

        CHECK(run.exitCode == 2);
        CHECK(isSingleJsonLine(run));
        CHECK_FALSE(run.ok());
        CHECK(run.text("error") != spike::kNotImplemented);
    }

    TEST_CASE("render-region prints format, exact length and the three loudness values")
    {
        TempDir dir;
        const auto file = makeStereoWav(dir, "in.wav", 10.0, kAmp20);
        const auto out = dir.file("region.wav");

        const auto run =
            runCli({"render-region", file.string(), "--start", "2", "--end", "8.5", "--out", out.string()});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(isSingleJsonLine(run));
        CHECK(std::filesystem::exists(out));
        SPIKE_CHECK_NEAR(run.number("sample_rate"), 48000.0, 0.5);
        CHECK(static_cast<int>(run.get("channels")) == 2);
        CHECK(static_cast<int>(run.get("bits_per_sample")) == 24);
        CHECK(run.integer("length_samples") == 312000);  // 6.5 s
        SPIKE_CHECK_NEAR(run.number("integrated_lufs"), -20.0, 0.1);
        SPIKE_CHECK_NEAR(run.number("true_peak_dbtp"), -20.0, 0.1);
        CHECK(run.has("lra"));
    }

    TEST_CASE("render-region without --out is a usage error (exit code 2)")
    {
        TempDir dir;
        const auto file = makeStereoWav(dir, "in.wav", 2.0, kAmp20);

        const auto run = runCli({"render-region", file.string(), "--start", "0", "--end", "1"});

        CHECK(run.exitCode == 2);
        CHECK(isSingleJsonLine(run));
        CHECK(run.text("error") != spike::kNotImplemented);
    }

    TEST_CASE("render-region with a non-numeric --start is a usage error (exit code 2)")
    {
        TempDir dir;
        const auto file = makeStereoWav(dir, "in.wav", 2.0, kAmp20);

        const auto run = runCli(
            {"render-region", file.string(), "--start", "abc", "--end", "1", "--out", dir.file("o.wav").string()});

        CHECK(run.exitCode == 2);
        CHECK(isSingleJsonLine(run));
        CHECK(run.text("error") != spike::kNotImplemented);
    }

    TEST_CASE("record-12 prints 12 inputs, 12 tracks, 12 files, no missing blocks and the deviation")
    {
        TempDir dir;

        const auto run = runCli({"record-12", "--seconds", "1", "--out-dir", dir.file("rec").string()});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(isSingleJsonLine(run));
        CHECK(static_cast<int>(run.get("inputs")) == 12);
        CHECK(static_cast<int>(run.get("tracks")) == 12);
        CHECK(run.get("files").size() == 12);
        CHECK(run.integer("length_samples") == 48000);
        CHECK(static_cast<int>(run.get("missing_blocks")) == 0);
        CHECK(run.number("worst_deviation_db") <= spike::kRecordToleranceDb);
    }

    TEST_CASE("record-12 without --seconds is a usage error (exit code 2)")
    {
        const auto run = runCli({"record-12"});

        CHECK(run.exitCode == 2);
        CHECK(isSingleJsonLine(run));
        CHECK(run.text("error") != spike::kNotImplemented);
    }

    TEST_CASE("load-vst3 prints plugin name, rendered flag and the measured gain")
    {
        REQUIRE_MESSAGE(!spikeGainBundle().empty(), "pass --spike-gain-vst3=<path> or set SPIKE_GAIN_VST3");

        const auto run = runCli({"load-vst3", spikeGainBundle().string(), "--gain-db", "3"});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(isSingleJsonLine(run));
        CHECK(run.text("plugin") == "SpikeGain");
        CHECK(static_cast<int>(run.get("scanned")) >= 1);
        CHECK(static_cast<bool>(run.get("rendered")));
        SPIKE_CHECK_NEAR(run.number("gain_db"), 3.0, 0.01);
        CHECK(run.has("input_rms_db"));
        CHECK(run.has("output_rms_db"));
    }

    TEST_CASE("load-vst3 without a bundle argument is a usage error (exit code 2)")
    {
        const auto run = runCli({"load-vst3"});

        CHECK(run.exitCode == 2);
        CHECK(isSingleJsonLine(run));
        CHECK(run.text("error") != spike::kNotImplemented);
    }

    TEST_CASE("dump-pcm prints the format of the decoded file and writes the raw samples")
    {
        TempDir dir;
        const auto file = makeStereoWav(dir, "in.wav", 1.0, kAmp20);
        const auto raw = dir.file("in.f32");

        const auto run = runCli({"dump-pcm", file.string(), "--out", raw.string()});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(isSingleJsonLine(run));
        CHECK(static_cast<int>(run.get("channels")) == 2);
        CHECK(run.integer("length_samples") == 48000);
        CHECK(std::filesystem::file_size(raw) == 48000u * 2u * sizeof(float));
    }

    TEST_CASE("the spike_cli executable prints the import JSON and exits with 0")
    {
        REQUIRE_MESSAGE(!cliExecutable().empty(), "pass --spike-cli=<path>");
        TempDir dir;
        const auto file = makeStereoWav(dir, "in.wav", 1.0, kAmp20);

        const auto run = runCliProcess({"import", file.string()});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(run.integer("length_samples") == 48000);
    }

    TEST_CASE("the spike_cli executable exits with 2 for an unknown command")
    {
        REQUIRE_MESSAGE(!cliExecutable().empty(), "pass --spike-cli=<path>");

        const auto run = runCliProcess({"frobnicate"});

        CHECK(run.exitCode == 2);
        CHECK_FALSE(run.ok());
        CHECK(run.text("error") != spike::kNotImplemented);
    }
}
