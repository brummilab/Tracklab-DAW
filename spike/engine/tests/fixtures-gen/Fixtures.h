// Fixture generator of the engine spike tests. Everything is synthesised at run time from analytic
// formulas (fixed parameters, no randomness) into a temporary directory. No audio file is ever
// committed (R13); the expected values in the tests follow from the formulas below.
#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace spike_test
{

/** Temporary directory, removed with all its content on destruction. */
class TempDir
{
public:
    TempDir();
    ~TempDir();
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& path() const { return dir; }
    std::filesystem::path file(const std::string& name) const { return dir / name; }

private:
    std::filesystem::path dir;
};

//==============================================================================
// Analytic signals

/** One segment of a piecewise sine: amplitude `amplitude` (peak) for `seconds`. */
struct Segment
{
    double seconds;
    double amplitude;
};

/** x[n] = A(n) * sin(2*pi*f*n/sr + phase), identical on all channels. A(n) follows the segments. */
juce::AudioBuffer<float> makeSine(double sampleRate, int numChannels, double frequencyHz,
                                  const std::vector<Segment>& segments, double phaseRad = 0.0);

/** dB value (re full scale) of a linear peak amplitude. */
double amplitudeToDb(double amplitude);

/** Linear amplitude of a dB value. */
double dbToAmplitude(double db);

//==============================================================================
// Files

/** Writes a WAV file with the JUCE writer (bitsPerSample 16, 24 or 32 = float). */
void writeWav(const std::filesystem::path& file, const juce::AudioBuffer<float>& audio, double sampleRate,
              int bitsPerSample);

/** True if the `lame` command line encoder is on the PATH. */
bool lameAvailable();

/** True if the `ffmpeg` command line tool is on the PATH. */
bool ffmpegAvailable();

/** Encodes a WAV to MP3 with `lame --silent -b 128 -q 2`. Returns false (and fills `log`) on failure. */
bool encodeMp3(const std::filesystem::path& wav, const std::filesystem::path& mp3, std::string& log);

/** Decodes `file` with ffmpeg to interleaved float32 PCM (reference decoder, independent of JUCE). */
bool decodeWithFfmpeg(const std::filesystem::path& file, std::vector<float>& interleaved, int& numChannels,
                      std::string& log);

/** Result of reading an audio file with the JUCE readers (independent of the code under test). */
struct AudioData
{
    bool ok = false;
    std::string error;
    double sampleRate = 0.0;
    int numChannels = 0;
    int bitsPerSample = 0;
    std::int64_t lengthSamples = 0;
    juce::AudioBuffer<float> samples;
};

AudioData readAudio(const std::filesystem::path& file);

/** Reads a raw interleaved little-endian float32 file. */
std::vector<float> readRawFloat32(const std::filesystem::path& file);

//==============================================================================
// Measurements

/** RMS of one channel over [start, start+num) in dB re full scale. */
double rmsDb(const juce::AudioBuffer<float>& buffer, int channel, int start, int num);

/** Largest |a[offsetA+i] - b[offsetB+i]| over i in [0, num) for one channel. */
double maxAbsDiff(const juce::AudioBuffer<float>& a, int channelA, int offsetA,
                  const juce::AudioBuffer<float>& b, int channelB, int offsetB, int num);

}  // namespace spike_test
