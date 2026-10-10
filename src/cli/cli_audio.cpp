#include "cli/cli_audio.h"

#include "cli/cli_error.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>

namespace tracklab::cli
{

namespace te = tracktion;
using core::Json;

namespace
{

constexpr double renderSampleRate = 48000.0;
constexpr int renderBitsPerSample = 24;

/** End of the last clip of any track; nullopt if the project has no clip. */
std::optional<te::TimePosition> endOfLastClip(const te::Edit& edit)
{
    std::optional<te::TimePosition> end;
    for (auto* track : te::getClipTracks(edit))
        for (auto* clip : track->getClips())
            if (!end || clip->getPosition().getEnd() > *end)
                end = clip->getPosition().getEnd();
    return end;
}

std::string toStd(const juce::String& text)
{
    return text.toStdString();
}

/** What a rendered file really is, read back through the engine's own reader. */
struct FileFormat
{
    double sampleRate = 0.0;
    int channels = 0;
    int bitsPerSample = 0;
    juce::int64 length = 0;
};

std::optional<FileFormat> readFormat(te::Engine& engine, const juce::File& file)
{
    juce::AudioFormat* format = nullptr;
    const std::unique_ptr<juce::AudioFormatReader> reader(
        te::AudioFileUtils::createReaderFindingFormat(engine, file, format));
    if (reader == nullptr)
        return std::nullopt;
    return FileFormat{reader->sampleRate, static_cast<int>(reader->numChannels),
                      static_cast<int>(reader->bitsPerSample), reader->lengthInSamples};
}

/** The meter reports digital silence as te::LoudnessMeter::silenceFloorDb (-100), not as -infinity. A value at or
    below that floor is "no measurement", not a level of -100 dB: JSON null. (nlohmann also writes NaN and infinity as
    null.) */
Json reading(float value)
{
    if (!(value > te::LoudnessMeter::silenceFloorDb))  // also true for NaN
        return nullptr;
    return static_cast<double>(value);
}

}  // namespace

Json renderProject(te::Engine& engine, te::Edit& edit, const juce::File& destination)
{
    const auto end = endOfLastClip(edit);
    if (!end || *end <= te::TimePosition())
        throw operationFailed("empty_project", "the project has no clip to render");

    if (!destination.getParentDirectory().createDirectory().wasOk())
        throw operationFailed("write_failed", "cannot create the folder of " + toStd(destination.getFullPathName()));

    // Rendered under a temporary name next to the destination: a failed render never leaves a half-written file there.
    juce::TemporaryFile temporary(destination);
    temporary.getFile().deleteFile();

    te::RenderSpecification spec;
    // The documented "empty list = whole Edit" does not hold in this Tracktion version (createRenderJob then has nothing
    // to render), so the tracks are listed: every audio track at any depth (also in a plain folder, which brings no
    // children along) and every submix folder. What lies inside a submix is not listed itself: the submix renders its
    // children through its own plugin chain, and a second entry would play them twice.
    for (auto* track : te::getAllTracks(edit))
    {
        if (track->isPartOfSubmix())
            continue;
        const auto* folder = dynamic_cast<te::FolderTrack*>(track);
        if (dynamic_cast<te::AudioTrack*>(track) != nullptr || (folder != nullptr && folder->isSubmixFolder()))
            spec.tracks.add(track->itemID);
    }
    spec.time = te::TimeRange(te::TimePosition(), *end);
    spec.includeTails = false;  // exactly as long as the clips, no reverb/delay tail
    spec.destination = temporary.getFile();
    spec.format = te::RenderFormat::wav;
    spec.sampleRate = renderSampleRate;
    spec.bitDepth = renderBitsPerSample;
    spec.channelLayout = "stereo";
    spec.dither = false;  // dither would make two renders differ (null test, Golden-Render)

    if (const auto valid = te::validateRenderSpecification(edit, spec); valid.failed())
        throw operationFailed("render_failed", "invalid render specification: " + toStd(valid.getErrorMessage()));

    auto job = te::createRenderJob(edit, spec);
    if (!job.has_value())
        throw operationFailed("render_failed", "could not create the render job");

    const auto rendered = te::Renderer::renderToFile("tracklab-cli render", job->params);
    if (rendered == juce::File() || !rendered.existsAsFile())
        throw operationFailed("render_failed", "the render failed");
    if (rendered != temporary.getFile())  // the renderer must not move the file away from where we asked for it
    {
        rendered.deleteFile();
        throw operationFailed("render_failed", "the renderer wrote an unexpected file");
    }

    const auto format = readFormat(engine, temporary.getFile());
    if (!format)
        throw operationFailed("render_failed", "the rendered file cannot be read back");
    if (!temporary.overwriteTargetFileWithTemporary())
        throw operationFailed("write_failed", "cannot write " + toStd(destination.getFullPathName()));

    return Json{
        {"out", toStd(destination.getFullPathName())}, {"format", "wav24"},
        {"sample_rate", format->sampleRate},           {"channels", format->channels},
        {"bits_per_sample", format->bitsPerSample},    {"length_samples", static_cast<std::int64_t>(format->length)}};
}

Json measureLoudness(te::Engine& engine, const juce::File& file, const Measurements& wanted)
{
    if (!file.existsAsFile())
        throw operationFailed("file_not_found", "no such file: " + toStd(file.getFullPathName()));

    juce::AudioFormat* format = nullptr;
    const std::unique_ptr<juce::AudioFormatReader> reader(
        te::AudioFileUtils::createReaderFindingFormat(engine, file, format));
    if (reader == nullptr)
        throw operationFailed("unreadable_audio",
                              "not an audio file the engine can read: " + toStd(file.getFullPathName()));

    constexpr int blockSize = 4096;
    const auto numChannels = static_cast<int>(reader->numChannels);
    te::LoudnessMeter meter;
    meter.prepare(reader->sampleRate, numChannels, blockSize);

    juce::AudioBuffer<float> buffer(numChannels, blockSize);
    for (juce::int64 pos = 0; pos < reader->lengthInSamples; pos += blockSize)
    {
        const auto count = static_cast<int>(std::min<juce::int64>(blockSize, reader->lengthInSamples - pos));
        if (!reader->read(&buffer, 0, count, pos, true, true))
            throw operationFailed("read_failed", "read error in " + toStd(file.getFullPathName()));
        meter.process(buffer.getArrayOfReadPointers(), numChannels, count);
    }
    meter.flush();

    const auto readings = meter.getReadings();
    Json result = Json::object();
    if (wanted.loudness)
        result["integrated_lufs"] = reading(readings.integratedLufs);
    if (wanted.truePeak)
        result["true_peak_dbtp"] = reading(readings.truePeakDb);
    if (wanted.lra)
        result["lra"] = reading(readings.loudnessRangeLu);
    return result;
}

}  // namespace tracklab::cli
