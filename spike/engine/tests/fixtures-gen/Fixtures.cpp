#include "Fixtures.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <fstream>

namespace spike_test
{

namespace
{
constexpr double kPi = 3.14159265358979323846;

juce::File toJuce(const std::filesystem::path& p)
{
    const auto utf8 = p.u8string();
    return juce::File(
        juce::String::fromUTF8(reinterpret_cast<const char*>(utf8.c_str()), static_cast<int>(utf8.size())));
}

/** Runs a command, waits for it and returns its exit code (-1 if it could not be started). */
int runProcess(const juce::StringArray& args, std::string& log, int timeoutMs = 120000)
{
    juce::ChildProcess process;
    if (!process.start(args))
        return -1;

    log = process.readAllProcessOutput().toStdString();
    if (!process.waitForProcessToFinish(timeoutMs))
    {
        process.kill();
        return -1;
    }
    return static_cast<int>(process.getExitCode());
}
}  // namespace

//==============================================================================
TempDir::TempDir()
{
    dir = std::filesystem::temp_directory_path() / ("spike-tests-" + juce::Uuid().toDashedString().toStdString());
    std::filesystem::create_directories(dir);
}

TempDir::~TempDir()
{
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

//==============================================================================
juce::AudioBuffer<float> makeSine(double sampleRate, int numChannels, double frequencyHz,
                                  const std::vector<Segment>& segments, double phaseRad)
{
    std::vector<std::int64_t> ends;
    std::int64_t total = 0;
    for (const auto& s : segments)
    {
        total += static_cast<std::int64_t>(std::llround(s.seconds * sampleRate));
        ends.push_back(total);
    }

    juce::AudioBuffer<float> buffer(numChannels, static_cast<int>(total));
    std::size_t seg = 0;
    for (std::int64_t n = 0; n < total; ++n)
    {
        while (n >= ends[seg])
            ++seg;
        const double v = segments[seg].amplitude *
                         std::sin(2.0 * kPi * frequencyHz * static_cast<double>(n) / sampleRate + phaseRad);
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample(ch, static_cast<int>(n), static_cast<float>(v));
    }
    return buffer;
}

double amplitudeToDb(double amplitude)
{
    return 20.0 * std::log10(amplitude);
}

double dbToAmplitude(double db)
{
    return std::pow(10.0, db / 20.0);
}

//==============================================================================
void writeWav(const std::filesystem::path& file, const juce::AudioBuffer<float>& audio, double sampleRate,
              int bitsPerSample)
{
    juce::WavAudioFormat format;
    auto stream = std::make_unique<juce::FileOutputStream>(toJuce(file));
    jassert(stream->openedOk());

    auto options =
        juce::AudioFormatWriterOptions()
            .withSampleRate(sampleRate)
            .withNumChannels(audio.getNumChannels())
            .withBitsPerSample(bitsPerSample)
            .withSampleFormat(bitsPerSample == 32 ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint
                                                  : juce::AudioFormatWriterOptions::SampleFormat::integral);
    std::unique_ptr<juce::OutputStream> out = std::move(stream);
    auto writer = format.createWriterFor(out, options);
    jassert(writer != nullptr);
    writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples());
}

bool lameAvailable()
{
    std::string log;
    return runProcess({"lame", "--version"}, log, 20000) == 0;
}

bool ffmpegAvailable()
{
    std::string log;
    return runProcess({"ffmpeg", "-hide_banner", "-version"}, log, 20000) == 0;
}

bool encodeMp3(const std::filesystem::path& wav, const std::filesystem::path& mp3, std::string& log)
{
    juce::StringArray args;
    args.addArray({"lame", "--silent", "-b", "128", "-q", "2"});
    args.add(juce::String(wav.string()));
    args.add(juce::String(mp3.string()));
    return runProcess(args, log) == 0 && std::filesystem::exists(mp3);
}

bool decodeWithFfmpeg(const std::filesystem::path& file, std::vector<float>& interleaved, int& numChannels,
                      std::string& log)
{
    const auto raw = file.string() + ".ref.f32";
    numChannels = 2;
    juce::StringArray args;
    args.addArray({"ffmpeg", "-v", "error", "-y", "-i"});
    args.add(juce::String(file.string()));
    args.addArray({"-ac", "2", "-f", "f32le", "-acodec", "pcm_f32le"});
    args.add(juce::String(raw));
    const int rc = runProcess(args, log);
    if (rc != 0)
        return false;
    interleaved = readRawFloat32(raw);
    return !interleaved.empty();
}

AudioData readAudio(const std::filesystem::path& file)
{
    AudioData data;
    juce::AudioFormatManager manager;
    manager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(manager.createReaderFor(toJuce(file)));
    if (reader == nullptr)
    {
        data.error = "no reader for " + file.string();
        return data;
    }

    data.sampleRate = reader->sampleRate;
    data.numChannels = static_cast<int>(reader->numChannels);
    data.bitsPerSample = static_cast<int>(reader->bitsPerSample);
    data.lengthSamples = reader->lengthInSamples;
    data.samples.setSize(data.numChannels, static_cast<int>(data.lengthSamples));
    data.ok = reader->read(&data.samples, 0, static_cast<int>(data.lengthSamples), 0, true, true);
    if (!data.ok)
        data.error = "read failed for " + file.string();
    return data;
}

std::vector<float> readRawFloat32(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary | std::ios::ate);
    if (!in)
        return {};
    const auto bytes = static_cast<std::size_t>(in.tellg());
    in.seekg(0);
    std::vector<float> values(bytes / sizeof(float));
    in.read(reinterpret_cast<char*>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(float)));
    return values;
}

//==============================================================================
double rmsDb(const juce::AudioBuffer<float>& buffer, int channel, int start, int num)
{
    const double rms = buffer.getRMSLevel(channel, start, num);
    return 20.0 * std::log10(std::max(rms, 1.0e-12));
}

double maxAbsDiff(const juce::AudioBuffer<float>& a, int channelA, int offsetA, const juce::AudioBuffer<float>& b,
                  int channelB, int offsetB, int num)
{
    const float* pa = a.getReadPointer(channelA) + offsetA;
    const float* pb = b.getReadPointer(channelB) + offsetB;
    double worst = 0.0;
    for (int i = 0; i < num; ++i)
        worst = std::max(worst, std::abs(static_cast<double>(pa[i]) - static_cast<double>(pb[i])));
    return worst;
}

}  // namespace spike_test
