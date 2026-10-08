// Audio devices of Tracklab (M1-06), see audio_devices.h for the contract of the four commands.
//
// Everything here runs on the message thread (CommandRegistry::execute) and may allocate, lock and open devices; none
// of it is reachable from an audio callback. The only link to the audio thread is the juce::AudioDeviceManager
// itself, which stops and restarts the device around the calls below.
#include "io/audio_devices.h"

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace tracklab::io
{

namespace
{

using core::Command;
using core::Json;

//==============================================================================
// Helpers

[[noreturn]] void fail(const juce::String& message)
{
    throw std::runtime_error(message.toStdString());
}

/** A rate such as 44100 as an integer in JSON (and in messages), 44100.5 as a fraction. */
Json numberJson(double value)
{
    if (juce::exactlyEqual(std::floor(value), value) && std::abs(value) < 1.0e15)
        return Json(static_cast<long long>(value));
    return Json(value);
}

juce::String numberText(double value)
{
    return numberJson(value).dump();
}

Json stringsJson(const juce::StringArray& strings)
{
    Json array = Json::array();
    for (const auto& s : strings)
        array.push_back(s.toStdString());
    return array;
}

/** The set bits of `mask` as a sorted array of 0-based channel indices. */
Json channelsJson(const juce::BigInteger& mask)
{
    Json array = Json::array();
    for (int i = mask.findNextSetBit(0); i >= 0; i = mask.findNextSetBit(i + 1))
        array.push_back(i);
    return array;
}

juce::String quoted(const juce::String& name)
{
    return "'" + name + "'";
}

bool sameRate(double a, double b)
{
    return std::abs(a - b) < 0.001;
}

bool offersRate(const juce::Array<double>& rates, double rate)
{
    for (const double r : rates)
        if (sameRate(r, rate))
            return true;
    return false;
}

juce::String rateListText(const juce::Array<double>& rates)
{
    juce::StringArray parts;
    for (const double r : rates)
        parts.add(numberText(r));
    return parts.joinIntoString(", ");
}

juce::String sizeListText(const juce::Array<int>& sizes)
{
    juce::StringArray parts;
    for (const int s : sizes)
        parts.add(juce::String(s));
    return parts.joinIntoString(", ");
}

juce::AudioIODeviceType* findType(juce::AudioDeviceManager& manager, const juce::String& name)
{
    for (auto* type : manager.getAvailableDeviceTypes())
        if (type->getTypeName() == name)
            return type;
    return nullptr;
}

/** A device to ask for channel names, rates and buffer sizes. The open device itself if it is exactly the requested
    one (hardware that is open must not be opened a second time just to look at it), a short-lived new one otherwise;
    the same way juce::AudioDeviceSelectorComponent finds out what a device offers. Opens nothing. */
struct Probe
{
    juce::AudioIODevice* device = nullptr;  ///< null if the type cannot create it
    std::unique_ptr<juce::AudioIODevice> owned;
};

Probe probeDevice(juce::AudioDeviceManager& manager, juce::AudioIODeviceType& type, const juce::String& output,
                  const juce::String& input)
{
    Probe probe;
    if (auto* current = manager.getCurrentAudioDevice();
        current != nullptr && current->getTypeName() == type.getTypeName())
    {
        const auto setup = manager.getAudioDeviceSetup();
        if (setup.outputDeviceName == output && setup.inputDeviceName == input)
        {
            probe.device = current;
            return probe;
        }
    }
    probe.owned.reset(type.createDevice(output, input));
    probe.device = probe.owned.get();
    return probe;
}

//==============================================================================
// io.list_device_types

Json listDeviceTypesHandler(te::Engine& engine)
{
    auto& manager = audioDeviceManager(engine);
    Json types = Json::array();
    Json hints = Json::array();

    for (auto* type : manager.getAvailableDeviceTypes())
    {
        const auto inputs = type->getDeviceNames(true).size();
        const auto outputs = type->getDeviceNames(false).size();
        types.push_back({{"name", type->getTypeName().toStdString()},
                         {"separate_inputs_and_outputs", type->hasSeparateInputsAndOutputs()},
                         {"input_device_count", inputs},
                         {"output_device_count", outputs}});

#if JUCE_LINUX
        // PipeWire offers JACK only to programs started through its JACK library; without it the type is there but
        // empty, which looks like a broken setup to the user.
        if (type->getTypeName() == "JACK" && inputs == 0 && outputs == 0)
            hints.push_back("No JACK devices found. Start Tracklab with `pw-jack` (PipeWire's JACK library), e.g. "
                            "`pw-jack tracklab`, or start a JACK server first.");
#endif
    }

    const auto* current = manager.getCurrentAudioDevice();
    return Json{{"current_type", current != nullptr ? current->getTypeName().toStdString() : std::string()},
                {"types", std::move(types)},
                {"hints", std::move(hints)}};
}

//==============================================================================
// io.list_devices

Json deviceJson(const juce::String& name, juce::AudioIODevice* device, bool input)
{
    Json channelNames = Json::array();
    Json rates = Json::array();
    Json sizes = Json::array();
    int defaultSize = 0;
    if (device != nullptr)  // the device vanished between listing its name and asking for it: listed, but empty
    {
        channelNames = stringsJson(input ? device->getInputChannelNames() : device->getOutputChannelNames());
        for (const double r : device->getAvailableSampleRates())
            rates.push_back(numberJson(r));
        for (const int s : device->getAvailableBufferSizes())
            sizes.push_back(s);
        defaultSize = device->getDefaultBufferSize();
    }
    return {{"name", name.toStdString()},
            {"channel_names", std::move(channelNames)},
            {"sample_rates", std::move(rates)},
            {"buffer_sizes", std::move(sizes)},
            {"default_buffer_size", defaultSize}};
}

Json typeDevicesJson(juce::AudioDeviceManager& manager, juce::AudioIODeviceType& type)
{
    const bool separate = type.hasSeparateInputsAndOutputs();
    Json lists = Json::object();
    for (const bool input : {true, false})
    {
        Json devices = Json::array();
        for (const auto& name : type.getDeviceNames(input))
        {
            // A type without separate lists has one device for both directions: ask for it with both names.
            const auto probe =
                separate ? probeDevice(manager, type, input ? juce::String() : name, input ? name : juce::String())
                         : probeDevice(manager, type, name, name);
            devices.push_back(deviceJson(name, probe.device, input));
        }
        lists[input ? "inputs" : "outputs"] = std::move(devices);
    }
    return {{"name", type.getTypeName().toStdString()},
            {"separate_inputs_and_outputs", separate},
            {"inputs", std::move(lists["inputs"])},
            {"outputs", std::move(lists["outputs"])}};
}

Json listDevicesHandler(te::Engine& engine, const Json& params)
{
    auto& manager = audioDeviceManager(engine);
    Json types = Json::array();

    if (params.contains("type"))
    {
        const auto name = juce::String(params.at("type").get<std::string>());
        auto* type = findType(manager, name);
        if (type == nullptr)
            fail("unknown audio driver type " + quoted(name));
        types.push_back(typeDevicesJson(manager, *type));
    }
    else
    {
        for (auto* type : manager.getAvailableDeviceTypes())
            types.push_back(typeDevicesJson(manager, *type));
    }
    return Json{{"types", std::move(types)}};
}

//==============================================================================
// io.get_device

Json currentSetupJson(juce::AudioDeviceManager& manager)
{
    auto* device = manager.getCurrentAudioDevice();
    if (device == nullptr)
        return {{"open", false},
                {"type", ""},
                {"input_device", ""},
                {"output_device", ""},
                {"sample_rate", 0},
                {"buffer_size", 0},
                {"active_input_channels", Json::array()},
                {"active_output_channels", Json::array()}};

    const auto setup = manager.getAudioDeviceSetup();
    return {{"open", true},
            {"type", device->getTypeName().toStdString()},
            {"input_device", setup.inputDeviceName.toStdString()},
            {"output_device", setup.outputDeviceName.toStdString()},
            {"sample_rate", numberJson(device->getCurrentSampleRate())},
            {"buffer_size", device->getCurrentBufferSizeSamples()},
            {"active_input_channels", channelsJson(device->getActiveInputChannels())},
            {"active_output_channels", channelsJson(device->getActiveOutputChannels())}};
}

//==============================================================================
// io.set_device

juce::String defaultDeviceName(juce::AudioIODeviceType& type, bool input)
{
    const auto names = type.getDeviceNames(input);
    const int index = type.getDefaultDeviceIndex(input);
    return index >= 0 && index < names.size() ? names[index] : juce::String();
}

juce::String stringParam(const Json& params, const char* key)
{
    return juce::String(params.at(key).get<std::string>());
}

/** The channels the new setup activates on one side (input or output) of the device, as a mask.
    Given indices: checked against the channels of the device. Not given: an unchanged device keeps its channels, a
    new device (or a side that had no device) gets all its channels (Lead decision 5, M1-06). */
juce::BigInteger resolveChannels(const Json& params, const char* key, bool input, const juce::String& deviceName,
                                 const juce::StringArray& channelNames, const juce::BigInteger* kept)
{
    juce::BigInteger mask;
    const bool given = params.contains(key);
    const juce::String side = input ? "input" : "output";

    if (deviceName.isEmpty())
    {
        if (given && !params.at(key).empty())
            fail("channel " + juce::String(params.at(key).front().get<int>()) + " requested, but there is no " + side +
                 " device");
        return mask;
    }

    if (given)
    {
        for (const auto& entry : params.at(key))
        {
            const int index = entry.get<int>();
            if (index >= channelNames.size())
                fail("channel index " + juce::String(index) + " is out of range for the " + side + " device " +
                     quoted(deviceName) + " (" + juce::String(channelNames.size()) + " channels)");
            mask.setBit(index);
        }
        return mask;
    }

    if (kept != nullptr)
    {
        juce::BigInteger inRange;
        inRange.setRange(0, channelNames.size(), true);
        mask = *kept & inRange;
    }
    if (mask.isZero())
        mask.setRange(0, channelNames.size(), true);
    return mask;
}

Json setDeviceHandler(te::Engine& engine, const Json& params)
{
    auto& manager = audioDeviceManager(engine);
    const auto& types = manager.getAvailableDeviceTypes();
    if (types.isEmpty())
        fail("no audio driver types available");

    auto* current = manager.getCurrentAudioDevice();
    const juce::String openType = current != nullptr ? current->getTypeName() : juce::String();
    const bool hadDevice = current != nullptr;  // `current` dangles once the manager replaces the device
    const auto before = manager.getAudioDeviceSetup();

    // ---- 1. Work out the complete new setup. Nothing is changed before all of it has been checked.
    juce::AudioIODeviceType* type = nullptr;
    if (params.contains("type"))
    {
        const auto name = stringParam(params, "type");
        type = findType(manager, name);
        if (type == nullptr)
            fail("unknown audio driver type " + quoted(name));
    }
    else
    {
        type = findType(manager, current != nullptr ? openType : manager.getCurrentAudioDeviceType());
        if (type == nullptr)
            type = types.getFirst();
    }
    const auto typeName = type->getTypeName();
    const bool separate = type->hasSeparateInputsAndOutputs();
    // The devices of the open setup are valid for this type only if it is the type of the open device.
    const bool sameType = current != nullptr && typeName == openType;

    const bool hasInput = params.contains("input_device");
    const bool hasOutput = params.contains("output_device");
    auto resolveName = [&](bool input)
    {
        if (params.contains(input ? "input_device" : "output_device"))
            return stringParam(params, input ? "input_device" : "output_device");
        if (sameType)
            return input ? before.inputDeviceName : before.outputDeviceName;
        return defaultDeviceName(*type, input);
    };
    juce::String inputName = resolveName(true);
    juce::String outputName = resolveName(false);

    // One device for both directions: naming one side selects the device for the other side too.
    if (!separate && hasInput != hasOutput)
    {
        auto& other = hasInput ? outputName : inputName;
        const auto& given = hasInput ? inputName : outputName;
        const auto& otherBefore = hasInput ? before.outputDeviceName : before.inputDeviceName;
        if (given.isNotEmpty() && (!sameType || otherBefore.isNotEmpty()))
            other = given;
    }

    for (const bool input : {true, false})
    {
        const auto& name = input ? inputName : outputName;
        if (name.isNotEmpty() && !type->getDeviceNames(input).contains(name))
            fail(juce::String(input ? "unknown input" : "unknown output") + " device " + quoted(name) +
                 " in audio driver type " + quoted(typeName));
    }
    if (!separate && inputName.isNotEmpty() && outputName.isNotEmpty() && inputName != outputName)
        fail("audio driver type " + quoted(typeName) + " has one device for input and output, but " +
             quoted(inputName) + " and " + quoted(outputName) + " were chosen");
    if (inputName.isEmpty() && outputName.isEmpty())
        fail("no audio device chosen: audio driver type " + quoted(typeName) + " has no device to select");

    auto probe = probeDevice(manager, *type, outputName, inputName);
    if (probe.device == nullptr)
        fail("cannot create the audio device " + quoted(outputName.isNotEmpty() ? outputName : inputName));
    auto& device = *probe.device;
    const auto deviceText = quoted(outputName.isNotEmpty() ? outputName : inputName);

    // Rate and size: what is asked for has to be offered exactly (JUCE would round to the nearest). What is not asked
    // for stays, as long as the new device offers it; if it does not, 0 lets the device manager choose its default.
    const auto rates = device.getAvailableSampleRates();
    const auto sizes = device.getAvailableBufferSizes();
    double sampleRate = 0.0;
    if (params.contains("sample_rate"))
    {
        sampleRate = params.at("sample_rate").get<double>();
        if (!offersRate(rates, sampleRate))
            fail("sample rate " + numberText(sampleRate) + " is not offered by " + deviceText +
                 " (available: " + rateListText(rates) + ")");
    }
    else if (current != nullptr && offersRate(rates, before.sampleRate))
    {
        sampleRate = before.sampleRate;
    }

    int bufferSize = 0;
    if (params.contains("buffer_size"))
    {
        bufferSize = params.at("buffer_size").get<int>();
        if (!sizes.contains(bufferSize))
            fail("buffer size " + juce::String(bufferSize) + " is not offered by " + deviceText +
                 " (available: " + sizeListText(sizes) + ")");
    }
    else if (current != nullptr && sizes.contains(before.bufferSize))
    {
        bufferSize = before.bufferSize;
    }

    const bool keepInput = sameType && before.inputDeviceName == inputName;
    const bool keepOutput = sameType && before.outputDeviceName == outputName;
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.inputDeviceName = inputName;
    setup.outputDeviceName = outputName;
    setup.sampleRate = sampleRate;
    setup.bufferSize = bufferSize;
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = false;
    setup.inputChannels = resolveChannels(params, "active_input_channels", true, inputName,
                                          device.getInputChannelNames(), keepInput ? &before.inputChannels : nullptr);
    setup.outputChannels =
        resolveChannels(params, "active_output_channels", false, outputName, device.getOutputChannelNames(),
                        keepOutput ? &before.outputChannels : nullptr);
    if (setup.inputChannels.isZero() && setup.outputChannels.isZero())
        fail("no active channel chosen for " + deviceText);

    // ---- 2. Apply.
    // A probe device that is not the open one has to be gone before the real device opens (exclusive hardware).
    probe.owned.reset();
    const auto managerType = manager.getCurrentAudioDeviceType();
    const bool managerUsesFirstType = managerType.isEmpty() || findType(manager, managerType) == nullptr;
    if (typeName != managerType && !(managerUsesFirstType && type == types.getFirst()))
        manager.setCurrentAudioDeviceType(typeName, true);

    const auto error = manager.setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty())
    {
        // Leave the application as it was: the same type and, if there was one, the same device.
        juce::String restored = "the previous device was not restored";
        if (hadDevice)
        {
            if (manager.getCurrentAudioDeviceType() != openType)
                manager.setCurrentAudioDeviceType(openType, true);
            if (manager.setAudioDeviceSetup(before, true).isEmpty())
                restored = "the previous device is open again";
        }
        fail("cannot open " + deviceText + ": " + error.trim() + " (" + restored + ")");
    }

    // Tracktion stores the setup when the device manager sends its change message, which needs the message loop.
    // The command promises that the setup is stored when it returns, so store it now (same key, same XML).
    if (const auto state = manager.createStateXml())
        engine.getPropertyStorage().setXmlProperty(te::SettingID::audio_device_setup, *state);

    return currentSetupJson(manager);
}

//==============================================================================
// Command definitions (schemas are part of the interface, see audio_devices.h)

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

Command listDeviceTypes(te::Engine& engine)
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
    c.handler = [&engine](const Json&) { return listDeviceTypesHandler(engine); };
    return c;
}

Command listDevices(te::Engine& engine)
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
    c.handler = [&engine](const Json& params) { return listDevicesHandler(engine, params); };
    return c;
}

Command getDevice(te::Engine& engine)
{
    Command c;
    c.id = "io.get_device";
    c.titleDe = "Aktuelles Audiogerät anzeigen";
    c.descriptionEn = "Returns the open audio device: driver type, input and output device, sample rate, buffer size "
                      "and active channels. 'open' is false when no device is open.";
    c.paramsSchema = emptyParams();
    c.resultSchema = setupSchema();
    c.flags.readOnly = true;
    c.handler = [&engine](const Json&) { return currentSetupJson(audioDeviceManager(engine)); };
    return c;
}

Command setDevice(te::Engine& engine)
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
    c.handler = [&engine](const Json& params) { return setDeviceHandler(engine, params); };
    return c;
}

}  // namespace

juce::AudioDeviceManager& audioDeviceManager(te::Engine& engine)
{
    return engine.getDeviceManager().deviceManager;
}

juce::String restoreAudioDeviceSetup(te::Engine& engine)
{
    const auto stored = engine.getPropertyStorage().getXmlProperty(te::SettingID::audio_device_setup);
    if (stored == nullptr)
        return {};

    // The call of Tracktion's DeviceManager::loadSettings(), without the MIDI and Tracktion device lists around it.
    // The channel counts only matter for a setup without stored channel masks: all channels, as in Tracktion.
    return audioDeviceManager(engine).initialise(te::DeviceManager::defaultNumChannelsToOpen,
                                                 te::DeviceManager::defaultNumChannelsToOpen, stored.get(), true);
}

core::RegisterResult registerIoCommands(core::CommandRegistry& registry, te::Engine& engine)
{
    for (auto make : {&listDeviceTypes, &listDevices, &getDevice, &setDevice})
        if (auto outcome = registry.registerCommand(make(engine)); !outcome.ok)
            return outcome;
    core::RegisterResult done;
    done.ok = true;
    return done;
}

}  // namespace tracklab::io
