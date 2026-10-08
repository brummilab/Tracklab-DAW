// Brief M0-06, Minimal-Ziel 4 (load-vst3) and Bewertung "VST3": scan SpikeGain.vst3, load it, render one block
// and check that the level changes by the expected value (+-0.01 dB).
//
// Test signal (spike_common.h): 1 kHz sine, amplitude 0.5, 48 kHz, one block of 4800 frames = 100 periods,
// so its RMS is exactly 0.5/sqrt(2) = -9.0309 dBFS. SpikeGain applies its gain from the first sample (no
// smoothing); default -6.0 dB, parameter "gainDb" in -24..+12 dB.
#include <fstream>

#include "test_support.h"

using namespace spike_test;

namespace
{
const double kInputRmsDb = 20.0 * std::log10(spike::kVst3TestAmplitude / std::sqrt(2.0));  // -9.0309 dB

std::filesystem::path bundle()
{
    return spikeGainBundle();
}
}  // namespace

TEST_SUITE("vst3")
{
    TEST_CASE("the SpikeGain.vst3 bundle was built and its path is known to the tests")
    {
        REQUIRE_MESSAGE(!bundle().empty(), "pass --spike-gain-vst3=<path> or set SPIKE_GAIN_VST3");
        INFO("bundle: " << bundle().string());
        CHECK(std::filesystem::exists(bundle()));
    }

    TEST_CASE("load-vst3 scans the bundle, loads SpikeGain and renders one block")
    {
        const auto r = spike::loadVst3(bundle());

        REQUIRE_MESSAGE(r.ok, r.error);
        CHECK(r.numScanned >= 1);
        CHECK(r.pluginName == "SpikeGain");
        CHECK(r.rendered);
    }

    TEST_CASE("load-vst3 feeds the documented test signal (input RMS -9.03 dBFS)")
    {
        const auto r = spike::loadVst3(bundle());

        REQUIRE_MESSAGE(r.ok, r.error);
        SPIKE_CHECK_NEAR(r.inputRmsDb, kInputRmsDb, 0.01);
    }

    TEST_CASE("SpikeGain with its default setting lowers the level by 6.0 dB +-0.01 dB")
    {
        const auto r = spike::loadVst3(bundle());

        REQUIRE_MESSAGE(r.ok, r.error);
        SPIKE_CHECK_NEAR(r.gainDb, spike::kSpikeGainDefaultDb, 0.01);
        SPIKE_CHECK_NEAR(r.outputRmsDb - r.inputRmsDb, spike::kSpikeGainDefaultDb, 0.01);
        SPIKE_CHECK_NEAR(r.outputRmsDb, kInputRmsDb + spike::kSpikeGainDefaultDb, 0.01);
    }

    TEST_CASE("SpikeGain with gainDb = +3 raises the level by 3.0 dB +-0.01 dB")
    {
        const auto r = spike::loadVst3(bundle(), 3.0);

        REQUIRE_MESSAGE(r.ok, r.error);
        SPIKE_CHECK_NEAR(r.gainDb, 3.0, 0.01);
        SPIKE_CHECK_NEAR(r.outputRmsDb, kInputRmsDb + 3.0, 0.01);
    }

    TEST_CASE("SpikeGain with gainDb = -12 lowers the level by 12.0 dB +-0.01 dB")
    {
        const auto r = spike::loadVst3(bundle(), -12.0);

        REQUIRE_MESSAGE(r.ok, r.error);
        SPIKE_CHECK_NEAR(r.gainDb, -12.0, 0.01);
    }

    TEST_CASE("SpikeGain with gainDb = 0 leaves the level unchanged")
    {
        const auto r = spike::loadVst3(bundle(), 0.0);

        REQUIRE_MESSAGE(r.ok, r.error);
        SPIKE_CHECK_NEAR(r.gainDb, 0.0, 0.01);
    }

    TEST_CASE("load-vst3 fails with a message if the bundle does not exist")
    {
        TempDir dir;

        const auto r = spike::loadVst3(dir.file("Missing.vst3"));

        CHECK_FALSE(r.ok);
        SPIKE_REQUIRE_IMPLEMENTED(r);
        CHECK_FALSE(r.error.empty());
        CHECK_FALSE(r.rendered);
    }

    TEST_CASE("load-vst3 fails with a message if the path is not a VST3 bundle")
    {
        TempDir dir;
        const auto file = dir.file("NotAPlugin.vst3");
        {
            std::ofstream out(file, std::ios::binary);
            out << "not a plugin";
        }

        const auto r = spike::loadVst3(file);

        CHECK_FALSE(r.ok);
        SPIKE_REQUIRE_IMPLEMENTED(r);
        CHECK_FALSE(r.error.empty());
        CHECK_FALSE(r.rendered);
    }
}
