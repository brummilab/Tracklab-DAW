// M1-06: the four io.* commands in the registry: ids, flags, and the schemas ("invalid values -> schema error").
// Almost all cases are refused by the schema before a handler runs, so no device is touched there.
#include "io_test_session.h"

#include "core/command_registry.h"

#include <optional>
#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::io_helpers;
using tracklab::core::CommandRegistry;
using tracklab_test::ScopedTempDir;
namespace error_code = tracklab::core::error_code;

struct BadParams
{
    const char* label;
    const char* command;
    const char* params;
    const char* pointer;
};

constexpr BadParams kBadParams[] = {
    {"get_device takes no params", "io.get_device", R"({"x": 1})", "/x"},
    {"list_device_types takes no params", "io.list_device_types", R"({"type": "ALSA"})", "/type"},
    {"list_devices: unknown property", "io.list_devices", R"({"typ": "ALSA"})", "/typ"},
    {"list_devices: type is not a string", "io.list_devices", R"({"type": 3})", "/type"},
    {"set_device: unknown property", "io.set_device", R"({"samplerate": 48000})", "/samplerate"},
    {"set_device: type is not a string", "io.set_device", R"({"type": 5})", "/type"},
    {"set_device: input device is not a string", "io.set_device", R"({"input_device": ["a"]})", "/input_device"},
    {"set_device: output device is not a string", "io.set_device", R"({"output_device": 1})", "/output_device"},
    {"set_device: sample rate is a string", "io.set_device", R"({"sample_rate": "48000"})", "/sample_rate"},
    {"set_device: sample rate is zero", "io.set_device", R"({"sample_rate": 0})", "/sample_rate"},
    {"set_device: sample rate is negative", "io.set_device", R"({"sample_rate": -44100})", "/sample_rate"},
    {"set_device: buffer size is a float", "io.set_device", R"({"buffer_size": 128.5})", "/buffer_size"},
    {"set_device: buffer size is zero", "io.set_device", R"({"buffer_size": 0})", "/buffer_size"},
    {"set_device: buffer size is negative", "io.set_device", R"({"buffer_size": -256})", "/buffer_size"},
    {"set_device: input channels are not an array", "io.set_device", R"({"active_input_channels": 0})",
     "/active_input_channels"},
    {"set_device: negative input channel", "io.set_device", R"({"active_input_channels": [0, -1]})",
     "/active_input_channels/1"},
    {"set_device: output channel is a string", "io.set_device", R"({"active_output_channels": ["Left"]})",
     "/active_output_channels/0"},
    {"set_device: output channel is a float", "io.set_device", R"({"active_output_channels": [0.5]})",
     "/active_output_channels/0"},
};

}  // namespace

TEST_SUITE("io")
{
    TEST_CASE("registerIoCommands registers io.list_device_types, io.list_devices, io.get_device, io.set_device")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());

        CHECK(session.registry.size() == 4);
        for (const char* id : {"io.list_device_types", "io.list_devices", "io.get_device", "io.set_device"})
        {
            CAPTURE(id);
            CHECK(session.registry.contains(id));
        }
    }

    TEST_CASE("the three listing commands are read-only")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());

        for (const char* id : {"io.list_device_types", "io.list_devices", "io.get_device"})
        {
            CAPTURE(id);
            const auto* command = session.registry.find(id);
            REQUIRE(command != nullptr);
            CHECK(command->flags.readOnly);
            CHECK_FALSE(command->flags.undoable);
            CHECK_FALSE(command->flags.destructive);
        }
    }

    TEST_CASE("io.set_device is neither undoable nor destructive nor read-only (devices are global to the app)")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());

        const auto* command = session.registry.find("io.set_device");
        REQUIRE(command != nullptr);
        CHECK_FALSE(command->flags.readOnly);
        CHECK_FALSE(command->flags.undoable);
        CHECK_FALSE(command->flags.destructive);
    }

    TEST_CASE("registering the io commands twice is refused with duplicate_id and changes nothing")
    {
        const ScopedTempDir temp;
        Session session(temp.dir());

        const auto again = tracklab::io::registerIoCommands(session.registry, *session.engine);
        CHECK_FALSE(again.ok);
        CHECK(again.error.code == error_code::duplicateId);
        CHECK(session.registry.size() == 4);
    }

    TEST_CASE("the io commands go next to other commands in one registry")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        CommandRegistry other;
        REQUIRE(tracklab::io::registerIoCommands(other, *session.engine).ok);
        CHECK(other.size() == 4);
        CHECK(other.idForToolName("io_set_device") == std::optional<std::string>("io.set_device"));
    }

    TEST_CASE("invalid parameters are refused with invalid_params and a pointer to the field; no device is touched")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());

        for (const auto& c : kBadParams)
        {
            CAPTURE(c.label);
            const auto outcome = session.run(c.command, Json::parse(c.params));
            CHECK_FALSE(outcome.ok);
            CHECK(outcome.error.code == error_code::invalidParams);
            CHECK(outcome.error.pointer == c.pointer);
            CHECK_FALSE(outcome.error.message.empty());
        }
        CHECK(session.backend->devicesCreated == 0);
        CHECK(session.backend->opens.empty());
    }

    TEST_CASE("io.set_device accepts an empty object and every parameter on its own (schema only)")
    {
        // Only the schema is checked here: whether the value is a real device is tested with the fake hardware.
        const ScopedTempDir temp;
        const Session session(temp.dir());

        for (const char* text : {R"({})", R"({"type": "ALSA"})", R"({"input_device": ""})", R"({"output_device": "x"})",
                                 R"({"sample_rate": 44100})", R"({"sample_rate": 44100.0})", R"({"buffer_size": 64})",
                                 R"({"active_input_channels": []})", R"({"active_output_channels": [0, 1]})"})
        {
            CAPTURE(text);
            const auto outcome = session.run("io.set_device", Json::parse(text));
            // Either it worked or the handler refused it; the schema must not.
            CHECK(outcome.error.code != error_code::invalidParams);
        }
    }
}
