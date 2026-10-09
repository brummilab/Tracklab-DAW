// Command line front end of Tracklab (M1-07): `tracklab-cli render | analyze | run-commands | export-tools`.
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
//     The output is always JSON; --json is accepted.
//
//   run-commands <project> <commands.json> [--save-as <out>]
//     Opens the project, runs the steps of commands.json as ONE undo transaction (CommandRegistry::executeBatch), then
//     saves. commands.json is a JSON array of {"id": string, "params"?: object} (params default to {}). `--save-as
//     <out>` is the path of the NEW project folder: the project is written as `<out>/<basename of out>.tracklab`
//     (project.save_as with folder = parent of out, name = basename of out); the opened project file is not touched.
//     Success: {"ok":true,"commands":N,"results":[<result of every step>],"saved":"<new .tracklab path>"}.
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
};

/** Runs the CLI. `args` excludes the program name. Has to be called on the JUCE message thread (main() sets that up). */
int runCli(const std::vector<std::string>& args, std::ostream& out, std::ostream& err, const CliHooks& hooks = {});

}  // namespace tracklab::cli
