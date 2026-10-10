// Command line front end of Tracklab (M1-07, O-14): `tracklab-cli render | analyze | run-commands | export-tools | io`.
// All logic lives in the library tracklab_cli (runCli); src/cli/main.cpp is a thin main() around it, so that tests can
// call runCli in-process (fast, with hooks) and a few tests start the real executable (exit codes, stdout purity).
//
// Contract (the tests in tests/cli/ pin it down):
//
//   tracklab-cli [--engine-temp-dir <dir>] <command> ...
//
//   --engine-temp-dir <dir>   global option, before the command: base folder of the engine's private caches
//                             (EngineOptions::tempDirectory). Default: the engine factory's default. Tests set it so that
//                             nothing is written into the real user folders.
//
//   render <project> --out <file> [--format wav24]
//     Offline render of the project (whole project from 0 to the end of its last clip, no tail) to a 48 kHz / 24 bit
//     stereo WAV (the only format of M1; `wav24` is the default). The project file is never changed. Success:
//     {"ok":true,"out":"<file>","format":"wav24","sample_rate":48000,"channels":2,"bits_per_sample":24,
//     "length_samples":N}. A project without any clip: exit 1, nothing is written.
//
//   analyze <file|project> [--loudness] [--truepeak] [--lra] [--json]
//     Measures with Tracktion's LoudnessMeter (as the engine spike did). A `.tracklab` argument is a project: it is
//     rendered to a temporary file first (as `render`), anything else is read as an audio file. Only the requested
//     measurements are in the result: {"ok":true,"source":"<arg>","integrated_lufs":x,"true_peak_dbtp":x,"lra":x}.
//     The output is always JSON; --json is accepted. Without any of the three flags everything is measured. A
//     measurement the meter reports as its silence floor (-100, digital silence) is null: "no measurement".
//
//   run-commands <project> <commands.json> [--save-as <out>]
//     Opens the project, runs the steps of commands.json as ONE undo transaction (CommandRegistry::executeBatch), then
//     saves. commands.json is a JSON array of {"id": string, "params"?: object} (params default to {}). `--save-as
//     <out>` is the path of the NEW project folder: the project is written as `<out>/<basename of out>.tracklab`
//     (project.save_as with folder = parent of out, name = basename of out); the opened project file is not touched.
//     Success: {"ok":true,"commands":N,"results":[<result of every step>],"saved":"<new .tracklab path>"}.
//     Only commands that are undoable or readOnly are allowed (others, e.g. project.open/save/close, cannot be rolled
//     back): any other step is refused before the first step runs, exit 1, code "command_not_allowed", failed_index.
//     A failing step rolls the whole batch back (nothing is saved, no output is created): exit 1 and
//     {"ok":false,"error":{"code","message","pointer"},"failed_index":i}.
//
//   export-tools [--check] [--out <tools.json>] [--docs <commands.md>]
//     Writes the registry export (core/command_export.h) of the registry the app builds (app.*, edit.*, io.*,
//     project.* commands): --out = tools.json, --docs = docs/commands.md; each is optional, at least one is required
//     (parent folders are created). With --check nothing is written: the given files are compared byte for byte with
//     the export; exit 1 if one is stale or missing, the message names it. Success: {"ok":true,"commands":N,
//     "tools":"<path>"?,"docs":"<path>"?} (only the files that were given), with --check {"ok":true,"up_to_date":true}.
//
//   io <command-id> [<params-json>] [--settings-dir <dir>]                                                  (O-14)
//     Runs exactly ONE io.* command of the registry (io.list_device_types, io.list_devices, io.get_device,
//     io.set_device) without a project, on the real audio devices of the machine: the engine is created with
//     DeviceMode::automatic. This is the hand test path of M1 (docs/testing/manual/M1.md); run-commands cannot be used
//     for it (needs a project, refuses io.set_device because it is not undoable). Same registry, same code path as
//     every other caller: no second implementation of the commands.
//     <command-id> must start with "io.": anything else (app.version, project.new, ...) is a usage error, exit 2, code
//     "usage", and is NOT executed. An id with the prefix that the registry does not know: exit 1, "unknown_command".
//     <params-json> is a JSON object as one argument (default {}). Text that is not valid JSON or not an object fails
//     (nothing is executed); parameters that violate the command's schema: exit 1, "invalid_params" with the
//     "pointer" of the offending field (e.g. "/sample_rate"). A command that fails (e.g. a device that does not exist):
//     exit 1, the code and message of the registry (e.g. "handler_failed").
//     Success: {"ok":true,"result":<result of the command>}.
//     --settings-dir <dir>: folder of settings.xml (EngineOptions::settingsDirectory with SettingsStorage::file;
//     created if missing), so that a hand test does not overwrite the real settings. Without it the normal user folder
//     (defaultSettingsDirectory()). The settings are written before runCli returns (the engine is destroyed first), and
//     the next `io` call on the same folder starts with the stored device setup open (restored).
//     Wrong number of arguments, missing option value, unknown option: exit 2.
//
// Output: exactly ONE JSON object, terminated by '\n', on `out` (also on failure); diagnostics only on `err`.
// Failure: {"ok":false,"error":{"code":<string>,"message":<string>[,"pointer":<string>]}}. Error codes of the
// registry and of the project session are passed on 1:1 (unknown_command, invalid_params, project_not_found,
// corrupt_project, project_exists, ...); usage errors have code "usage".
//
// Exit codes: 0 success, 1 the operation failed (file or project missing/unreadable, a command failed, render failed),
// 2 usage error (no/unknown command, missing or unknown option, missing option value, invalid option value).
#pragma once

#include "core/command_registry.h"
#include "core/edit_context.h"

#include <tracktion_engine/tracktion_engine.h>

#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

namespace tracklab::cli
{

inline constexpr int exitOk = 0;
inline constexpr int exitFailed = 1;
inline constexpr int exitUsage = 2;

/** Test seams (like ProjectSession::setBeforeReplaceHook): never set by main(). */
struct CliHooks
{
    /** run-commands: called after the built-in commands are registered, before the project is opened. Lets a test add
        commands (e.g. an undoable one) to the registry the CLI builds. The context is the one the CLI's commands use. */
    std::function<void(core::CommandRegistry&, core::EditContext&)> registerExtraCommands;
    /** run-commands: called with the open Edit after the batch ran (also after a failed, rolled back one), before
        anything is saved. */
    std::function<void(tracktion::Edit&)> afterCommands;
    /** io: the test seam for audio hardware. If set, the engine of `io` is created with DeviceMode::none (no system
        device types, nothing of the real machine is scanned or opened), this function adds the (fake) device types to
        the engine's AudioDeviceManager, and then the stored setup is restored (io::restoreAudioDeviceSetup), exactly
        as a start of the app does with the system types. Never set by main(). */
    std::function<void(juce::AudioDeviceManager&)> addAudioDeviceTypes;
};

/** Runs the CLI. `args` excludes the program name. Has to be called on the JUCE message thread (main() sets that up). */
int runCli(const std::vector<std::string>& args, std::ostream& out, std::ostream& err, const CliHooks& hooks = {});

}  // namespace tracklab::cli
