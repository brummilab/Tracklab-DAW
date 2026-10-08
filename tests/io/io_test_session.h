// Helpers of the io tests (M1-06): an engine without system audio devices, with the fake device types of
// fake_audio_backend.h, its settings in a temporary folder, and a registry with the io.* commands.
#pragma once

#include "core/command.h"
#include "core/command_registry.h"
#include "engine/engine_factory.h"
#include "io/audio_devices.h"

#include "engine/engine_test_options.h"
#include "fake_audio_backend.h"
#include "test_support.h"

#include <cctype>
#include <memory>
#include <string>

namespace tracklab_test::io_helpers
{

using tracklab::core::CommandResult;
using tracklab::core::Json;

/** Engine options of a test that keeps its settings in `settingsDirectory` (settings.xml), DeviceMode::none. */
inline tracklab::engine::EngineOptions fileSettingsOptions(const juce::File& settingsDirectory)
{
    auto options = testOptions();
    options.storage = tracklab::engine::SettingsStorage::file;
    options.settingsDirectory = settingsDirectory;
    return options;
}

/** One "run of the application": engine, fake hardware, registry. Destroying it is the end of the run (the engine
    writes its settings); a second session on the same folder is the next start. */
class Session
{
public:
    /** `restore`: restoreAudioDeviceSetup() right after the fake types were added (what DeviceMode::automatic does
        when the engine is created). */
    explicit Session(const juce::File& settingsDirectory, const fake::Hardware& hardware = {}, bool restore = false)
        : backend(std::make_shared<fake::Backend>()),
          engine(tracklab::engine::createEngine(fileSettingsOptions(settingsDirectory)))
    {
        REQUIRE(engine != nullptr);
        fake::addFakeTypes(tracklab::io::audioDeviceManager(*engine), backend, hardware);
        if (restore)
            restoreError = tracklab::io::restoreAudioDeviceSetup(*engine);
        const auto registered = tracklab::io::registerIoCommands(registry, *engine);
        INFO("registerIoCommands: " << registered.error.code << " " << registered.error.message);
        REQUIRE(registered.ok);
    }

    CommandResult run(const std::string& id, const Json& params = Json::object()) const
    {
        return registry.execute(id, params);
    }

    /** Runs a command that has to succeed and returns its result. */
    Json ok(const std::string& id, const Json& params = Json::object()) const
    {
        const auto outcome = run(id, params);
        INFO(id << " " << params.dump() << " -> " << outcome.error.code << ": " << outcome.error.message << " @ "
                << outcome.error.pointer);
        REQUIRE(outcome.ok);
        return outcome.result;
    }

    Json setDevice(const Json& params) const { return ok("io.set_device", params); }
    Json getDevice() const { return ok("io.get_device"); }

    juce::AudioDeviceManager& deviceManager() const { return tracklab::io::audioDeviceManager(*engine); }

    std::shared_ptr<fake::Backend> backend;
    std::unique_ptr<tracklab::engine::te::Engine> engine;  // destroyed after the registry (declared before it)
    tracklab::core::CommandRegistry registry;
    juce::String restoreError;
};

/** The Setup of io.get_device for "no device open". */
inline Json noDeviceSetup()
{
    return Json{{"open", false},
                {"type", ""},
                {"input_device", ""},
                {"output_device", ""},
                {"sample_rate", 0},
                {"buffer_size", 0},
                {"active_input_channels", Json::array()},
                {"active_output_channels", Json::array()}};
}

/** Lower-case copy, for message checks that must not depend on capitalisation. */
inline std::string lower(std::string text)
{
    for (auto& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

inline bool contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

/** The standard first setup of many tests: mic interface in, speakers out, 48 kHz, 128 frames, inputs 0 and 2. */
inline Json micAndSpeakersParams()
{
    return Json{{"type", fake::kSeparateType},
                {"input_device", fake::kMicInterface},
                {"output_device", fake::kSpeakers},
                {"sample_rate", 48000},
                {"buffer_size", 128},
                {"active_input_channels", Json::array({0, 2})},
                {"active_output_channels", Json::array({0, 1})}};
}

/** The Setup that micAndSpeakersParams() has to produce. */
inline Json micAndSpeakersSetup()
{
    Json setup = micAndSpeakersParams();
    setup["open"] = true;
    return setup;
}

}  // namespace tracklab_test::io_helpers
