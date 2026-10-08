// render-region: offline render of a region of an audio file through a Tracktion Edit, then loudness measurement.
//
// Render API: RenderSpecification -> createRenderJob() -> Renderer::renderToFile(). RenderSpecification is the
// plain-data render description of Tracktion 3.5 with a canonical JSON form, which matches Tracklab's rule that
// every action is a command with a JSON schema; the low-level Renderer::Parameters stay an implementation detail.
//
// Loudness: Tracktion's LoudnessMeter (BS.1770-4 K-weighting and gating, EBU Tech 3342 LRA, 4x oversampled true
// peak), driven offline over the rendered file. The measurement is of the file as written (24 bit).
#include <cmath>

#include "spike_common.h"
#include "spike_engine.h"

namespace spike
{

namespace
{

namespace te = tracktion;
using detail::SpikeEngine;
using detail::toJuceFile;
using detail::toStd;
using detail::toStdPath;

/** Measures Integrated LUFS, True Peak and LRA of a file with Tracktion's LoudnessMeter. */
bool measureLoudness(te::Engine& engine, const juce::File& file, Loudness& loudness, std::string& error)
{
    juce::AudioFormat* format = nullptr;
    std::unique_ptr<juce::AudioFormatReader> reader(
        te::AudioFileUtils::createReaderFindingFormat(engine, file, format));
    if (reader == nullptr)
    {
        error = "cannot read the rendered file " + toStd(file.getFullPathName());
        return false;
    }

    constexpr int kBlock = 4096;
    const auto numChannels = static_cast<int>(reader->numChannels);
    te::LoudnessMeter meter;
    meter.prepare(reader->sampleRate, numChannels, kBlock);

    juce::AudioBuffer<float> buffer(numChannels, kBlock);
    for (juce::int64 pos = 0; pos < reader->lengthInSamples; pos += kBlock)
    {
        const auto num = static_cast<int>(std::min<juce::int64>(kBlock, reader->lengthInSamples - pos));
        if (!reader->read(&buffer, 0, num, pos, true, true))
        {
            error = "read error in the rendered file " + toStd(file.getFullPathName());
            return false;
        }
        meter.process(buffer.getArrayOfReadPointers(), numChannels, num);
    }
    meter.flush();

    const auto readings = meter.getReadings();
    loudness.integratedLufs = static_cast<double>(readings.integratedLufs);
    loudness.truePeakDbtp = static_cast<double>(readings.truePeakDb);
    loudness.lra = static_cast<double>(readings.loudnessRangeLu);
    return true;
}

}  // namespace

RenderResult renderRegion(const std::filesystem::path& source, double startSeconds, double endSeconds,
                          const std::filesystem::path& outFile)
{
    RenderResult result;
    result.outFile = outFile;

    if (!(startSeconds >= 0.0) || !(endSeconds > startSeconds))
    {
        result.error = "invalid region: --end must be after --start and --start must not be negative";
        return result;
    }

    SpikeEngine engine;
    const auto sourceFile = toJuceFile(source);
    const te::AudioFile audioFile(engine.get(), sourceFile);
    if (!sourceFile.existsAsFile())
    {
        result.error = "file does not exist: " + source.string();
        return result;
    }

    const auto info = audioFile.getInfo();
    if (!info.wasParsedOk || info.sampleRate <= 0.0 || info.numChannels <= 0)
    {
        result.error = "unsupported or damaged audio file: " + source.string();
        return result;
    }
    if (info.numChannels > 2)
    {
        result.error = "only mono and stereo sources are supported by the spike";
        return result;
    }

    // Brief M0-06, decision 6: a region beyond the end of the file is an error, it is never clamped.
    if (std::llround(endSeconds * info.sampleRate) > info.lengthInSamples)
    {
        result.error = "region end " + std::to_string(endSeconds) + " s is after the end of the file (" +
                       std::to_string(info.getLengthInSeconds()) + " s)";
        return result;
    }

    auto edit = te::Edit::createSingleTrackEdit(engine.get(), te::Edit::EditRole::forRendering);
    if (edit == nullptr)
    {
        result.error = "could not create an edit";
        return result;
    }
    edit->getMasterVolumePlugin()->setVolumeDb(0.0f);

    auto* track = te::getAudioTracks(*edit)[0];
    const auto regionLength = te::TimeDuration::fromSeconds(endSeconds - startSeconds);

    // The clip starts at 0 in the edit; its offset selects the region inside the source file.
    te::ClipPosition position;
    position.time = te::TimeRange(te::TimePosition(), regionLength);
    position.offset = te::TimeDuration::fromSeconds(startSeconds);
    auto clip = te::insertWaveClip(*track, "region", sourceFile, position, te::DeleteExistingClips::yes);
    if (clip == nullptr)
    {
        result.error = "could not insert the source file into the edit";
        return result;
    }
    // Read the source directly. A proxy (Tracktion's decoded/resampled cache file) would be built asynchronously
    // and is not needed for a one-off render.
    clip->setUsesProxy(false);

    te::RenderSpecification spec;
    spec.tracks = {track->itemID};
    spec.time = te::TimeRange(te::TimePosition(), regionLength);
    spec.includeTails = false;  // exact region length, no reverb/delay tail
    spec.destination = toJuceFile(outFile);
    spec.format = te::RenderFormat::wav;
    spec.sampleRate = kRenderSampleRate;
    spec.bitDepth = kRenderBitsPerSample;
    spec.channelLayout = info.numChannels == 1 ? "mono" : "stereo";
    spec.dither = false;

    if (const auto valid = te::validateRenderSpecification(*edit, spec); valid.failed())
    {
        result.error = "invalid render specification: " + toStd(valid.getErrorMessage());
        return result;
    }

    auto job = te::createRenderJob(*edit, spec);
    if (!job.has_value())
    {
        result.error = "could not create the render job";
        return result;
    }

    std::filesystem::create_directories(outFile.parent_path().empty() ? std::filesystem::path(".")
                                                                      : outFile.parent_path());
    const auto rendered = te::Renderer::renderToFile("render-region", job->params);
    if (rendered == juce::File() || !rendered.existsAsFile())
    {
        const auto warning = engine.takeLastWarning();
        result.error = "render failed" + (warning.isNotEmpty() ? ": " + toStd(warning) : std::string());
        return result;
    }

    // Report what was really written, read back through the engine's import path.
    const te::AudioFile renderedFile(engine.get(), rendered);
    const auto out = renderedFile.getInfo();
    if (!out.wasParsedOk)
    {
        result.error = "the rendered file cannot be read: " + toStd(rendered.getFullPathName());
        return result;
    }

    if (!measureLoudness(engine.get(), rendered, result.loudness, result.error))
        return result;

    result.ok = true;
    result.error.clear();
    result.outFile = toStdPath(rendered);
    result.sampleRate = out.sampleRate;
    result.numChannels = out.numChannels;
    result.bitsPerSample = out.bitsPerSample;
    result.lengthSamples = out.lengthInSamples;
    return result;
}

}  // namespace spike
