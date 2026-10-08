// M1-06: the device setup survives a restart. "Restart" = destroy the engine, create a new one on the same settings
// folder (settings.xml of M1-01), add the (fake) device types again, restoreAudioDeviceSetup().
#include "io_test_session.h"

#include <memory>

namespace
{

using namespace tracklab_test::io_helpers;
using tracklab_test::ScopedTempDir;
namespace error_code = tracklab::core::error_code;
namespace fake = tracklab_test::fake;

juce::File settingsFile(const ScopedTempDir& temp)
{
    return temp.dir().getChildFile("settings.xml");
}

Json duplexParams()
{
    return Json{{"type", fake::kDuplexType},
                {"input_device", fake::kDuplexInterface},
                {"output_device", fake::kDuplexInterface},
                {"sample_rate", 44100},
                {"buffer_size", 64},
                {"active_input_channels", Json::array({1, 2})},
                {"active_output_channels", Json::array({4, 5})}};
}

Json withOpen(Json setup)
{
    setup["open"] = true;
    return setup;
}

}  // namespace

TEST_SUITE("io")
{
    TEST_CASE("the chosen device is restored after a restart: type, devices, rate, buffer size and channels")
    {
        const ScopedTempDir temp;
        {
            const Session first(temp.dir());
            first.setDevice(micAndSpeakersParams());
        }  // end of the run: the engine writes settings.xml

        const Session second(temp.dir(), {}, /*restore=*/true);
        CHECK(second.restoreError.isEmpty());
        CHECK(second.getDevice() == micAndSpeakersSetup());

        // The second run really opened the device again, as stored.
        REQUIRE_FALSE(second.backend->opens.empty());
        const auto& open = second.backend->opens.back();
        CHECK(open.inputDevice == fake::kMicInterface);
        CHECK(open.outputDevice == fake::kSpeakers);
        CHECK(open.sampleRate == doctest::Approx(48000.0));
        CHECK(open.bufferSize == 128);
        CHECK(open.inputChannels[0]);
        CHECK_FALSE(open.inputChannels[1]);
        CHECK(open.inputChannels[2]);
        CHECK(second.backend->callback != nullptr);
    }

    TEST_CASE("a device of another driver type is restored with its type")
    {
        const ScopedTempDir temp;
        {
            const Session first(temp.dir());
            first.setDevice(duplexParams());
        }

        const Session second(temp.dir(), {}, true);
        CHECK(second.getDevice() == withOpen(duplexParams()));
        CHECK(second.ok("io.list_device_types").at("current_type") == fake::kDuplexType);
    }

    TEST_CASE("an output-only setup is restored as output only")
    {
        const ScopedTempDir temp;
        Json expected;
        {
            const Session first(temp.dir());
            first.setDevice(micAndSpeakersParams());
            expected = first.setDevice(Json{{"input_device", ""}});
        }

        const Session second(temp.dir(), {}, true);
        CHECK(second.getDevice() == expected);
        CHECK(second.getDevice().at("input_device") == "");
    }

    TEST_CASE("the last successful io.set_device wins")
    {
        const ScopedTempDir temp;
        {
            const Session first(temp.dir());
            first.setDevice(micAndSpeakersParams());
            first.setDevice(duplexParams());
        }

        const Session second(temp.dir(), {}, true);
        CHECK(second.getDevice() == withOpen(duplexParams()));
    }

    TEST_CASE("a refused io.set_device does not change what is stored")
    {
        const ScopedTempDir temp;
        {
            const Session first(temp.dir());
            first.setDevice(micAndSpeakersParams());
            CHECK_FALSE(first.run("io.set_device", Json{{"sample_rate", 12345}}).ok);
            CHECK_FALSE(first.run("io.set_device", Json{{"output_device", "Muster Lautsprecher"}}).ok);
        }

        const Session second(temp.dir(), {}, true);
        CHECK(second.getDevice() == micAndSpeakersSetup());
    }

    TEST_CASE("the setup is in settings.xml as soon as the command returns, without a message loop")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());

        // No runDispatchLoop: Tracktion's own save runs on a change message, which has not been delivered here.
        session.engine->getPropertyStorage().flushSettingsToDisk();
        REQUIRE(settingsFile(temp).existsAsFile());
        const auto text = settingsFile(temp).loadFileAsString();
        CHECK(text.contains("audio_device_setup"));
        CHECK(text.contains(fake::kMicInterface));
        CHECK(text.contains(fake::kSpeakers));
        CHECK(text.contains(fake::kSeparateType));
    }

    TEST_CASE("settings.xml written by a flush alone is enough for the next start")
    {
        // The command stores the setup; flushing is the settings storage's job (M1-01). Here the first engine is still
        // alive (not destroyed, as after a crash) when the second one reads the file.
        const ScopedTempDir temp;
        const Session first(temp.dir());
        first.setDevice(micAndSpeakersParams());
        first.engine->getPropertyStorage().flushSettingsToDisk();

        const Session second(temp.dir(), {}, true);
        CHECK(second.getDevice() == micAndSpeakersSetup());
    }

    TEST_CASE("listing and reading devices store no device setup")
    {
        const ScopedTempDir temp;
        {
            const Session session(temp.dir());
            session.ok("io.list_device_types");
            session.ok("io.list_devices");
            session.ok("io.get_device");
        }

        const auto file = settingsFile(temp);
        const bool stored = file.existsAsFile() && file.loadFileAsString().contains("audio_device_setup");
        CHECK_FALSE(stored);

        const Session second(temp.dir(), {}, true);
        CHECK(second.restoreError.isEmpty());
        CHECK(second.getDevice() == noDeviceSetup());
        CHECK(second.backend->opens.empty());
    }

    TEST_CASE("restoring with nothing stored does nothing and reports no error")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir(), {}, true);
        CHECK(session.restoreError.isEmpty());
        CHECK(session.getDevice() == noDeviceSetup());
        CHECK(session.backend->devicesCreated == 0);
    }

    TEST_CASE("a stored device that is gone does not crash the restart and is not reported as open")
    {
        const ScopedTempDir temp;
        {
            const Session first(temp.dir());
            first.setDevice(micAndSpeakersParams());
        }

        // The next run has no "Fake Separate" type at all (the interface was unplugged / the backend is missing).
        fake::Hardware hardware;
        hardware.separateType = false;
        const Session second(temp.dir(), hardware, true);

        const auto setup = second.getDevice();
        CHECK(setup.at("input_device") != fake::kMicInterface);
        CHECK(setup.at("output_device") != fake::kSpeakers);
        CHECK(setup.at("type") != fake::kSeparateType);

        // The user can choose another device right away, and that is what is stored from now on.
        const auto chosen = second.setDevice(duplexParams());
        CHECK(chosen == withOpen(duplexParams()));
    }

    TEST_CASE("after a restore the device can be changed and the change is stored again")
    {
        const ScopedTempDir temp;
        {
            const Session first(temp.dir());
            first.setDevice(micAndSpeakersParams());
        }
        {
            const Session second(temp.dir(), {}, true);
            second.setDevice(Json{{"sample_rate", 96000}});
        }

        const Session third(temp.dir(), {}, true);
        Json expected = micAndSpeakersSetup();
        expected["sample_rate"] = 96000;
        CHECK(third.getDevice() == expected);
    }

    TEST_CASE("the io commands work on an engine with in-memory settings")
    {
        auto engine = tracklab::engine::createEngine(tracklab_test::testOptions());  // in-memory settings
        auto backend = std::make_shared<fake::Backend>();
        fake::addFakeTypes(tracklab::io::audioDeviceManager(*engine), backend);
        tracklab::core::CommandRegistry registry;
        REQUIRE(tracklab::io::registerIoCommands(registry, *engine).ok);

        const auto outcome = registry.execute("io.set_device", micAndSpeakersParams());
        INFO(outcome.error.code << ": " << outcome.error.message);
        REQUIRE(outcome.ok);
        CHECK(outcome.result == micAndSpeakersSetup());
    }
}
