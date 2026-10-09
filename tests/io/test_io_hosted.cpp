// M1-06: engine round trip through a hosted audio device (te::HostedAudioDeviceInterface, as in the engine spike
// spike/engine/src/spike_record12.cpp): the test plays the role of the sound card and drives the audio callback block
// by block, so the whole path DeviceManager -> playback graph -> output runs headless and deterministic.
// The io commands see this device like any other.
#include "io_test_session.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{

using namespace tracklab_test::io_helpers;
using tracklab_test::ScopedTempDir;
namespace error_code = tracklab::core::error_code;
namespace te = tracklab::engine::te;

constexpr double kRate = 48000.0;
constexpr int kBlock = 512;
constexpr double kToneAmplitude = 0.5;
constexpr double kToneHz = 440.0;

/** Restores the device manager when the test ends (same teardown as the spike). */
struct HostedDeviceScope
{
    te::DeviceManager& dm;
    ~HostedDeviceScope()
    {
        dm.deviceManager.closeAudioDevice();
        dm.removeHostedAudioDeviceInterface();
    }
};

/** Writes `seconds` of a mono 16 bit sine to `file`. */
bool writeTone(const juce::File& file, double seconds)
{
    juce::WavAudioFormat wav;
    auto stream = std::make_unique<juce::FileOutputStream>(file);
    if (stream->failedToOpen())
        return false;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), kRate, 1, 16, {}, 0));
    if (writer == nullptr)
        return false;
    [[maybe_unused]] auto* const released = stream.release();  // the writer owns the stream now

    const auto frames = static_cast<int>(seconds * kRate);
    juce::AudioBuffer<float> buffer(1, frames);
    for (int n = 0; n < frames; ++n)
        buffer.setSample(
            0, n, static_cast<float>(kToneAmplitude * std::sin(2.0 * 3.14159265358979323846 * kToneHz * n / kRate)));
    return writer->writeFromAudioSampleBuffer(buffer, 0, frames);
}

/** The engine, a registry with the io commands and a hosted device (2 in, 2 out, 48 kHz, 512 frames). */
struct HostedRig
{
    HostedRig()
        : engine(tracklab::engine::createEngine(tracklab_test::testOptions())), dm(engine->getDeviceManager()),
          scope{dm}
    {
        // Blocks are pumped faster than real time; without this the device manager mutes them (as in the spike).
        dm.setCpuLimitBeforeMuting(1000.0);

        auto& audioIO = dm.getHostedAudioDeviceInterface();
        te::HostedAudioDeviceInterface::Parameters params;
        params.sampleRate = kRate;
        params.blockSize = kBlock;
        params.inputChannels = 2;
        params.outputChannels = 2;
        audioIO.initialise(params);
        audioIO.prepareToPlay(kRate, kBlock);
        dm.dispatchPendingUpdates();

        REQUIRE(tracklab::io::registerIoCommands(registry, *engine).ok);
    }

    /** Feeds `numBlocks` blocks of silence on the inputs and returns the peak of the outputs. */
    float pump(int numBlocks)
    {
        auto& audioIO = dm.getHostedAudioDeviceInterface();
        juce::AudioBuffer<float> buffer(2, kBlock);
        juce::MidiBuffer midi;
        float peak = 0.0f;
        for (int b = 0; b < numBlocks; ++b)
        {
            buffer.clear();
            audioIO.processBlock(buffer, midi);
            peak = std::max({peak, buffer.getMagnitude(0, 0, kBlock), buffer.getMagnitude(1, 0, kBlock)});
        }
        return peak;
    }

    std::unique_ptr<te::Engine> engine;
    te::DeviceManager& dm;
    HostedDeviceScope scope;
    tracklab::core::CommandRegistry registry;
};

}  // namespace

TEST_SUITE("io")
{
    TEST_CASE("io.list_device_types lists the hosted device and io.get_device describes it")
    {
        HostedRig rig;

        const auto types = rig.registry.execute("io.list_device_types", Json::object());
        REQUIRE(types.ok);
        bool found = false;
        for (const auto& type : types.result.at("types"))
            found = found || type.at("name") == "Hosted Device";
        CHECK(found);
        CHECK(types.result.at("current_type") == "Hosted Device");

        const auto device = rig.registry.execute("io.get_device", Json::object());
        INFO(device.error.code << ": " << device.error.message);
        REQUIRE(device.ok);
        CHECK(device.result.at("open") == true);
        CHECK(device.result.at("type") == "Hosted Device");
        CHECK(device.result.at("output_device") == "Hosted Device");
        CHECK(device.result.at("sample_rate") == kRate);
        CHECK(device.result.at("buffer_size") == kBlock);
        CHECK(device.result.at("active_input_channels") == Json::array({0, 1}));
        CHECK(device.result.at("active_output_channels") == Json::array({0, 1}));
    }

    TEST_CASE("io.list_devices offers the hosted device with exactly its own rate and buffer size")
    {
        HostedRig rig;

        const auto devices = rig.registry.execute("io.list_devices", Json{{"type", "Hosted Device"}});
        INFO(devices.error.code << ": " << devices.error.message);
        REQUIRE(devices.ok);
        REQUIRE(devices.result.at("types").size() == 1);
        const auto& type = devices.result.at("types")[0];
        REQUIRE(type.at("outputs").size() == 1);
        CHECK(type.at("outputs")[0].at("name") == "Hosted Device");
        CHECK(type.at("outputs")[0].at("sample_rates") == Json::array({48000}));
        CHECK(type.at("outputs")[0].at("buffer_sizes") == Json::array({kBlock}));
        CHECK(type.at("outputs")[0].at("channel_names").size() == 2);
    }

    TEST_CASE("io.set_device refuses a rate the hosted device does not offer and leaves it as it was")
    {
        HostedRig rig;
        const auto before = rig.registry.execute("io.get_device", Json::object());
        REQUIRE(before.ok);

        const auto outcome = rig.registry.execute("io.set_device", Json{{"sample_rate", 44100}});
        CHECK_FALSE(outcome.ok);
        CHECK(outcome.error.code == error_code::handlerFailed);
        CHECK(contains(lower(outcome.error.message), "sample rate"));

        const auto after = rig.registry.execute("io.get_device", Json::object());
        REQUIRE(after.ok);
        CHECK(after.result == before.result);
    }

    TEST_CASE("engine round trip: an edit with a tone plays through the hosted device; silent before and after")
    {
        const ScopedTempDir temp;
        const auto toneFile = temp.dir().getChildFile("tone.wav");
        REQUIRE(writeTone(toneFile, 1.0));

        HostedRig rig;
        auto edit = te::Edit::createSingleTrackEdit(*rig.engine, te::Edit::EditRole::forEditing);
        REQUIRE(edit != nullptr);
        edit->getMasterVolumePlugin()->setVolumeDb(0.0f);
        auto* track = te::getAudioTracks(*edit)[0];

        te::ClipPosition position;
        position.time = te::TimeRange(te::TimePosition(), te::TimeDuration::fromSeconds(1.0));
        auto clip = te::insertWaveClip(*track, "tone", toneFile, position, te::DeleteExistingClips::yes);
        REQUIRE(clip != nullptr);

        auto& transport = edit->getTransport();
        transport.ensureContextAllocated();
        edit->dispatchPendingUpdatesSynchronously();

        // Stopped: the device runs, nothing plays.
        CHECK(rig.pump(4) == doctest::Approx(0.0f).epsilon(1.0e-6));

        // Playing: the tone reaches the output of the device (peak about the amplitude of the tone, within the gain
        // of the mixer: far from silence, never above full scale).
        transport.setPosition(te::TimePosition());
        transport.play(false);
        edit->dispatchPendingUpdatesSynchronously();
        const float playing = rig.pump(40);  // about 0.43 s
        CHECK(playing > 0.1f);
        CHECK(playing <= 1.0f);

        // Stopped again: silent.
        transport.stop(false, false);
        edit->dispatchPendingUpdatesSynchronously();
        rig.pump(8);  // tail of the last block
        CHECK(rig.pump(4) == doctest::Approx(0.0f).epsilon(1.0e-6));

        // The io commands did not disturb it: the device is still the hosted one.
        const auto device = rig.registry.execute("io.get_device", Json::object());
        REQUIRE(device.ok);
        CHECK(device.result.at("type") == "Hosted Device");
    }
}
