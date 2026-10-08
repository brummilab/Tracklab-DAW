// Brief M0-06, Minimal-Ziel 1 (import): sample rate, channels and length of WAV files.
// MP3 import needs `lame` and lives in test_mp3.cpp (suite "mp3").
#include <fstream>

#include "test_support.h"

using namespace spike_test;

TEST_SUITE("import")
{
    TEST_CASE("import reports sample rate, channels and length of a 44.1 kHz 16 bit stereo WAV")
    {
        TempDir dir;
        const auto file = dir.file("stereo-44k1.wav");
        writeWav(file, makeSine(44100.0, 2, 440.0, {{2.5, 0.5}}), 44100.0, 16);

        const auto result = spike::importFile(file);

        REQUIRE_MESSAGE(result.ok, result.error);
        CHECK(result.sampleRate == doctest::Approx(44100.0));
        CHECK(result.numChannels == 2);
        CHECK(result.lengthSamples == 110250);
    }

    TEST_CASE("import reports sample rate, channels and length of a 48 kHz 24 bit stereo WAV")
    {
        TempDir dir;
        const auto file = dir.file("stereo-48k.wav");
        writeWav(file, makeSine(48000.0, 2, 997.0, {{3.0, 0.1}}), 48000.0, 24);

        const auto result = spike::importFile(file);

        REQUIRE_MESSAGE(result.ok, result.error);
        CHECK(result.sampleRate == doctest::Approx(48000.0));
        CHECK(result.numChannels == 2);
        CHECK(result.lengthSamples == 144000);
    }

    TEST_CASE("import reports channels and length of a 96 kHz 32 bit float mono WAV")
    {
        TempDir dir;
        const auto file = dir.file("mono-96k.wav");
        writeWav(file, makeSine(96000.0, 1, 1000.0, {{0.5, 0.25}}), 96000.0, 32);

        const auto result = spike::importFile(file);

        REQUIRE_MESSAGE(result.ok, result.error);
        CHECK(result.sampleRate == doctest::Approx(96000.0));
        CHECK(result.numChannels == 1);
        CHECK(result.lengthSamples == 48000);
    }

    TEST_CASE("import fails with a message for a file that does not exist")
    {
        TempDir dir;

        const auto result = spike::importFile(dir.file("missing.wav"));

        CHECK_FALSE(result.ok);
        SPIKE_REQUIRE_IMPLEMENTED(result);
        CHECK_FALSE(result.error.empty());
    }

    TEST_CASE("import fails with a message for a file that is not audio")
    {
        TempDir dir;
        const auto file = dir.file("notes.wav");
        {
            std::ofstream out(file, std::ios::binary);
            out << "this is not a RIFF file";
        }

        const auto result = spike::importFile(file);

        CHECK_FALSE(result.ok);
        SPIKE_REQUIRE_IMPLEMENTED(result);
        CHECK_FALSE(result.error.empty());
    }

    TEST_CASE("dump-pcm writes the decoded WAV samples as interleaved little-endian float32")
    {
        TempDir dir;
        const auto file = dir.file("stereo-48k.wav");
        const auto raw = dir.file("stereo-48k.f32");
        auto source = makeSine(48000.0, 2, 997.0, {{0.5, 0.1}, {0.5, 0.2}});
        writeWav(file, source, 48000.0, 24);
        const auto expected = readAudio(file);  // what the file really contains after 24 bit quantisation
        REQUIRE(expected.ok);

        const auto result = spike::dumpPcm(file, raw);

        REQUIRE_MESSAGE(result.ok, result.error);
        CHECK(result.sampleRate == doctest::Approx(48000.0));
        CHECK(result.numChannels == 2);
        CHECK(result.lengthSamples == 48000);
        const auto values = readRawFloat32(raw);
        REQUIRE(values.size() == 2u * 48000u);
        double worst = 0.0;
        for (int n = 0; n < 48000; ++n)
            for (int ch = 0; ch < 2; ++ch)
                worst = std::max(worst, std::abs(static_cast<double>(values[static_cast<std::size_t>(n) * 2 + static_cast<std::size_t>(ch)])
                                                 - expected.samples.getSample(ch, n)));
        CHECK(worst <= 1.0e-6);  // 24 bit quantisation step is 1.2e-7
    }
}
