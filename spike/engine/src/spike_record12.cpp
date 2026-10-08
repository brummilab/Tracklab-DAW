// record-12: 12 mono inputs of a hosted audio device recorded on 12 tracks.
//
// The HostedAudioDeviceInterface lets the caller drive the engine's audio callback instead of a sound card, so
// the recording path is exactly the one a real device would use (DeviceManager -> WaveInputDevice -> recording
// thread -> file), but deterministic and headless. The driver below plays the role of the device driver: it
// produces the documented input signals block by block and calls processBlock(). Like a real device callback it
// does not allocate, lock or do IO; all buffers are created before the first block.
#include <algorithm>
#include <array>
#include <cmath>

#include "spike_common.h"
#include "spike_engine.h"
#include "spike_realtime.h"

namespace spike
{

namespace
{

namespace te = tracktion;
using detail::SpikeEngine;
using detail::toJuceFile;
using detail::toStd;
using detail::toStdPath;

constexpr double kPi = 3.14159265358979323846;
constexpr double kMaxRecordSeconds = 600.0;  // keeps a typo in --seconds from filling the disk

/** Reference input signal of channel `channel` at frame `n` (spike_common.h), rounded to float like the device. */
float inputSample(double omega, std::int64_t n) noexcept
{
    return static_cast<float>(kRecordInputAmplitude * std::sin(omega * static_cast<double>(n)));
}

/** The simulated device: owns the block buffers and feeds the engine one block per call. */
class HostedDeviceDriver
{
public:
    explicit HostedDeviceDriver(te::HostedAudioDeviceInterface& io)
        : audioIO(io), block(kRecordNumInputs, kRecordBlockSize)
    {
        block.clear();
        for (int c = 0; c < kRecordNumInputs; ++c)
            omega[static_cast<std::size_t>(c)] = 2.0 * kPi * recordInputFrequencyHz(c) / kRecordSampleRate;
    }

    /** Feeds `numFrames` frames: the input signals from frame `startFrame` on, or silence if `silent`.
        Runs on the "audio thread" of the simulated device: no allocation, no locks, no IO in this function. */
    void feed(std::int64_t startFrame, int numFrames, bool silent) noexcept SPIKE_NONBLOCKING
    {
        float* const* channels = block.getArrayOfWritePointers();
        for (int c = 0; c < kRecordNumInputs; ++c)
        {
            float* data = channels[c];
            const double w = omega[static_cast<std::size_t>(c)];
            for (int i = 0; i < numFrames; ++i)
                data[i] = silent ? 0.0f : inputSample(w, startFrame + i);
        }

        // A short last block is passed as a view on the same memory (no allocation: AudioBuffer keeps up to 32
        // channel pointers inline). The engine writes its output into the same buffer, like a duplex device.
        juce::AudioBuffer<float> view(channels, kRecordNumInputs, numFrames);
        audioIO.processBlock(numFrames == kRecordBlockSize ? block : view, midi);
    }

private:
    te::HostedAudioDeviceInterface& audioIO;
    juce::AudioBuffer<float> block;
    juce::MidiBuffer midi;
    std::array<double, kRecordNumInputs> omega{};
};

/** Restores the device manager when record12 returns (same teardown as Tracktion's EnginePlayer). */
struct HostedDeviceScope
{
    te::DeviceManager& dm;
    ~HostedDeviceScope()
    {
        dm.deviceManager.closeAudioDevice();
        dm.removeHostedAudioDeviceInterface();
    }
};

/** Verifies one recording against the reference signal; adds the indices of bad blocks to `badBlocks`. */
bool verifyRecording(te::Engine& engine, const juce::File& file, int channel, std::int64_t expectedFrames,
                     std::vector<bool>& badBlocks, double& worstAbs, std::int64_t& length, std::string& error)
{
    juce::AudioFormat* format = nullptr;
    std::unique_ptr<juce::AudioFormatReader> reader(
        te::AudioFileUtils::createReaderFindingFormat(engine, file, format));
    if (reader == nullptr)
    {
        error = "cannot read recording " + toStd(file.getFullPathName());
        return false;
    }
    if (reader->numChannels != 1)
    {
        error = "recording of input " + std::to_string(channel + 1) + " is not mono";
        return false;
    }

    length = reader->lengthInSamples;
    const double tolerance = std::pow(10.0, kRecordToleranceDb / 20.0);
    const double w = 2.0 * kPi * recordInputFrequencyHz(channel) / kRecordSampleRate;
    juce::AudioBuffer<float> buffer(1, kRecordBlockSize);

    for (std::int64_t start = 0, b = 0; start < expectedFrames; start += kRecordBlockSize, ++b)
    {
        const auto num = static_cast<int>(std::min<std::int64_t>(kRecordBlockSize, expectedFrames - start));
        const auto available = static_cast<int>(std::clamp<std::int64_t>(length - start, 0, num));
        buffer.clear();
        if (available > 0)
            reader->read(&buffer, 0, available, start, true, false);

        double blockWorst = available < num ? 1.0 : 0.0;  // missing frames count as a missing block
        for (int i = 0; i < available; ++i)
        {
            const double d =
                std::abs(static_cast<double>(buffer.getSample(0, i)) - static_cast<double>(inputSample(w, start + i)));
            blockWorst = std::max(blockWorst, d);
        }
        worstAbs = std::max(worstAbs, blockWorst);
        if (blockWorst > tolerance)
            badBlocks[static_cast<std::size_t>(b)] = true;
    }
    return true;
}

}  // namespace

Record12Result record12(double seconds, const std::filesystem::path& outDir)
{
    Record12Result result;

    if (!(seconds > 0.0) || seconds > kMaxRecordSeconds)
    {
        result.error = "--seconds must be > 0 and <= " + std::to_string(static_cast<int>(kMaxRecordSeconds));
        return result;
    }
    const auto totalFrames = static_cast<std::int64_t>(std::llround(seconds * kRecordSampleRate));

    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    const auto dir = toJuceFile(std::filesystem::absolute(outDir, ec));
    if (!dir.isDirectory())
    {
        result.error = "cannot create the output directory " + outDir.string();
        return result;
    }

    SpikeEngine engine;
    engine.setRecordingDirectory(dir);
    auto& dm = engine.get().getDeviceManager();

    // Blocks are pumped faster than real time. Without this the DeviceManager mutes blocks once its CPU budget
    // per block is exceeded (same setting as Tracktion's own EnginePlayer test harness).
    dm.setCpuLimitBeforeMuting(1000.0);

    auto& audioIO = dm.getHostedAudioDeviceInterface();
    const HostedDeviceScope deviceScope{dm};
    te::HostedAudioDeviceInterface::Parameters params;
    params.sampleRate = kRecordSampleRate;
    params.blockSize = kRecordBlockSize;
    params.inputChannels = kRecordNumInputs;
    params.outputChannels = 2;
    audioIO.initialise(params);
    audioIO.prepareToPlay(kRecordSampleRate, kRecordBlockSize);
    dm.dispatchPendingUpdates();

    // HostedAudioDeviceInterface::initialise makes every input a mono device.
    auto waveInputs = dm.getWaveInputDevices();
    if (waveInputs.size() != static_cast<std::size_t>(kRecordNumInputs))
    {
        result.error = "the hosted device has " + std::to_string(waveInputs.size()) + " wave inputs instead of " +
                       std::to_string(kRecordNumInputs);
        return result;
    }
    for (auto* input : waveInputs)
        input->setMonitorMode(te::InputDevice::MonitorMode::off);  // record only, no input monitoring
    result.numInputs = static_cast<int>(waveInputs.size());

    auto edit = te::Edit::createSingleTrackEdit(engine.get(), te::Edit::EditRole::forEditing);
    if (edit == nullptr)
    {
        result.error = "could not create an edit";
        return result;
    }
    edit->ensureNumberOfAudioTracks(kRecordNumInputs);
    auto& transport = edit->getTransport();
    transport.ensureContextAllocated();
    auto* context = edit->getCurrentPlaybackContext();
    const auto tracks = te::getAudioTracks(*edit);
    if (context == nullptr || tracks.size() != kRecordNumInputs)
    {
        result.error = "could not set up 12 tracks with a playback context";
        return result;
    }
    result.numTracks = tracks.size();

    // Input c -> track c, armed.
    for (int c = 0; c < kRecordNumInputs; ++c)
    {
        te::InputDeviceInstance* instance = nullptr;
        for (auto* candidate : context->getAllInputs())
            if (&candidate->owner == waveInputs[static_cast<std::size_t>(c)])
                instance = candidate;

        if (instance == nullptr)
        {
            result.error = "input " + std::to_string(c + 1) + " has no instance in the playback context";
            return result;
        }
        auto destination = instance->setTarget(tracks[c]->itemID, true, nullptr);
        if (!destination)
        {
            result.error = "cannot assign input " + std::to_string(c + 1) + ": " + toStd(destination.error());
            return result;
        }
        (*destination)->recordEnabled = true;
    }
    edit->dispatchPendingUpdatesSynchronously();

    transport.record(false);
    if (!transport.isRecording())
    {
        const auto warning = engine.takeLastWarning();
        result.error = "recording did not start" + (warning.isNotEmpty() ? ": " + toStd(warning) : std::string());
        return result;
    }

    // Drive the device. The engine drops the first `latency` samples of every recording to compensate the graph
    // latency, so the same number of silent samples is fed at the end to complete the recording.
    HostedDeviceDriver driver(audioIO);
    for (std::int64_t frame = 0; frame < totalFrames; frame += kRecordBlockSize)
        driver.feed(frame, static_cast<int>(std::min<std::int64_t>(kRecordBlockSize, totalFrames - frame)), false);

    result.graphLatencySamples = context->getLatencySamples();
    for (int left = result.graphLatencySamples; left > 0; left -= kRecordBlockSize)
        driver.feed(0, std::min(left, kRecordBlockSize), true);

    transport.stop(false, true);

    // Collect the recordings: one wave clip per track.
    std::vector<juce::File> recorded;
    for (int c = 0; c < kRecordNumInputs; ++c)
    {
        auto* clip = dynamic_cast<te::WaveAudioClip*>(tracks[c]->getClips().getFirst());
        if (clip == nullptr)
        {
            const auto warning = engine.takeLastWarning();
            result.error = "track " + std::to_string(c + 1) + " has no recording" +
                           (warning.isNotEmpty() ? ": " + toStd(warning) : std::string());
            return result;
        }
        if (c == 0)
            result.clipStartSamples = te::toSamples(clip->getPosition().getStart(), kRecordSampleRate);
        recorded.push_back(clip->getSourceFileReference().getFile());
    }

    // Compare with the input signals. A block is "missing" if any channel lacks frames there or deviates.
    const auto numBlocks = static_cast<std::size_t>((totalFrames + kRecordBlockSize - 1) / kRecordBlockSize);
    std::vector<bool> badBlocks(numBlocks, false);
    double worstAbs = 0.0;
    for (int c = 0; c < kRecordNumInputs; ++c)
    {
        std::int64_t length = 0;
        if (!verifyRecording(engine.get(), recorded[static_cast<std::size_t>(c)], c, totalFrames, badBlocks, worstAbs,
                             length, result.error))
            return result;
        if (c == 0)
            result.lengthSamples = length;
        else if (length != result.lengthSamples)
        {
            result.error = "the recordings have different lengths";
            return result;
        }
        result.files.push_back(toStdPath(recorded[static_cast<std::size_t>(c)]));
    }

    result.ok = true;
    result.error.clear();
    result.missingBlocks = static_cast<int>(std::count(badBlocks.begin(), badBlocks.end(), true));
    // 20*log10(0) is -inf, which JSON cannot carry: identical recordings report the floor of -200 dB.
    result.worstDeviationDb = 20.0 * std::log10(std::max(worstAbs, 1.0e-10));
    return result;
}

}  // namespace spike
