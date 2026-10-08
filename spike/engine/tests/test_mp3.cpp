// Brief M0-06, Bewertung "Import: MP3 dekodiert auf Windows und Linux sample-gleich".
//
// MP3 fixtures are encoded at run time with the `lame` command line tool from a synthetic sine (never committed).
// If `lame` is missing the tests are reported as SKIPPED (CTest exit code 77), not as passed; with
// SPIKE_REQUIRE_TOOLS=1 (CI) a missing tool is a failure.
//
// What can be verified locally (one platform):
//  - the MP3 is imported with the right sample rate / channel count, length within the codec padding,
//  - decoding is deterministic (two decodes are bit-identical),
//  - the decoded samples equal those of an independent reference decoder (ffmpeg) after removing the constant
//    start offset (encoder delay handling differs between decoders), error <= -70 dB.
// The comparison between Windows and Linux is done by the CI workflow (tests/mp3_platform_compare.py) on the same
// MP3 file; it needs both platforms.
//
// Length: an MP3 stream consists of whole frames of 1152 samples plus the encoder delay (about 1105 samples), and
// decoders handle the padding differently, so the length is only required to be within 2 frames of the source.
#include <algorithm>

#include "test_support.h"

using namespace spike_test;

namespace
{
constexpr int kFrame = 1152;

/** Encodes a 2 s stereo sine (440 Hz, amplitude 0.5) to MP3. Empty path if lame is unavailable (caller skips). */
std::filesystem::path makeMp3(const TempDir& dir, double sampleRate, const char* name)
{
    if (!lameAvailable())
        return {};
    const auto wav = dir.file(std::string(name) + ".wav");
    const auto mp3 = dir.file(std::string(name) + ".mp3");
    writeWav(wav, makeSine(sampleRate, 2, 440.0, {{2.0, 0.5}}), sampleRate, 16);
    std::string log;
    REQUIRE_MESSAGE(encodeMp3(wav, mp3, log), "lame failed: " << log);
    return mp3;
}

void checkMp3Import(const spike::ImportResult& r, double sampleRate)
{
    REQUIRE_MESSAGE(r.ok, r.error);
    CHECK(r.sampleRate == doctest::Approx(sampleRate));
    CHECK(r.numChannels == 2);
    const auto expected = static_cast<std::int64_t>(2.0 * sampleRate);
    INFO("decoded length " << r.lengthSamples << ", source length " << expected);
    CHECK(std::abs(r.lengthSamples - expected) <= 2 * kFrame);
}

/** Frame offset `lag` that best aligns b[n + lag] with a[n] (channel 0), searched in [-maxLag, maxLag]. */
int findLag(const std::vector<float>& a, const std::vector<float>& b, int numChannels, int maxLag)
{
    const int window = 4000;
    const int start = 8000;
    double bestErr = 1.0e300;
    int bestLag = 0;
    const auto framesA = static_cast<int>(a.size()) / numChannels;
    const auto framesB = static_cast<int>(b.size()) / numChannels;
    for (int lag = -maxLag; lag <= maxLag; ++lag)
    {
        if (start + lag < 0 || start + window + lag > framesB || start + window > framesA)
            continue;
        double err = 0.0;
        for (int n = start; n < start + window; ++n)
        {
            const double d = static_cast<double>(a[static_cast<std::size_t>(n * numChannels)]) -
                             static_cast<double>(b[static_cast<std::size_t>((n + lag) * numChannels)]);
            err += d * d;
        }
        if (err < bestErr)
        {
            bestErr = err;
            bestLag = lag;
        }
    }
    return bestLag;
}

/** Compares interleaved stereo `decoded` with the ffmpeg decoding of `mp3` after removing the constant start offset. */
void checkAgainstFfmpeg(const std::filesystem::path& mp3, const std::vector<float>& decoded)
{
    std::vector<float> reference;
    int refChannels = 0;
    std::string log;
    REQUIRE_MESSAGE(decodeWithFfmpeg(mp3, reference, refChannels, log), "ffmpeg failed: " << log);

    const int lag = findLag(decoded, reference, 2, 3000);
    INFO("offset of the reference relative to the decoder output: " << lag << " frames");
    const int framesDecoded = static_cast<int>(decoded.size()) / 2;
    const int framesRef = static_cast<int>(reference.size()) / 2;
    const int from = std::max(0, -lag) + 2 * kFrame;                       // skip the codec start-up region
    const int to = std::min(framesDecoded, framesRef - lag) - 2 * kFrame;  // and the tail
    REQUIRE(to - from > 20000);
    double errPower = 0.0;
    double sigPower = 0.0;
    for (int n = from; n < to; ++n)
        for (int ch = 0; ch < 2; ++ch)
        {
            const double a = decoded[static_cast<std::size_t>(n * 2 + ch)];
            const double b = reference[static_cast<std::size_t>((n + lag) * 2 + ch)];
            errPower += (a - b) * (a - b);
            sigPower += b * b;
        }
    const double errDb = 10.0 * std::log10(std::max(errPower / sigPower, 1.0e-30));
    INFO("error relative to signal: " << errDb << " dB");
    CHECK(errDb <= -70.0);
}
}  // namespace

TEST_SUITE("mp3")
{
    TEST_CASE("mp3 import reports 44.1 kHz, 2 channels and a length within the codec padding")
    {
        TempDir dir;
        const auto mp3 = makeMp3(dir, 44100.0, "stereo-44k1");
        SPIKE_SKIP_UNLESS(!mp3.empty(), "lame is not installed");

        checkMp3Import(spike::importFile(mp3), 44100.0);
    }

    TEST_CASE("mp3 import reports 48 kHz, 2 channels and a length within the codec padding")
    {
        TempDir dir;
        const auto mp3 = makeMp3(dir, 48000.0, "stereo-48k");
        SPIKE_SKIP_UNLESS(!mp3.empty(), "lame is not installed");

        checkMp3Import(spike::importFile(mp3), 48000.0);
    }

    TEST_CASE("mp3 decoding is deterministic: two decodes are bit-identical")
    {
        TempDir dir;
        const auto mp3 = makeMp3(dir, 44100.0, "stereo-44k1");
        SPIKE_SKIP_UNLESS(!mp3.empty(), "lame is not installed");

        const auto first = spike::dumpPcm(mp3, dir.file("first.f32"));
        const auto second = spike::dumpPcm(mp3, dir.file("second.f32"));

        REQUIRE_MESSAGE(first.ok, first.error);
        REQUIRE_MESSAGE(second.ok, second.error);
        const auto a = readRawFloat32(dir.file("first.f32"));
        const auto b = readRawFloat32(dir.file("second.f32"));
        REQUIRE(a.size() == static_cast<std::size_t>(first.lengthSamples * first.numChannels));
        CHECK(a == b);
    }

    TEST_CASE(
        "baseline: the JUCE MP3 reader equals the ffmpeg reference within -70 dB (validates the comparison itself)")
    {
        TempDir dir;
        const auto mp3 = makeMp3(dir, 44100.0, "stereo-44k1");
        SPIKE_SKIP_UNLESS(!mp3.empty(), "lame is not installed");
        SPIKE_SKIP_UNLESS(ffmpegAvailable(), "ffmpeg is not installed");

        const auto juceDecoded = readAudio(mp3);
        REQUIRE_MESSAGE(juceDecoded.ok, juceDecoded.error);
        REQUIRE(juceDecoded.numChannels == 2);
        std::vector<float> interleaved(static_cast<std::size_t>(juceDecoded.lengthSamples) * 2);
        for (int n = 0; n < juceDecoded.lengthSamples; ++n)
            for (int ch = 0; ch < 2; ++ch)
                interleaved[static_cast<std::size_t>(n) * 2 + static_cast<std::size_t>(ch)] =
                    juceDecoded.samples.getSample(ch, n);

        checkAgainstFfmpeg(mp3, interleaved);
    }

    TEST_CASE(
        "mp3 decoded samples of dump-pcm equal the ffmpeg reference within -70 dB after aligning the start offset")
    {
        TempDir dir;
        const auto mp3 = makeMp3(dir, 44100.0, "stereo-44k1");
        SPIKE_SKIP_UNLESS(!mp3.empty(), "lame is not installed");
        SPIKE_SKIP_UNLESS(ffmpegAvailable(), "ffmpeg is not installed");

        const auto dumped = spike::dumpPcm(mp3, dir.file("decoded.f32"));

        REQUIRE_MESSAGE(dumped.ok, dumped.error);
        REQUIRE(dumped.numChannels == 2);
        checkAgainstFfmpeg(mp3, readRawFloat32(dir.file("decoded.f32")));
    }

    TEST_CASE("render region from an MP3 source has the exact length")
    {
        TempDir dir;
        const auto mp3 = makeMp3(dir, 44100.0, "stereo-44k1");
        SPIKE_SKIP_UNLESS(!mp3.empty(), "lame is not installed");

        const auto r = spike::renderRegion(mp3, 0.5, 1.5, dir.file("region.wav"));

        REQUIRE_MESSAGE(r.ok, r.error);
        CHECK(r.sampleRate == doctest::Approx(48000.0));
        CHECK(r.bitsPerSample == 24);
        CHECK(r.lengthSamples == 48000);
    }

    TEST_CASE("CLI import of an MP3 prints sample rate, channels and length as JSON")
    {
        TempDir dir;
        const auto mp3 = makeMp3(dir, 44100.0, "stereo-44k1");
        SPIKE_SKIP_UNLESS(!mp3.empty(), "lame is not installed");

        const auto run = runCli({"import", mp3.string()});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        SPIKE_CHECK_NEAR(run.number("sample_rate"), 44100.0, 0.5);
        CHECK(static_cast<int>(run.get("channels")) == 2);
        CHECK(std::abs(run.integer("length_samples") - 88200) <= 2 * kFrame);
    }
}
