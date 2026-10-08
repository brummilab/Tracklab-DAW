// Brief M0-06, Minimal-Ziel 3 (record-12) and Bewertung "12 Eingaenge": 12 inputs of a hosted audio device,
// 12 tracks, 12 recordings; comparison with the input signals <= -99 dB, no missing blocks (5 s, 48 kHz).
//
// The input signal of channel c is documented in spike_common.h (sine, amplitude 0.25, 200 + 110*c Hz, computed
// in double). The tests re-read the recorded files and verify them independently of the result's own numbers.
#include "test_support.h"

using namespace spike_test;

namespace
{
const double kToleranceAbs = std::pow(10.0, spike::kRecordToleranceDb / 20.0);  // -99 dB = 1.12e-5

/** Reference input signal of one channel as float, exactly as the device is fed. */
juce::AudioBuffer<float> referenceInput(int channel, int numFrames)
{
    juce::AudioBuffer<float> reference(1, numFrames);
    const double w = 2.0 * 3.14159265358979323846 * spike::recordInputFrequencyHz(channel) / spike::kRecordSampleRate;
    for (int n = 0; n < numFrames; ++n)
        reference.setSample(0, n,
                            static_cast<float>(spike::kRecordInputAmplitude * std::sin(w * static_cast<double>(n))));
    return reference;
}

/** Verifies the 12 recorded files against the reference signals. */
void checkRecordings(const spike::Record12Result& r, int expectedFrames)
{
    REQUIRE_MESSAGE(r.ok, r.error);
    REQUIRE(r.files.size() == 12u);
    for (int c = 0; c < 12; ++c)
    {
        INFO("input " << c << " file " << r.files[static_cast<std::size_t>(c)].string());
        const auto file = readAudio(r.files[static_cast<std::size_t>(c)]);
        REQUIRE_MESSAGE(file.ok, file.error);
        CHECK(file.numChannels == 1);
        CHECK(file.sampleRate == doctest::Approx(spike::kRecordSampleRate));
        REQUIRE(file.lengthSamples == expectedFrames);  // a missing block would shorten or shift the file
        const auto reference = referenceInput(c, expectedFrames);
        const double worst = maxAbsDiff(file.samples, 0, 0, reference, 0, 0, expectedFrames);
        INFO("worst deviation " << 20.0 * std::log10(std::max(worst, 1.0e-12)) << " dB");
        CHECK(worst <= kToleranceAbs);
    }
}
}  // namespace

TEST_SUITE("record12")
{
    TEST_CASE("record12 records 12 inputs for 5 s on 12 tracks into 12 mono files")
    {
        TempDir dir;

        const auto r = spike::record12(5.0, dir.file("rec"));

        REQUIRE_MESSAGE(r.ok, r.error);
        CHECK(r.numInputs == 12);
        CHECK(r.numTracks == 12);
        CHECK(r.files.size() == 12u);
        CHECK(r.lengthSamples == 240000);
    }

    TEST_CASE("record12 recordings equal the input signals within -99 dB, 5 s at 48 kHz, no missing blocks")
    {
        TempDir dir;

        const auto r = spike::record12(5.0, dir.file("rec"));

        checkRecordings(r, 240000);
    }

    TEST_CASE("record12 reports no missing blocks and a worst deviation of at most -99 dB")
    {
        TempDir dir;

        const auto r = spike::record12(5.0, dir.file("rec"));

        REQUIRE_MESSAGE(r.ok, r.error);
        CHECK(r.missingBlocks == 0);
        CHECK(r.worstDeviationDb <= spike::kRecordToleranceDb);
    }

    TEST_CASE("record12 keeps the exact length when the duration is not a multiple of the 512 frame block")
    {
        TempDir dir;

        const auto r = spike::record12(1.0, dir.file("rec"));  // 48000 frames = 93.75 blocks

        checkRecordings(r, 48000);
    }

    TEST_CASE("record12 writes one file per input in input order (no swapped channels)")
    {
        TempDir dir;

        const auto r = spike::record12(1.0, dir.file("rec"));

        REQUIRE_MESSAGE(r.ok, r.error);
        REQUIRE(r.files.size() == 12u);
        // Every file must match its own reference and NOT the reference of its neighbour (distinct frequencies).
        for (int c = 0; c < 11; ++c)
        {
            const auto file = readAudio(r.files[static_cast<std::size_t>(c)]);
            REQUIRE_MESSAGE(file.ok, file.error);
            const auto neighbour = referenceInput(c + 1, 48000);
            CHECK(maxAbsDiff(file.samples, 0, 0, neighbour, 0, 0, 48000) > 0.1);
        }
    }

    TEST_CASE("record12 fails with a message for a duration of zero or less")
    {
        TempDir dir;

        const auto zero = spike::record12(0.0, dir.file("zero"));
        const auto negative = spike::record12(-1.0, dir.file("negative"));

        CHECK_FALSE(zero.ok);
        SPIKE_REQUIRE_IMPLEMENTED(zero);
        CHECK_FALSE(negative.ok);
        SPIKE_REQUIRE_IMPLEMENTED(negative);
    }
}
