// M1-06: io.set_device on fake hardware: choose type, devices, rate, buffer size and active channels; the device is
// really opened as asked; invalid values give a defined error and change nothing.
#include "io_test_session.h"

namespace
{

using namespace tracklab_test::io_helpers;
using tracklab_test::ScopedTempDir;
namespace error_code = tracklab::core::error_code;
namespace fake = tracklab_test::fake;

/** Whether exactly the channels in `indices` are set in `mask`. */
bool maskIs(const juce::BigInteger& mask, std::initializer_list<int> indices)
{
    juce::BigInteger expected;
    for (const int i : indices)
        expected.setBit(i);
    return mask == expected;
}

/** Runs io.set_device with `params`, expects handler_failed with `part` (lower case) in the message, and that neither
    the open device nor the number of device opens changed. */
void expectRefused(const Session& session, const Json& params, const std::string& part)
{
    const auto before = session.getDevice();
    const auto opensBefore = session.backend->opens.size();

    const auto outcome = session.run("io.set_device", params);
    INFO("params " << params.dump() << " -> " << outcome.error.code << ": " << outcome.error.message);
    CHECK_FALSE(outcome.ok);
    CHECK(outcome.error.code == error_code::handlerFailed);
    CHECK(contains(lower(outcome.error.message), lower(part)));

    CHECK(session.getDevice() == before);
    CHECK(session.backend->opens.size() == opensBefore);
}

}  // namespace

TEST_SUITE("io")
{
    TEST_CASE("io.set_device opens the chosen devices with the chosen rate, buffer size and channels")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());

        const auto result = session.setDevice(micAndSpeakersParams());
        CHECK(result == micAndSpeakersSetup());

        // The fake hardware was asked for exactly this.
        REQUIRE_FALSE(session.backend->opens.empty());
        const auto& open = session.backend->opens.back();
        CHECK(open.inputDevice == fake::kMicInterface);
        CHECK(open.outputDevice == fake::kSpeakers);
        CHECK(open.sampleRate == doctest::Approx(48000.0));
        CHECK(open.bufferSize == 128);
        CHECK(maskIs(open.inputChannels, {0, 2}));
        CHECK(maskIs(open.outputChannels, {0, 1}));

        // And it is running.
        CHECK(session.backend->callback != nullptr);
        CHECK(session.deviceManager().getCurrentAudioDevice() != nullptr);
    }

    TEST_CASE("io.get_device returns what io.set_device set")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());

        session.setDevice(micAndSpeakersParams());
        CHECK(session.getDevice() == micAndSpeakersSetup());
    }

    TEST_CASE("io.set_device changes only the sample rate: devices, buffer size and channels stay")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());

        const auto result = session.setDevice(Json{{"sample_rate", 44100}});

        Json expected = micAndSpeakersSetup();
        expected["sample_rate"] = 44100;
        CHECK(result == expected);
        CHECK(session.getDevice() == expected);
        const auto& open = session.backend->opens.back();
        CHECK(open.sampleRate == doctest::Approx(44100.0));
        CHECK(open.bufferSize == 128);
        CHECK(maskIs(open.inputChannels, {0, 2}));
    }

    TEST_CASE("io.set_device changes only the buffer size")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());

        const auto result = session.setDevice(Json{{"buffer_size", 512}});

        Json expected = micAndSpeakersSetup();
        expected["buffer_size"] = 512;
        CHECK(result == expected);
        CHECK(session.backend->opens.back().bufferSize == 512);
    }

    TEST_CASE("io.set_device changes only the active channels")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());

        const auto result = session.setDevice(
            Json{{"active_input_channels", Json::array({1, 3})}, {"active_output_channels", Json::array({1})}});

        Json expected = micAndSpeakersSetup();
        expected["active_input_channels"] = Json::array({1, 3});
        expected["active_output_channels"] = Json::array({1});
        CHECK(result == expected);
        const auto& open = session.backend->opens.back();
        CHECK(maskIs(open.inputChannels, {1, 3}));
        CHECK(maskIs(open.outputChannels, {1}));
    }

    TEST_CASE("io.set_device switches the output device and keeps the input device")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());

        const auto result =
            session.setDevice(Json{{"output_device", fake::kHeadphones}, {"sample_rate", 48000}, {"buffer_size", 256}});

        CHECK(result.at("output_device") == fake::kHeadphones);
        CHECK(result.at("input_device") == fake::kMicInterface);
        CHECK(result.at("sample_rate") == 48000);
        CHECK(result.at("buffer_size") == 256);
        CHECK(session.backend->opens.back().outputDevice == fake::kHeadphones);
    }

    TEST_CASE("io.set_device with an empty input device runs output only")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());

        const auto result = session.setDevice(Json{{"input_device", ""}});

        CHECK(result.at("open") == true);
        CHECK(result.at("input_device") == "");
        CHECK(result.at("active_input_channels").empty());
        CHECK(result.at("output_device") == fake::kSpeakers);
        CHECK(result.at("active_output_channels") == Json::array({0, 1}));
        CHECK(session.getDevice() == result);
    }

    TEST_CASE("io.set_device switches the driver type with its devices")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());

        const Json duplex{{"type", fake::kDuplexType},
                          {"input_device", fake::kDuplexInterface},
                          {"output_device", fake::kDuplexInterface},
                          {"sample_rate", 44100},
                          {"buffer_size", 64},
                          {"active_input_channels", Json::array({0, 1, 2, 3, 4, 5, 6, 7})},
                          {"active_output_channels", Json::array({6, 7})}};
        const auto result = session.setDevice(duplex);

        Json expected = duplex;
        expected["open"] = true;
        CHECK(result == expected);
        CHECK(session.getDevice() == expected);
        CHECK(session.ok("io.list_device_types").at("current_type") == fake::kDuplexType);
        const auto& open = session.backend->opens.back();
        CHECK(open.inputDevice == fake::kDuplexInterface);
        CHECK(maskIs(open.outputChannels, {6, 7}));
    }

    TEST_CASE("io.set_device with only a type selects the default devices of that type")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());

        const auto result = session.setDevice(Json{{"type", fake::kDuplexType}});

        CHECK(result.at("open") == true);
        CHECK(result.at("type") == fake::kDuplexType);
        CHECK(result.at("input_device") == fake::kDuplexInterface);
        CHECK(result.at("output_device") == fake::kDuplexInterface);
        CHECK(result.at("sample_rate").get<double>() > 0.0);
        CHECK(result.at("buffer_size").get<int>() > 0);
    }

    //==========================================================================
    // Active channels that are not given (Lead decision 5, M1-06): a new device gets all its channels, an unchanged
    // device keeps the channels it had.
    TEST_CASE("io.set_device without channels activates all channels of a new device")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());

        const auto result = session.setDevice(Json{{"type", fake::kSeparateType},
                                                   {"input_device", fake::kMicInterface},
                                                   {"output_device", fake::kSpeakers},
                                                   {"sample_rate", 48000},
                                                   {"buffer_size", 128}});

        CHECK(result.at("active_input_channels") == Json::array({0, 1, 2, 3}));  // mic interface: 4 channels
        CHECK(result.at("active_output_channels") == Json::array({0, 1}));
        const auto& open = session.backend->opens.back();
        CHECK(maskIs(open.inputChannels, {0, 1, 2, 3}));
        CHECK(maskIs(open.outputChannels, {0, 1}));
    }

    TEST_CASE("io.set_device with only a type activates all channels of the default devices")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());  // channels 0 and 2 in, 0 and 1 out

        const auto result = session.setDevice(Json{{"type", fake::kDuplexType}});

        CHECK(result.at("active_input_channels") == Json::array({0, 1, 2, 3, 4, 5, 6, 7}));
        CHECK(result.at("active_output_channels") == Json::array({0, 1, 2, 3, 4, 5, 6, 7}));
    }

    TEST_CASE("io.set_device keeps the channels of the side whose device stays, the new side gets all channels")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());  // mic: channels 0 and 2

        const auto result =
            session.setDevice(Json{{"output_device", fake::kHeadphones}, {"sample_rate", 48000}, {"buffer_size", 256}});

        CHECK(result.at("active_input_channels") == Json::array({0, 2}));   // same input device: as it was
        CHECK(result.at("active_output_channels") == Json::array({0, 1}));  // new output device: all
    }

    TEST_CASE("io.set_device gives all channels to a device that was added to an output-only setup")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());
        session.setDevice(Json{{"input_device", ""}});

        const auto result = session.setDevice(Json{{"input_device", fake::kLineInterface}});

        CHECK(result.at("active_input_channels") == Json::array({0, 1}));  // line interface: 2 channels
        CHECK(result.at("active_output_channels") == Json::array({0, 1}));
    }

    TEST_CASE("the running fake device reaches the audio callback of the device manager")
    {
        // The device manager forwards the device's callback to its own callbacks: audio can flow once a device is set.
        struct Counter final : juce::AudioIODeviceCallback
        {
            void audioDeviceIOCallbackWithContext(const float* const*, int, float* const* out, int numOut,
                                                  int numSamples, const juce::AudioIODeviceCallbackContext&) override
            {
                ++blocks;
                frames += numSamples;
                outputs = numOut;
                for (int c = 0; c < numOut; ++c)
                    juce::FloatVectorOperations::clear(out[c], numSamples);
            }
            void audioDeviceAboutToStart(juce::AudioIODevice* device) override
            {
                rate = device->getCurrentSampleRate();
            }
            void audioDeviceStopped() override {}

            int blocks = 0;
            int frames = 0;
            int outputs = 0;
            double rate = 0.0;
        } counter;

        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.deviceManager().addAudioCallback(&counter);
        session.setDevice(micAndSpeakersParams());
        REQUIRE(session.backend->callback != nullptr);
        CHECK(counter.rate == doctest::Approx(48000.0));

        std::vector<float> in0(128), in1(128), out0(128), out1(128);
        const float* inputs[2] = {in0.data(), in1.data()};
        float* outputs[2] = {out0.data(), out1.data()};
        session.backend->callback->audioDeviceIOCallbackWithContext(inputs, 2, outputs, 2, 128, {});
        CHECK(counter.blocks == 1);
        CHECK(counter.frames == 128);
        CHECK(counter.outputs == 2);

        session.deviceManager().removeAudioCallback(&counter);
    }

    //==========================================================================
    // Invalid values -> defined errors, nothing changes
    TEST_CASE("io.set_device with an unknown type fails and names the type")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());

        expectRefused(session, Json{{"type", "Muster Driver"}}, "Muster Driver");
    }

    TEST_CASE("io.set_device with an unknown output device fails and names the device")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());

        expectRefused(session, Json{{"output_device", "Muster Lautsprecher"}}, "Muster Lautsprecher");
    }

    TEST_CASE("io.set_device with an unknown input device fails and names the device")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());

        expectRefused(session, Json{{"input_device", "Muster Mikrofon"}}, "Muster Mikrofon");
    }

    TEST_CASE("io.set_device with an unknown device also fails when no device is open yet")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());

        expectRefused(session, Json{{"type", fake::kSeparateType}, {"output_device", "Muster Lautsprecher"}},
                      "Muster Lautsprecher");
        CHECK(session.getDevice() == noDeviceSetup());
    }

    TEST_CASE("io.set_device does not accept a device of another type")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());

        expectRefused(session, Json{{"type", fake::kDuplexType}, {"output_device", fake::kSpeakers}}, fake::kSpeakers);
    }

    TEST_CASE("io.set_device refuses a sample rate the device does not offer, it does not round to the nearest")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());

        for (const int rate : {22050, 88200, 192000})
        {
            CAPTURE(rate);
            expectRefused(session, Json{{"sample_rate", rate}}, "sample rate");
            expectRefused(session, Json{{"sample_rate", rate}}, std::to_string(rate));
        }
        // A rate with a fraction is a different rate.
        expectRefused(session, Json{{"sample_rate", 44100.5}}, "sample rate");
    }

    TEST_CASE("io.set_device checks the sample rate against the chosen device, not against the previous one")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());  // speakers: 44.1/48/96 kHz

        // The headphones run at 48 kHz only.
        expectRefused(session, Json{{"output_device", fake::kHeadphones}, {"sample_rate", 44100}, {"buffer_size", 256}},
                      "sample rate");
        expectRefused(session, Json{{"output_device", fake::kHeadphones}, {"sample_rate", 96000}, {"buffer_size", 256}},
                      "sample rate");
    }

    TEST_CASE("io.set_device checks the buffer size against the chosen output device too")
    {
        // The headphones offer 256 frames only, whatever the input device offers.
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());

        expectRefused(session, Json{{"output_device", fake::kHeadphones}, {"sample_rate", 48000}, {"buffer_size", 128}},
                      "buffer size");
    }

    TEST_CASE("io.set_device refuses a buffer size the device does not offer")
    {
        // Lead decision 2 (M1-06): an error like the sample rate, no silent rounding (JUCE would round).
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());

        for (const int size : {100, 1024, 16})
        {
            CAPTURE(size);
            expectRefused(session, Json{{"buffer_size", size}}, "buffer size");
            expectRefused(session, Json{{"buffer_size", size}}, std::to_string(size));
        }
    }

    TEST_CASE("io.set_device refuses a channel index beyond the channels of the device")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());

        expectRefused(session, Json{{"active_output_channels", Json::array({0, 2})}}, "channel");  // speakers: 0..1
        expectRefused(session, Json{{"active_input_channels", Json::array({1, 4})}}, "channel");   // mic: 0..3
        expectRefused(session, Json{{"active_input_channels", Json::array({9})}}, "9");
    }

    TEST_CASE("a refused io.set_device leaves the running device running")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());

        expectRefused(session, Json{{"sample_rate", 12345}}, "sample rate");
        expectRefused(session, Json{{"output_device", "Muster Lautsprecher"}}, "Muster Lautsprecher");

        CHECK(session.backend->callback != nullptr);
        CHECK(session.deviceManager().getCurrentAudioDevice() != nullptr);
        CHECK(session.getDevice() == micAndSpeakersSetup());
    }

    TEST_CASE("a refused request with several faults changes nothing, also not the valid parts")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());

        // Valid new buffer size and channels, but an unknown output device.
        expectRefused(session,
                      Json{{"buffer_size", 512},
                           {"active_input_channels", Json::array({3})},
                           {"output_device", "Muster Lautsprecher"}},
                      "Muster Lautsprecher");
    }

    TEST_CASE("io.set_device takes channels and rates of the unchanged side from the open device (exclusive hardware)")
    {
        // A device that is created while its hardware is open reports nothing (like ALSA hw: or WASAPI exclusive).
        // The input stays the mic interface: its channels must come from the open device, not from such a probe.
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.backend->exclusive = true;
        session.setDevice(micAndSpeakersParams());

        const auto result =
            session.setDevice(Json{{"output_device", fake::kHeadphones}, {"sample_rate", 48000}, {"buffer_size", 256}});
        CHECK(result.at("output_device") == fake::kHeadphones);
        CHECK(result.at("input_device") == fake::kMicInterface);
        CHECK(result.at("active_input_channels") == Json::array({0, 2}));
        CHECK(result.at("active_output_channels") == Json::array({0, 1}));

        REQUIRE_FALSE(session.backend->opens.empty());
        CHECK(maskIs(session.backend->opens.back().inputChannels, {0, 2}));
        CHECK(session.getDevice() == result);
    }

    TEST_CASE("io.set_device keeps the unchanged side when only the input changes (exclusive hardware)")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.backend->exclusive = true;
        session.setDevice(micAndSpeakersParams());

        const auto result = session.setDevice(Json{{"input_device", fake::kLineInterface}});
        CHECK(result.at("input_device") == fake::kLineInterface);
        CHECK(result.at("active_input_channels") == Json::array({0, 1}));  // new device: all its channels
        CHECK(result.at("output_device") == fake::kSpeakers);
        CHECK(result.at("active_output_channels") == Json::array({0, 1}));
        CHECK(result.at("sample_rate") == 48000);
        CHECK(result.at("buffer_size") == 128);
    }

    TEST_CASE("io.set_device refuses a device that reports no channels, never answers ok")
    {
        // Another program holds the line interface: the probe sees no channels. The output would be silent or the
        // input dead, so this is an error with a text that names the device and the likely cause.
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.backend->exclusive = true;
        session.setDevice(micAndSpeakersParams());
        session.backend->busy.emplace_back(fake::kLineInterface);

        expectRefused(session, Json{{"input_device", fake::kLineInterface}}, "no channels");
        expectRefused(session, Json{{"input_device", fake::kLineInterface}}, fake::kLineInterface);
    }
}
