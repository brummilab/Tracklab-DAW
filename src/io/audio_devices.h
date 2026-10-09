// Audio devices of Tracklab (M1-06): the commands io.list_device_types, io.list_devices, io.get_device and
// io.set_device on top of the juce::AudioDeviceManager of the Tracktion Engine (te::DeviceManager::deviceManager).
//
// All four commands run on the message thread (CommandRegistry::execute). None of them is undoable: audio devices are
// global to the application, not part of a project. The device setup is stored in the settings of the engine
// (te::PropertyStorage, SettingID::audio_device_setup; with SettingsStorage::file in settings.xml, see M1-01).
//
// Errors of a handler are thrown as std::exception and therefore reach the caller as CommandError
// "handler_failed" (core/command.h). The message names the offending value:
//   unknown type                   -> contains the type name
//   unknown input/output device    -> contains the device name (also if it exists in another type only)
//   sample rate not supported      -> contains "sample rate" and the requested rate
//   buffer size not supported      -> contains "buffer size" and the requested size
//   channel index out of range     -> contains "channel" and the index
// JUCE would silently pick the nearest rate/size; Tracklab does not (the user must see what they get).
// A failed io.set_device changes nothing: not the open device, not the stored setup.
#pragma once

#include "core/command_registry.h"

#include <tracktion_engine/tracktion_engine.h>

namespace tracklab::io
{

namespace te = tracktion;

/** The device manager of the engine (`engine.getDeviceManager().deviceManager`). Tests add fake device types to it
    with addAudioDeviceType(). */
juce::AudioDeviceManager& audioDeviceManager(te::Engine& engine);

/** Opens the device stored in the settings of the engine (SettingID::audio_device_setup) on the device manager.
    DeviceMode::automatic does this when the engine is created (Tracktion's loadSettings); this function is for an
    engine created with DeviceMode::none (CLI, tests) after the device types were added. It touches nothing but the
    juce::AudioDeviceManager: no MIDI scan, no Tracktion device lists.
    If nothing is stored it does nothing. If the stored device does not exist (any more) the device manager falls back
    to the default device of a type it has (JUCE behaviour, selectDefaultDeviceOnFailure). Returns the error text of
    the device manager, empty on success. Message thread only. */
juce::String restoreAudioDeviceSetup(te::Engine& engine);

/** Registers the four io.* commands. `engine` has to outlive every execute() of `registry`.
    The exports (tools.json, docs/commands.md) list them like all other commands, so a headless engine
    (DeviceMode::none) is enough to build the full registry.

    io.list_device_types  readOnly, params {}
        result {"current_type": string, "types": [{"name", "separate_inputs_and_outputs": boolean,
                "input_device_count": integer, "output_device_count": integer}], "hints": [string]}
        "types" in the order of the device manager. "current_type" is the type of the open device ("" if none).
        "hints" are texts for the user. Linux: if the type named "JACK" exists and has no devices (neither inputs nor
        outputs), one hint says that Tracklab has to be started with `pw-jack` (PipeWire's JACK library). No hint
        when JACK has devices, never on other systems.

    io.list_devices       readOnly, params {"type"?: string}
        result {"types": [{"name", "separate_inputs_and_outputs": boolean, "inputs": [Device], "outputs": [Device]}]}
        Device = {"name": string, "channel_names": [string], "sample_rates": [number], "buffer_sizes": [integer],
                  "default_buffer_size": integer}
        "type" absent: all types; unknown type: handler_failed. For types without separate inputs and outputs the same
        device appears in "inputs" (with its input channel names) and in "outputs" (with its output channel names).
        The device that is open is described by the open device itself (a second device object for hardware that is in
        use cannot be asked); other devices are asked briefly. A device used exclusively by another program is listed
        with empty lists.

    io.get_device         readOnly, params {}
        result Setup = {"open": boolean, "type": string, "input_device": string, "output_device": string,
                        "sample_rate": number, "buffer_size": integer, "active_input_channels": [integer],
                        "active_output_channels": [integer]}
        Channel indices are 0-based positions in the channel names of the device. No device open: "open" false, empty
        names and lists, rate and size 0.

    io.set_device         not readOnly, not undoable, not destructive,
        params {"type"?: string, "input_device"?: string, "output_device"?: string, "sample_rate"?: number,
                "buffer_size"?: integer, "active_input_channels"?: [integer], "active_output_channels"?: [integer]}
        Absent = keep the current value; "" for a device = none. A new "type" without devices selects that type's
        default devices. The side that stays (same device name) takes its channels, rates and buffer sizes from the open
        device. A chosen device that reports no channels is an error ("reports no channels ..."), never ok. The
        result is the Setup after the change (same shape as io.get_device). The device is
        opened, and the setup is stored in the settings of the engine before the command returns (no message loop is
        needed in between). */
[[nodiscard]] core::RegisterResult registerIoCommands(core::CommandRegistry& registry, te::Engine& engine);

}  // namespace tracklab::io
