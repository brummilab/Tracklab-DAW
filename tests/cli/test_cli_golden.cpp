// Golden null test of the m1-mini fixture (M1-07, DESIGN section 8 "Golden-Render"):
//   fixture project (generated in the test: sines and fixed-seed noise, see buildMiniProject)
//   -> tracklab-cli run-commands (--save-as)  -> tracklab-cli render  -> compared with tests/fixtures/m1-mini/golden.wav.
// The test passes when the residual is below -90 dBFS (sample-wise difference, so every deviation counts).
//
// golden.wav is synthetic (no recording) and is only ever changed through the CMake target tracklab_update_golden
// (see tests/CMakeLists.txt), which needs a reason:
//   TRACKLAB_GOLDEN_REASON="why the render legitimately changed" cmake --build <dir> --target tracklab_update_golden
// The reason is appended to tests/fixtures/m1-mini/golden-update.log. A plain test run never writes the golden file.
//
// TODO (open question in the report of M1-07): as soon as track.* / clip.* / audio.import commands exist, the fixture is
// built completely by the commands.json given to run-commands. Today only project.* / edit.* / app.* / io.* exist, so
// buildMiniProject inserts the two clips with the Tracktion API and run-commands carries the project through a
// --save-as round trip.
#include "cli/cli_test_support.h"

#include <cstdlib>

using namespace tracklab_test::cli;

namespace
{
const fs::path fixtureDir = fs::path(TRACKLAB_SOURCE_DIR) / "tests" / "fixtures" / "m1-mini";
const fs::path goldenPath = fixtureDir / "golden.wav";
constexpr double nullThresholdDbfs = -90.0;

std::string env(const char* name)
{
    const char* value = std::getenv(name);
    return value != nullptr ? value : "";
}

/** Builds the fixture and renders it through the CLI; returns the rendered file. */
juce::File renderFixtureThroughCli(const ScopedTempDir& dir)
{
    const auto base = buildMiniProject(dir.dir(), "Muster");
    const auto commands = writeCommands(dir.dir(), Json::array({Json{{"id", "project.get_info"}}}));
    const auto built = dir.dir().getChildFile("Gebaut");

    const auto edit = runCli({"run-commands", path(base), path(commands), "--save-as", path(built)});
    REQUIRE_MESSAGE(edit.ok(), edit.out);

    const auto rendered = dir.dir().getChildFile("render.wav");
    const auto render = runCli({"render", edit.text("saved"), "--out", path(rendered)});
    REQUIRE_MESSAGE(render.ok(), render.out);
    return rendered;
}
}  // namespace

TEST_SUITE("cli_golden")
{
    TEST_CASE("the null-test comparison itself: it sees a 0.01 dB gain change and ignores nothing")
    {
        // Guards the guard: a comparison that cannot fail would make the golden test worthless.
        auto signal = makeSine(48000.0, 2, 440.0, {{1.0, 0.25}});
        auto louder = juce::AudioBuffer<float>(signal);
        louder.applyGain(1.0012f);  // +0.01 dB: residual 0.25 * 0.0012 = 0.0003 = -70 dBFS
        AudioData a, b;
        a.valid = b.valid = true;
        a.channels = b.channels = 2;
        a.length = b.length = signal.getNumSamples();
        a.samples = signal;
        b.samples = louder;

        CHECK(residualDbfs(a, a) <= -200.0);
        CHECK(residualDbfs(a, b) > nullThresholdDbfs);
        CHECK(residualDbfs(a, b) < -60.0);

        b.length = a.length - 1;  // different length: never a null
        CHECK(residualDbfs(a, b) >= 0.0);
    }

    TEST_CASE("m1-mini: the render of the project built through run-commands nulls against golden.wav (< -90 dBFS)")
    {
        ScopedTempDir dir;
        const auto rendered = renderFixtureThroughCli(dir);

        const bool update = env("TRACKLAB_UPDATE_GOLDEN") == "1";
        if (update)
        {
            const auto reason = env("TRACKLAB_GOLDEN_REASON");
            REQUIRE_MESSAGE(!reason.empty(), "golden update needs TRACKLAB_GOLDEN_REASON=\"why the render changed\"");
            fs::create_directories(fixtureDir);
            fs::copy_file(fs::path(path(rendered)), goldenPath, fs::copy_options::overwrite_existing);
            std::ofstream log(fixtureDir / "golden-update.log", std::ios::app);
            log << reason << "\n";
        }

        INFO("golden file: " << goldenPath.string()
                             << " -- missing? created only by: TRACKLAB_GOLDEN_REASON=... cmake --build <dir> --target "
                                "tracklab_update_golden");
        REQUIRE(fs::exists(goldenPath));
        const auto golden = readAudio(juce::File(goldenPath.string()));
        const auto actual = readAudio(rendered);
        REQUIRE(golden.valid);
        REQUIRE(actual.valid);

        // The golden file is what it claims to be: a 3 s, 48 kHz, 24 bit stereo render that is not silence.
        CHECK(golden.sampleRate == doctest::Approx(48000.0));
        CHECK(golden.channels == 2);
        CHECK(golden.bitsPerSample == 24);
        CHECK(golden.length == 144000);
        CHECK(peakDbfs(golden) > -40.0);

        CHECK(actual.length == golden.length);
        CHECK(residualDbfs(golden, actual) < nullThresholdDbfs);
    }

    TEST_CASE("m1-mini: the base project (before run-commands) renders to the same signal as golden.wav")
    {
        ScopedTempDir dir;
        const auto base = buildMiniProject(dir.dir(), "Muster");
        const auto rendered = dir.dir().getChildFile("render.wav");

        const auto render = runCli({"render", path(base), "--out", path(rendered)});

        REQUIRE_MESSAGE(render.ok(), render.out);
        REQUIRE(fs::exists(goldenPath));
        CHECK(residualDbfs(readAudio(juce::File(goldenPath.string())), readAudio(rendered)) < nullThresholdDbfs);
    }
}
