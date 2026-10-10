// Helpers of the CLI tests (M1-07): running the CLI (in-process and as a real process), JSON access, synthetic audio
// (sines with fixed amplitudes, fixed-seed noise: nothing is recorded), reading rendered files, the null-test residual,
// and the generated m1-mini fixture project. No audio device, no network, no real user folder.
#pragma once

#include "cli/cli.h"
#include "core/command.h"
#include "project/project_fixture.h"

#include "test_support.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace tracklab_test::cli
{

using tracklab::core::Json;
using tracklab_test::ScopedTempDir;
namespace fs = std::filesystem;

inline constexpr double pi = 3.14159265358979323846;

//==============================================================================
// Running the CLI

struct CliRun
{
    int exitCode = -1000;
    std::string out;  ///< stdout
    std::string err;  ///< stderr (in-process only)
    Json json;        ///< parsed stdout, null if it is not JSON

    bool ok() const { return json.is_object() && json.value("ok", false); }
    bool has(const std::string& key) const { return json.is_object() && json.contains(key); }
    /** NaN / -1 if the key is missing, so that a missing value is a plain failed CHECK, not an exception. */
    double number(const std::string& key) const
    {
        return json.is_object() && json.contains(key) && json[key].is_number() ? json[key].get<double>() : std::nan("");
    }
    std::int64_t integer(const std::string& key) const
    {
        return json.is_object() && json.contains(key) && json[key].is_number_integer() ? json[key].get<std::int64_t>()
                                                                                       : -1;
    }
    std::string text(const std::string& key) const
    {
        return json.is_object() && json.contains(key) && json[key].is_string() ? json[key].get<std::string>() : "";
    }
    /** error.code of a failed run, "" if there is none. */
    std::string errorCode() const
    {
        return json.is_object() && json.contains("error") && json["error"].is_object()
                   ? json["error"].value("code", std::string())
                   : std::string();
    }
    std::string errorMessage() const
    {
        return json.is_object() && json.contains("error") && json["error"].is_object()
                   ? json["error"].value("message", std::string())
                   : std::string();
    }
};

/** The exact stdout contract: one JSON object, one line, terminated by '\n'. */
inline bool isSingleJsonLine(const CliRun& run)
{
    return !run.out.empty() && run.out.back() == '\n' && std::count(run.out.begin(), run.out.end(), '\n') == 1 &&
           run.json.is_object();
}

inline std::string path(const juce::File& file)
{
    return file.getFullPathName().toStdString();
}

inline std::string path(const fs::path& file)
{
    return file.string();
}

inline Json parseOutput(const std::string& text)
{
    return Json::parse(text, nullptr, /*allow_exceptions*/ false);  // discarded value -> treated as null below
}

inline void finish(CliRun& run)
{
    run.json = parseOutput(run.out);
    if (run.json.is_discarded())
        run.json = Json();
}

/** The engine's private caches go into the folder of the test run, never into the real user folders. */
inline std::vector<std::string> withTempDir(std::vector<std::string> args)
{
    args.insert(args.begin(), {"--engine-temp-dir", path(engineTempDirectory())});
    return args;
}

/** In-process call of tracklab::cli::runCli (on the test's message thread). */
inline CliRun runCli(const std::vector<std::string>& args, const tracklab::cli::CliHooks& hooks = {})
{
    std::ostringstream out;
    std::ostringstream err;
    CliRun run;
    run.exitCode = tracklab::cli::runCli(withTempDir(args), out, err, hooks);
    run.out = out.str();
    run.err = err.str();
    finish(run);
    return run;
}

/** The real executable `tracklab-cli` (TRACKLAB_CLI_EXE): exit code and stdout. */
inline CliRun runCliProcess(const std::vector<std::string>& args)
{
    CliRun run;
    juce::StringArray command;
    command.add(juce::String(juce::CharPointer_UTF8(TRACKLAB_CLI_EXE)));
    for (const auto& a : withTempDir(args))
        command.add(juce::String::fromUTF8(a.c_str()));

    juce::ChildProcess process;
    if (!process.start(command, juce::ChildProcess::wantStdOut))
        return run;

    run.out = process.readAllProcessOutput().toStdString();
    process.waitForProcessToFinish(120000);
    run.exitCode = static_cast<int>(process.getExitCode());
    finish(run);
    return run;
}

//==============================================================================
// Files

inline std::string readText(const fs::path& file)
{
    std::ifstream in(file, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

inline void writeText(const fs::path& file, const std::string& text)
{
    fs::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << text;
}

/** The commands.json of run-commands: a JSON array of {"id", "params"?}. */
inline fs::path writeCommands(const juce::File& dir, const Json& commands, const char* name = "commands.json")
{
    const fs::path file = fs::path(path(dir)) / name;
    writeText(file, commands.dump(2));
    return file;
}

//==============================================================================
// Synthetic audio

struct Segment
{
    double seconds;
    double amplitude;  ///< peak, linear
};

/** `channels` identical channels of a sine; the segments follow each other. Optionally with a start phase. */
inline juce::AudioBuffer<float> makeSine(double sampleRate, int channels, double frequency,
                                         const std::vector<Segment>& segments, double phase = 0.0)
{
    int total = 0;
    for (const auto& s : segments)
        total += juce::roundToInt(s.seconds * sampleRate);
    juce::AudioBuffer<float> buffer(channels, total);
    int pos = 0;
    for (const auto& s : segments)
    {
        const int n = juce::roundToInt(s.seconds * sampleRate);
        for (int i = 0; i < n; ++i)
        {
            const auto v =
                static_cast<float>(s.amplitude * std::sin(2.0 * pi * frequency * (pos + i) / sampleRate + phase));
            for (int c = 0; c < channels; ++c)
                buffer.setSample(c, pos + i, v);
        }
        pos += n;
    }
    return buffer;
}

inline void writeWav(const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate, int bits)
{
    REQUIRE(file.getParentDirectory().createDirectory().wasOk());
    file.deleteFile();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
    REQUIRE(stream != nullptr);
    auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions()
                                                     .withSampleRate(sampleRate)
                                                     .withNumChannels(buffer.getNumChannels())
                                                     .withBitsPerSample(bits));
    REQUIRE(writer != nullptr);
    REQUIRE(writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()));
}

inline double amplitudeToDb(double amplitude)
{
    return 20.0 * std::log10(amplitude);
}

//==============================================================================
// Reading rendered files

struct AudioData
{
    bool valid = false;
    double sampleRate = 0.0;
    int channels = 0;
    int bitsPerSample = 0;
    std::int64_t length = 0;
    juce::AudioBuffer<float> samples;
};

inline AudioData readAudio(const juce::File& file)
{
    AudioData data;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr)
        return data;
    data.valid = true;
    data.sampleRate = reader->sampleRate;
    data.channels = static_cast<int>(reader->numChannels);
    data.bitsPerSample = static_cast<int>(reader->bitsPerSample);
    data.length = reader->lengthInSamples;
    data.samples.setSize(data.channels, static_cast<int>(data.length));
    reader->read(&data.samples, 0, static_cast<int>(data.length), 0, true, true);
    return data;
}

/** Largest absolute difference of two files in dBFS (null test): -200 for identical files, 0 (full scale) if the
    files differ in length or channel count or cannot be read. */
inline double residualDbfs(const AudioData& a, const AudioData& b)
{
    if (!a.valid || !b.valid || a.length != b.length || a.channels != b.channels)
        return 0.0;
    float worst = 0.0f;
    for (int c = 0; c < a.channels; ++c)
        for (int i = 0; i < static_cast<int>(a.length); ++i)
            worst = std::max(worst, std::abs(a.samples.getSample(c, i) - b.samples.getSample(c, i)));
    return worst > 0.0f ? amplitudeToDb(static_cast<double>(worst)) : -200.0;
}

inline double peakDbfs(const AudioData& a)
{
    float peak = 0.0f;
    for (int c = 0; c < a.channels; ++c)
        peak = std::max(peak, a.samples.getMagnitude(c, 0, static_cast<int>(a.length)));
    return peak > 0.0f ? amplitudeToDb(static_cast<double>(peak)) : -200.0;
}

//==============================================================================
// Projects

/** An empty project `<root>/<name>/<name>.tracklab`, created through the project.new command. The engine used for it
    is gone when this returns, so that the CLI under test is the only engine. */
inline juce::File makeEmptyProject(const juce::File& root, const std::string& name = "Muster")
{
    tracklab_test::project::ProjectFixture fx;
    const auto info = fx.run("project.new", Json{{"folder", path(root)}, {"name", name}});
    fx.settleEdit();
    return tracklab_test::project::fileFromUtf8(info["path"].get<std::string>());
}

/** Fixed noise: a linear congruential generator, so that the samples do not depend on the standard library. */
inline float noiseSample(std::uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(static_cast<double>(state >> 8) / 8388608.0 - 1.0);  // [-1, 1)
}

/** The m1-mini fixture project (also the source of tests/fixtures/m1-mini/golden.wav), generated, nothing recorded:
    two mono 48 kHz / 24 bit stems in `<project>/Audio/`, on two tracks:
      track 1: 220 Hz sine, amplitude 0.25, clip 0 s .. 3 s
      track 2: 330 Hz sine, amplitude 0.125 plus noise (seed 20260709, amplitude 0.01), clip 1 s .. 3 s
    The render of the whole project is 3.000 s = 144000 samples at 48 kHz.

    STAND-IN: there are no track.* / clip.* / audio.import commands yet (M1-04 only has project.* and edit.*), so the
    clips are inserted with the Tracktion API, like the project tests do. When those commands exist, this function
    becomes a commands.json that run-commands applies to the empty project (see the TODO in test_cli_golden.cpp).
    The project is created through project.new and saved through project.save; the engine is gone on return. */
inline juce::File buildMiniProject(const juce::File& root, const std::string& name = "Muster")
{
    namespace te = tracktion;
    tracklab_test::project::ProjectFixture fx;
    const auto info = fx.run("project.new", Json{{"folder", path(root)}, {"name", name}});
    fx.settleEdit();
    const auto projectFile = tracklab_test::project::fileFromUtf8(info["path"].get<std::string>());
    const auto audioDir = projectFile.getParentDirectory().getChildFile("Audio");

    constexpr double sr = 48000.0;
    auto stem1 = makeSine(sr, 1, 220.0, {{3.0, 0.25}});
    auto stem2 = makeSine(sr, 1, 330.0, {{2.0, 0.125}});
    std::uint32_t seed = 20260709u;
    for (int i = 0; i < stem2.getNumSamples(); ++i)
        stem2.setSample(0, i, stem2.getSample(0, i) + 0.01f * noiseSample(seed));
    writeWav(audioDir.getChildFile("stem1.wav"), stem1, sr, 24);
    writeWav(audioDir.getChildFile("stem2.wav"), stem2, sr, 24);

    auto& edit = fx.edit();
    edit.ensureNumberOfAudioTracks(2);
    auto tracks = te::getAudioTracks(edit);
    REQUIRE(tracks.size() >= 2);
    const auto insert = [&](int track, const char* clipName, const juce::File& file, double start, double end)
    {
        auto clip = te::insertWaveClip(*tracks[track], clipName, file,
                                       te::ClipPosition{.time = te::TimeRange(te::TimePosition::fromSeconds(start),
                                                                              te::TimePosition::fromSeconds(end))},
                                       te::DeleteExistingClips::no);
        REQUIRE(clip != nullptr);
        clip->setUsesProxy(false);
    };
    insert(0, "Stem 1", audioDir.getChildFile("stem1.wav"), 0.0, 3.0);
    insert(1, "Stem 2", audioDir.getChildFile("stem2.wav"), 1.0, 3.0);
    tracklab_test::project::settle(edit);
    tracklab_test::project::ProjectFixture::letPluginTimersRunOut(edit);
    fx.run("project.save");
    return projectFile;
}

}  // namespace tracklab_test::cli
