// import and dump-pcm: both open the file through the engine's own import path (Tracktion's
// AudioFileFormatManager), so the CLI reports exactly what a clip in an Edit would see.
#include <bit>
#include <cstring>
#include <fstream>

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

/** Opens a reader with the same format lookup that Tracktion uses for clips (AudioFileUtils). */
std::unique_ptr<juce::AudioFormatReader> openReader(te::Engine& engine, const juce::File& file, std::string& error)
{
    if (!file.existsAsFile())
    {
        error = "file does not exist: " + toStd(file.getFullPathName());
        return nullptr;
    }

    juce::AudioFormat* format = nullptr;
    std::unique_ptr<juce::AudioFormatReader> reader(
        te::AudioFileUtils::createReaderFindingFormat(engine, file, format));
    if (reader == nullptr || format == nullptr)
    {
        error = "unsupported or damaged audio file: " + toStd(file.getFullPathName());
        return nullptr;
    }
    if (reader->sampleRate <= 0.0 || reader->numChannels == 0)
    {
        error = "audio file has no channels or no sample rate: " + toStd(file.getFullPathName());
        return nullptr;
    }
    return reader;
}

}  // namespace

ImportResult importFile(const std::filesystem::path& file)
{
    ImportResult result;
    SpikeEngine engine;

    // te::AudioFile is what a clip uses; its info goes through the engine's format manager.
    const te::AudioFile audioFile(engine.get(), toJuceFile(file));
    if (!audioFile.getFile().existsAsFile())
    {
        result.error = "file does not exist: " + file.string();
        return result;
    }

    const auto info = audioFile.getInfo();
    if (!info.wasParsedOk || info.sampleRate <= 0.0 || info.numChannels <= 0)
    {
        result.error = "unsupported or damaged audio file: " + file.string();
        return result;
    }

    result.ok = true;
    result.error.clear();
    result.sampleRate = info.sampleRate;
    result.numChannels = info.numChannels;
    result.lengthSamples = info.lengthInSamples;
    return result;
}

DumpPcmResult dumpPcm(const std::filesystem::path& file, const std::filesystem::path& rawOut)
{
    DumpPcmResult result;
    SpikeEngine engine;

    auto reader = openReader(engine.get(), toJuceFile(file), result.error);
    if (reader == nullptr)
        return result;

    std::ofstream out(rawOut, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        result.error = "cannot write " + rawOut.string();
        return result;
    }

    const auto numChannels = static_cast<int>(reader->numChannels);
    const juce::int64 length = reader->lengthInSamples;
    constexpr int kChunk = 16384;
    juce::AudioBuffer<float> buffer(numChannels, kChunk);
    std::vector<char> bytes(static_cast<std::size_t>(kChunk) * static_cast<std::size_t>(numChannels) * 4u);

    for (juce::int64 pos = 0; pos < length; pos += kChunk)
    {
        const auto num = static_cast<int>(std::min<juce::int64>(kChunk, length - pos));
        if (!reader->read(&buffer, 0, num, pos, true, true))
        {
            result.error = "decode error at sample " + std::to_string(pos) + " in " + file.string();
            return result;
        }

        // Interleaved little-endian float32, independent of the host byte order.
        std::size_t b = 0;
        for (int n = 0; n < num; ++n)
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const auto bits =
                    juce::ByteOrder::swapIfBigEndian(std::bit_cast<juce::uint32>(buffer.getSample(ch, n)));
                std::memcpy(bytes.data() + b, &bits, 4);
                b += 4;
            }
        out.write(bytes.data(), static_cast<std::streamsize>(b));
    }

    out.close();
    if (!out)
    {
        result.error = "write error in " + rawOut.string();
        return result;
    }

    result.ok = true;
    result.error.clear();
    result.sampleRate = reader->sampleRate;
    result.numChannels = numChannels;
    result.lengthSamples = length;
    return result;
}

}  // namespace spike
