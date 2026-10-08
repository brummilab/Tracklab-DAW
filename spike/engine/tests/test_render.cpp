// Brief M0-06, Minimal-Ziel 2 (render-region) and Bewertung "Render": offline render of a region to 48 kHz /
// 24 bit, then Integrated LUFS, True Peak and LRA, checked against analytically known signals.
//
// Reference values (ITU-R BS.1770-4, EBU Tech 3341/3342):
//  - A stereo sine of 997 Hz with the same peak amplitude A on both channels has an Integrated Loudness of
//    20*log10(A) LUFS (K-weighting gain at 997 Hz is +0.691 dB, offset is -0.691 dB). A = 0.1 -> -20.0 LUFS.
//  - A pure sine has a true peak of 20*log10(A) dBTP.
//  - A sine at fs/6 (8 kHz at 48 kHz) with a phase of 60 degrees has samples of only 0 and +-A*sin(60 deg)
//    (sample peak 1.25 dB below the true peak), but the continuous waveform reaches A: true peak 20*log10(A) dBTP.
//    (fs/4 is not used: the 4x oversampling filter of ffmpeg's ebur128 reads 0.6 dB high at 12 kHz.)
//    The sine is faded in and out over 10 ms: a hard cut at the file edges is a step whose band-limited
//    reconstruction overshoots (Gibbs) to 0.526 = -5.58 dBTP, so the file would not have the sine's true peak.
//  - Two equally long halves at -20 and -30 dBFS: the gated power mean is 10*log10(0.5*0.01 + 0.5*0.001)
//    = -22.60 LUFS (both halves are above the relative gate); the short-term loudness plateaus are -20 and
//    -30 LUFS, so the 10th and 95th percentile sit on the plateaus: LRA = 10 LU.
#include "test_support.h"

using namespace spike_test;

namespace
{
constexpr double kFreq = 997.0;
constexpr double kPi = 3.14159265358979323846;

const double kAmp20 = 0.1;                      // -20 dBFS peak
const double kAmp30 = 0.031622776601683794;     // -30 dBFS peak

spike::RenderResult render(const std::filesystem::path& source, double start, double end, const TempDir& dir,
                           const char* outName = "out.wav")
{
    return spike::renderRegion(source, start, end, dir.file(outName));
}

/** Checks the rendered file with the JUCE reader, independent of the code under test. */
void checkRenderedFile(const spike::RenderResult& r, std::int64_t expectedFrames, int expectedChannels = 2)
{
    REQUIRE_MESSAGE(r.ok, r.error);
    CHECK(r.sampleRate == doctest::Approx(48000.0));
    CHECK(r.bitsPerSample == 24);
    CHECK(r.numChannels == expectedChannels);
    CHECK(r.lengthSamples == expectedFrames);

    const auto file = readAudio(r.outFile);
    REQUIRE_MESSAGE(file.ok, file.error);
    CHECK(file.sampleRate == doctest::Approx(48000.0));
    CHECK(file.bitsPerSample == 24);
    CHECK(file.numChannels == expectedChannels);
    CHECK(file.lengthSamples == expectedFrames);
}
}  // namespace

TEST_SUITE("render")
{
    TEST_CASE("render region of a 48 kHz source has the exact length, 48 kHz and 24 bit")
    {
        TempDir dir;
        const auto source = dir.file("source.wav");
        writeWav(source, makeSine(48000.0, 2, kFreq, {{8.0, kAmp20}}), 48000.0, 24);

        const auto r = render(source, 2.0, 7.5, dir);  // 5.5 s

        checkRenderedFile(r, 264000);
    }

    TEST_CASE("render region with fractional second positions has the exact length")
    {
        TempDir dir;
        const auto source = dir.file("source.wav");
        writeWav(source, makeSine(48000.0, 2, kFreq, {{4.0, kAmp20}}), 48000.0, 24);

        const auto r = render(source, 0.3125, 1.0625, dir);  // 0.75 s

        checkRenderedFile(r, 36000);
    }

    TEST_CASE("render region contains the requested samples of the source, sample-accurately")
    {
        TempDir dir;
        const auto source = dir.file("source.wav");
        const auto reference = makeSine(48000.0, 2, kFreq, {{4.0, kAmp20}, {4.0, kAmp30}});  // level step at 4.0 s
        writeWav(source, reference, 48000.0, 24);

        const auto r = render(source, 3.0, 5.0, dir);  // spans the level step, which must land at output frame 48000

        checkRenderedFile(r, 96000);
        const auto rendered = readAudio(r.outFile);
        REQUIRE(rendered.ok);
        for (int ch = 0; ch < 2; ++ch)
        {
            INFO("channel " << ch);
            // A one-sample shift of the region would cause errors of >= 4e-3 (slope of the sine) and 0.07 at the step.
            CHECK(maxAbsDiff(rendered.samples, ch, 0, reference, ch, 3 * 48000, 96000) <= 2.0e-4);
        }
    }

    TEST_CASE("render region from a 44.1 kHz source is converted to 48 kHz with exact length and level")
    {
        TempDir dir;
        const auto source = dir.file("source-44k1.wav");
        writeWav(source, makeSine(44100.0, 2, kFreq, {{10.0, kAmp20}}), 44100.0, 16);

        const auto r = render(source, 1.0, 4.0, dir);  // 3 s -> 144000 frames at 48 kHz

        checkRenderedFile(r, 144000);
        SPIKE_CHECK_NEAR(r.loudness.integratedLufs, -20.0, 0.1);
    }

    TEST_CASE("render region of the second level of a step signal measures the level of that region only")
    {
        TempDir dir;
        const auto source = dir.file("step.wav");
        writeWav(source, makeSine(48000.0, 2, kFreq, {{4.0, kAmp20}, {4.0, kAmp30}}), 48000.0, 24);

        const auto first = render(source, 0.0, 3.0, dir, "first.wav");
        const auto second = render(source, 4.5, 7.5, dir, "second.wav");

        REQUIRE_MESSAGE(first.ok, first.error);
        REQUIRE_MESSAGE(second.ok, second.error);
        SPIKE_CHECK_NEAR(first.loudness.integratedLufs, -20.0, 0.1);
        SPIKE_CHECK_NEAR(second.loudness.integratedLufs, -30.0, 0.1);
    }

    TEST_CASE("render region fails if the end is not after the start")
    {
        TempDir dir;
        const auto source = dir.file("source.wav");
        writeWav(source, makeSine(48000.0, 2, kFreq, {{2.0, kAmp20}}), 48000.0, 24);

        const auto equal = render(source, 1.0, 1.0, dir, "equal.wav");
        const auto reversed = render(source, 1.5, 0.5, dir, "reversed.wav");

        CHECK_FALSE(equal.ok);
        SPIKE_REQUIRE_IMPLEMENTED(equal);
        CHECK_FALSE(reversed.ok);
        SPIKE_REQUIRE_IMPLEMENTED(reversed);
    }

    TEST_CASE("render region fails with a message if the source file does not exist")
    {
        TempDir dir;

        const auto r = render(dir.file("missing.wav"), 0.0, 1.0, dir);

        CHECK_FALSE(r.ok);
        SPIKE_REQUIRE_IMPLEMENTED(r);
        CHECK_FALSE(r.error.empty());
    }

    TEST_CASE("loudness: Integrated LUFS of a 997 Hz sine at -20 dBFS on both channels is -20.0 +-0.1 LU")
    {
        TempDir dir;
        const auto source = dir.file("sine-20.wav");
        writeWav(source, makeSine(48000.0, 2, kFreq, {{10.0, kAmp20}}), 48000.0, 24);

        const auto r = render(source, 0.0, 10.0, dir);

        REQUIRE_MESSAGE(r.ok, r.error);
        SPIKE_CHECK_NEAR(r.loudness.integratedLufs, -20.0, 0.1);
    }

    TEST_CASE("loudness: Integrated LUFS of a -30 dBFS sine is -30.0 +-0.1 LU (level dependence)")
    {
        TempDir dir;
        const auto source = dir.file("sine-30.wav");
        writeWav(source, makeSine(48000.0, 2, kFreq, {{10.0, kAmp30}}), 48000.0, 24);

        const auto r = render(source, 0.0, 10.0, dir);

        REQUIRE_MESSAGE(r.ok, r.error);
        SPIKE_CHECK_NEAR(r.loudness.integratedLufs, -30.0, 0.1);
    }

    TEST_CASE("loudness: True Peak of a 997 Hz sine at -20 dBFS is -20.0 +-0.1 dBTP")
    {
        TempDir dir;
        const auto source = dir.file("sine-20.wav");
        writeWav(source, makeSine(48000.0, 2, kFreq, {{10.0, kAmp20}}), 48000.0, 24);

        const auto r = render(source, 0.0, 10.0, dir);

        REQUIRE_MESSAGE(r.ok, r.error);
        SPIKE_CHECK_NEAR(r.loudness.truePeakDbtp, -20.0, 0.1);
    }

    TEST_CASE("loudness: True Peak detects the inter-sample peak of a fs/6 sine at 60 degrees (-6.02 dBTP)")
    {
        TempDir dir;
        const auto source = dir.file("fs6.wav");
        // 8 kHz at 48 kHz: the samples are 0, +-0.433 (sample peak -7.27 dBFS), the waveform itself reaches 0.5.
        auto signal = makeSine(48000.0, 2, 8000.0, {{5.0, 0.5}}, kPi / 3.0);
        // 10 ms fades: without them the edges of the file overshoot to -5.58 dBTP (ideal sinc reconstruction).
        signal.applyGainRamp(0, 480, 0.0f, 1.0f);
        signal.applyGainRamp(signal.getNumSamples() - 480, 480, 1.0f, 0.0f);
        writeWav(source, signal, 48000.0, 24);

        const auto r = render(source, 0.0, 5.0, dir);

        REQUIRE_MESSAGE(r.ok, r.error);
        SPIKE_CHECK_NEAR(r.loudness.truePeakDbtp, amplitudeToDb(0.5), 0.1);
    }

    TEST_CASE("loudness: LRA of 30 s at -20 dBFS followed by 30 s at -30 dBFS is 10 LU, gated Integrated -22.6 LUFS")
    {
        TempDir dir;
        const auto source = dir.file("two-level.wav");
        writeWav(source, makeSine(48000.0, 2, kFreq, {{30.0, kAmp20}, {30.0, kAmp30}}), 48000.0, 24);

        const auto r = render(source, 0.0, 60.0, dir);

        REQUIRE_MESSAGE(r.ok, r.error);
        SPIKE_CHECK_NEAR(r.loudness.lra, 10.0, 0.3);
        SPIKE_CHECK_NEAR(r.loudness.integratedLufs, 10.0 * std::log10(0.5 * 0.01 + 0.5 * 0.001), 0.1);
    }

    TEST_CASE("loudness: LRA of a constant-level signal is 0 LU")
    {
        TempDir dir;
        const auto source = dir.file("constant.wav");
        writeWav(source, makeSine(48000.0, 2, kFreq, {{20.0, kAmp20}}), 48000.0, 24);

        const auto r = render(source, 0.0, 20.0, dir);

        REQUIRE_MESSAGE(r.ok, r.error);
        SPIKE_CHECK_NEAR(r.loudness.lra, 0.0, 0.1);
    }

    TEST_CASE("loudness: True Peak is never below the sample peak of the rendered file")
    {
        TempDir dir;
        const auto source = dir.file("sine-20.wav");
        writeWav(source, makeSine(48000.0, 2, kFreq, {{6.0, kAmp20}}), 48000.0, 24);

        const auto r = render(source, 1.0, 5.0, dir);

        REQUIRE_MESSAGE(r.ok, r.error);
        const auto rendered = readAudio(r.outFile);
        REQUIRE(rendered.ok);
        double samplePeak = 0.0;
        for (int ch = 0; ch < rendered.numChannels; ++ch)
            samplePeak = std::max(samplePeak, static_cast<double>(rendered.samples.getMagnitude(ch, 0, rendered.samples.getNumSamples())));
        // The true peak can never be below the sample peak of the file that was measured.
        CHECK(r.loudness.truePeakDbtp >= amplitudeToDb(samplePeak) - 0.01);
    }
}
