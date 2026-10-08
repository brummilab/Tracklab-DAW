// STUB (test-writer, M1-06): the schemas and flags below are the interface the tests expect; the behaviour is the
// implementer's job. Every handler throws until it is implemented.
#include "io/audio_devices.h"

#include <stdexcept>
#include <utility>

namespace tracklab::io
{

namespace
{

using core::Command;
using core::Json;

const char* const kDeviceSchema = R"({
    "type": "object",
    "properties": {
        "name": {"type": "string"},
        "channel_names": {"type": "array", "items": {"type": "string"}},
        "sample_rates": {"type": "array", "items": {"type": "number"}},
        "buffer_sizes": {"type": "array", "items": {"type": "integer"}},
        "default_buffer_size": {"type": "integer"}
    },
    "required": ["name", "channel_names", "sample_rates", "buffer_sizes", "default_buffer_size"],
    "additionalProperties": false
})";

Json setupProperties()
{
    return Json::parse(R"({
        "open": {"type": "boolean"},
        "type": {"type": "string"},
        "input_device": {"type": "string"},
        "output_device": {"type": "string"},
        "sample_rate": {"type": "number"},
        "buffer_size": {"type": "integer"},
        "active_input_channels": {"type": "array", "items": {"type": "integer"}},
        "active_output_channels": {"type": "array", "items": {"type": "integer"}}
    })");
}

Json emptyParams()
{
    return Json::parse(R"({"type": "object", "properties": {}, "additionalProperties": false})");
}

Json setupSchema()
{
    return Json{{"type", "object"},
                {"properties", setupProperties()},
                {"required", Json::array({"open", "type", "input_device", "output_device", "sample_rate", "buffer_size",
                                          "active_input_channels", "active_output_channels"})},
                {"additionalProperties", false}};
}

std::function<Json(const Json&)> notImplemented()
{
    return [](const Json&) -> Json { throw std::runtime_error("not implemented"); };
}

Command listDeviceTypes()
{
    Command c;
    c.id = "io.list_device_types";
    c.titleDe = "Audio-Treibertypen auflisten";
    c.descriptionEn = "Lists the audio driver types (e.g. ALSA, JACK, WASAPI, ASIO) with the number of devices and the "
                      "type of the open device. 'hints' tells the user how to make an empty type work.";
    c.paramsSchema = emptyParams();
    c.resultSchema = Json::parse(R"({
        "type": "object",
        "properties": {
            "current_type": {"type": "string"},
            "types": {"type": "array", "items": {
                "type": "object",
                "properties": {
                    "name": {"type": "string"},
                    "separate_inputs_and_outputs": {"type": "boolean"},
                    "input_device_count": {"type": "integer"},
                    "output_device_count": {"type": "integer"}
                },
                "required": ["name", "separate_inputs_and_outputs", "input_device_count", "output_device_count"],
                "additionalProperties": false
            }},
            "hints": {"type": "array", "items": {"type": "string"}}
        },
        "required": ["current_type", "types", "hints"],
        "additionalProperties": false
    })");
    c.flags.readOnly = true;
    c.handler = notImplemented();
    return c;
}

Command listDevices()
{
    const Json device = Json::parse(kDeviceSchema);
    Command c;
    c.id = "io.list_devices";
    c.titleDe = "Audiogeräte auflisten";
    c.descriptionEn = "Lists the audio devices per driver type: input and output devices with channel names, sample "
                      "rates and buffer sizes. Optional 'type' limits the list to one driver type.";
    c.paramsSchema = Json::parse(R"({
        "type": "object",
        "properties": {"type": {"type": "string", "description": "Driver type name, e.g. ALSA"}},
        "additionalProperties": false
    })");
    c.resultSchema = Json{{"type", "object"},
                          {"properties",
                           {{"types",
                             {{"type", "array"},
                              {"items",
                               {{"type", "object"},
                                {"properties",
                                 {{"name", {{"type", "string"}}},
                                  {"separate_inputs_and_outputs", {{"type", "boolean"}}},
                                  {"inputs", {{"type", "array"}, {"items", device}}},
                                  {"outputs", {{"type", "array"}, {"items", device}}}}},
                                {"required", Json::array({"name", "separate_inputs_and_outputs", "inputs", "outputs"})},
                                {"additionalProperties", false}}}}}}},
                          {"required", Json::array({"types"})},
                          {"additionalProperties", false}};
    c.flags.readOnly = true;
    c.handler = notImplemented();
    return c;
}

Command getDevice()
{
    Command c;
    c.id = "io.get_device";
    c.titleDe = "Aktuelles Audiogerät anzeigen";
    c.descriptionEn = "Returns the open audio device: driver type, input and output device, sample rate, buffer size "
                      "and active channels. 'open' is false when no device is open.";
    c.paramsSchema = emptyParams();
    c.resultSchema = setupSchema();
    c.flags.readOnly = true;
    c.handler = notImplemented();
    return c;
}

Command setDevice()
{
    Command c;
    c.id = "io.set_device";
    c.titleDe = "Audiogerät einstellen";
    c.descriptionEn = "Selects and opens the audio device: driver type, input and output device, sample rate, buffer "
                      "size and active channels (0-based indices). Omitted values stay as they are; an empty device "
                      "name means none. Fails without changing anything if a device, rate, size or channel is not "
                      "available. Returns the new setup. The setting is stored.";
    c.paramsSchema =
        Json{{"type", "object"},
             {"properties",
              {{"type", {{"type", "string"}}},
               {"input_device", {{"type", "string"}}},
               {"output_device", {{"type", "string"}}},
               {"sample_rate", {{"type", "number"}, {"minimum", 8000}, {"maximum", 768000}}},
               {"buffer_size", {{"type", "integer"}, {"minimum", 16}, {"maximum", 16384}}},
               {"active_input_channels", {{"type", "array"}, {"items", {{"type", "integer"}, {"minimum", 0}}}}},
               {"active_output_channels", {{"type", "array"}, {"items", {{"type", "integer"}, {"minimum", 0}}}}}}},
             {"additionalProperties", false}};
    c.resultSchema = setupSchema();
    // Devices are global to the application: no undo, no confirmation.
    c.handler = notImplemented();
    return c;
}

}  // namespace

juce::AudioDeviceManager& audioDeviceManager(te::Engine& engine)
{
    return engine.getDeviceManager().deviceManager;
}

juce::String restoreAudioDeviceSetup(te::Engine&)
{
    return "not implemented";
}

core::RegisterResult registerIoCommands(core::CommandRegistry& registry, te::Engine&)
{
    for (auto make : {&listDeviceTypes, &listDevices, &getDevice, &setDevice})
        if (auto outcome = registry.registerCommand(make()); !outcome.ok)
            return outcome;
    core::RegisterResult done;
    done.ok = true;
    return done;
}

}  // namespace tracklab::io
