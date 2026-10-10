// tracklab-cli (M1-07, O-14): argument parsing, the five subcommands and the one-JSON-line output. The contract is in
// cli.h.
// Everything that changes a project goes through the command registry (project.open, project.save(_as), executeBatch);
// there is no second code path next to the commands. Render and analyze are not commands yet (they read a project but
// change nothing), so they use the engine directly.
#include "cli/cli.h"

#include "cli/builtin_commands.h"
#include "cli/cli_audio.h"
#include "cli/cli_error.h"
#include "core/command_export.h"
#include "engine/engine_factory.h"
#include "io/audio_devices.h"
#include "project/project_session.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

namespace tracklab::cli
{

namespace
{

using core::Json;
namespace fs = std::filesystem;

/** The undo step of run-commands (one transaction for the whole file), fixed so that it reads the same everywhere. */
constexpr const char* runCommandsTransactionName = "CLI: run-commands";

//==============================================================================
// Arguments

struct ArgSpec
{
    std::set<std::string> flags;         ///< options without a value
    std::set<std::string> valueOptions;  ///< options with exactly one value
    std::size_t minPositionals;
    std::size_t maxPositionals;
    const char* usage;
};

struct ParsedArgs
{
    std::set<std::string> flags;
    std::map<std::string, std::string> values;
    std::vector<std::string> positionals;

    bool has(const std::string& flag) const { return flags.count(flag) != 0; }
    const std::string* value(const std::string& option) const
    {
        const auto it = values.find(option);
        return it == values.end() ? nullptr : &it->second;
    }
};

/** Spelled out in full at every call (the compiler warns about omitted members of a designated initializer). */
ArgSpec makeSpec(std::set<std::string> flags, std::set<std::string> valueOptions, std::size_t minPositionals,
                 std::size_t maxPositionals, const char* usage)
{
    return ArgSpec{std::move(flags), std::move(valueOptions), minPositionals, maxPositionals, usage};
}

ParsedArgs parseArgs(const std::vector<std::string>& args, std::size_t from, const ArgSpec& spec)
{
    ParsedArgs parsed;
    for (std::size_t i = from; i < args.size(); ++i)
    {
        const auto& arg = args[i];
        if (arg.rfind("--", 0) != 0)
        {
            parsed.positionals.push_back(arg);
        }
        else if (spec.flags.count(arg) != 0)
        {
            if (!parsed.flags.insert(arg).second)
                throw usageError("option " + arg + " given twice. " + spec.usage);
        }
        else if (spec.valueOptions.count(arg) != 0)
        {
            if (i + 1 >= args.size())
                throw usageError("option " + arg + " needs a value. " + spec.usage);
            if (!parsed.values.emplace(arg, args[++i]).second)
                throw usageError("option " + arg + " given twice. " + spec.usage);
        }
        else
        {
            throw usageError("unknown option " + arg + ". " + spec.usage);
        }
    }
    if (parsed.positionals.size() < spec.minPositionals || parsed.positionals.size() > spec.maxPositionals)
        throw usageError(std::string("wrong number of arguments. ") + spec.usage);
    return parsed;
}

/** Paths are relative to the working directory of the process. */
juce::File fileFromArg(const std::string& arg)
{
    return juce::File::getCurrentWorkingDirectory().getChildFile(juce::String::fromUTF8(arg.c_str()));
}

std::string pathOf(const juce::File& file)
{
    return file.getFullPathName().toStdString();
}

bool isProjectArg(const std::string& arg)
{
    return fileFromArg(arg).hasFileExtension(".tracklab");
}

//==============================================================================
// Workspace: the engine, the open project and the registry of the app, as the commands see them

/** What the engine of a call is built for. The default is the headless CLI: no audio device, in-memory settings,
    private cache. `io` is the only caller that wants devices and the settings on disk. */
struct WorkspaceOptions
{
    std::string engineTempDir;  ///< the only member the headless callers set
    bool realDevices = false;   ///< io: audio devices and settings.xml (in `settingsDir`, or the user folder if empty)
    std::string settingsDir = {};
    const CliHooks* hooks = nullptr;
};

class Workspace
{
public:
    explicit Workspace(const WorkspaceOptions& workspaceOptions)
    {
        engine::EngineOptions options;
        if (!workspaceOptions.engineTempDir.empty())
            options.tempDirectory = fileFromArg(workspaceOptions.engineTempDir);
        const bool fakeDevices = workspaceOptions.realDevices && workspaceOptions.hooks != nullptr &&
                                 static_cast<bool>(workspaceOptions.hooks->addAudioDeviceTypes);
        if (workspaceOptions.realDevices)
        {
            options.storage = engine::SettingsStorage::file;
            if (!workspaceOptions.settingsDir.empty())
                options.settingsDirectory = fileFromArg(workspaceOptions.settingsDir);
            // The test seam replaces the machine's device types; the engine then scans nothing on its own.
            options.devices = fakeDevices ? engine::DeviceMode::none : engine::DeviceMode::automatic;
        }
        engine = engine::createEngine(options);
        if (fakeDevices)
        {
            workspaceOptions.hooks->addAudioDeviceTypes(io::audioDeviceManager(*engine));
            // What DeviceMode::automatic does on creation: open the stored setup. An error is not fatal here, the
            // command that follows reports the state it finds.
            io::restoreAudioDeviceSetup(*engine);
        }
        session = std::make_unique<project::ProjectSession>(*engine, context);
        registry.setEditContext(&context);
        const auto registered = registerBuiltInCommands(registry, *engine, context, *session, TRACKLAB_VERSION_STRING);
        if (!registered.ok)
            throw operationFailed("internal_error", "registering the commands failed: " + registered.error.code + " " +
                                                        registered.error.message);
    }

    Workspace(const Workspace&) = delete;
    Workspace& operator=(const Workspace&) = delete;

    /** The command, executed through the registry; a failure becomes a CliFailure with the code of the command. */
    Json run(const std::string& id, const Json& params)
    {
        auto result = registry.execute(id, params);
        if (!result.ok)
            throw CliFailure(exitFailed, result.error.code, result.error.message, result.error.pointer);
        return result.result;
    }

    void openProject(const juce::File& file) { run("project.open", Json{{"path", pathOf(file)}}); }

    // Declaration order is the construction order, and members are destroyed in reverse: the registry first (its
    // handlers reference the others), then the session (closes the project), the context, and the engine last.
    std::unique_ptr<tracktion::Engine> engine;
    core::EditContext context;
    std::unique_ptr<project::ProjectSession> session;
    core::CommandRegistry registry;
};

//==============================================================================
// Subcommands: each returns the success object, or throws CliFailure

Json commandRender(Workspace& workspace, const ParsedArgs& args)
{
    const auto* format = args.value("--format");
    if (format != nullptr && *format != "wav24")
        throw usageError("unknown format \"" + *format + "\": only wav24 exists. render <project> --out <file>");

    workspace.openProject(fileFromArg(args.positionals[0]));
    return renderProject(*workspace.engine, *workspace.session->edit(), fileFromArg(*args.value("--out")));
}

/** The temporary render of a project for `analyze`: next to the engine's private folders if the caller set one. */
juce::File temporaryRenderFile(const std::string& engineTempDir)
{
    const auto folder =
        engineTempDir.empty() ? juce::File::getSpecialLocation(juce::File::tempDirectory) : fileFromArg(engineTempDir);
    folder.createDirectory();
    return folder.getNonexistentChildFile("tracklab-analyze", ".wav", false);
}

Json commandAnalyze(Workspace& workspace, const ParsedArgs& args, const std::string& engineTempDir)
{
    Measurements wanted{args.has("--loudness"), args.has("--truepeak"), args.has("--lra")};
    if (!wanted.loudness && !wanted.truePeak && !wanted.lra)
        wanted = Measurements{true, true, true};  // nothing asked for = everything

    const auto& source = args.positionals[0];
    Json result = Json::object();
    if (isProjectArg(source))
    {
        workspace.openProject(fileFromArg(source));
        const auto temporary = temporaryRenderFile(engineTempDir);
        try
        {
            renderProject(*workspace.engine, *workspace.session->edit(), temporary);
            result = measureLoudness(*workspace.engine, temporary, wanted);
        }
        catch (...)
        {
            temporary.deleteFile();
            throw;
        }
        temporary.deleteFile();
    }
    else
    {
        result = measureLoudness(*workspace.engine, fileFromArg(source), wanted);
    }
    result["source"] = source;
    return result;
}

/** The one io.* command of `io`, through the registry like every other caller. */
Json commandIo(Workspace& workspace, const std::string& id, const Json& params)
{
    return Json{{"result", workspace.run(id, params)}};
}

/** commands.json: an array of {"id": string, "params"?: object}. */
std::vector<core::BatchStep> loadCommands(const juce::File& file)
{
    if (!file.existsAsFile())
        throw operationFailed("commands_file_not_found", "no such commands file: " + pathOf(file));
    const auto text = file.loadFileAsString().toStdString();
    const auto json = Json::parse(text, nullptr, /*allow_exceptions*/ false);
    if (json.is_discarded())
        throw operationFailed("invalid_commands_file", "the commands file is not valid JSON: " + pathOf(file));
    if (!json.is_array())
        throw operationFailed("invalid_commands_file", "the commands file has to be a JSON array of steps");

    std::vector<core::BatchStep> steps;
    for (std::size_t i = 0; i < json.size(); ++i)
    {
        const auto& item = json[i];
        const auto where = "step " + std::to_string(i);
        if (!item.is_object() || !item.contains("id") || !item["id"].is_string())
            throw operationFailed("invalid_commands_file", where + " has to be an object with a string \"id\"");
        for (const auto& [key, value] : item.items())
            if (key != "id" && key != "params")
            {
                std::string message = where + " has the unknown key \"";
                message += key;
                message += '"';
                throw operationFailed("invalid_commands_file", message);
            }
        core::BatchStep step;
        step.id = item["id"].get<std::string>();
        if (item.contains("params"))
        {
            if (!item["params"].is_object())
                throw operationFailed("invalid_commands_file", where + ": \"params\" has to be an object");
            step.params = item["params"];
        }
        steps.push_back(std::move(step));
    }
    return steps;
}

/** Only commands that are undoable (rolled back with the batch) or readOnly (nothing to roll back) may run in a batch.
    Everything else (project.open/new/save/save_as/close, ...) has effects outside the Edit's undo history that a
    failed batch could not take back, and project.* would also fight with the CLI's own open and save. The rule is
    about the flags, not about names, so commands added later are covered. Checked before anything runs; ids the
    registry does not know are left to executeBatch (unknown_command). */
void requireBatchCommands(const core::CommandRegistry& registry, const std::vector<core::BatchStep>& steps)
{
    for (std::size_t i = 0; i < steps.size(); ++i)
    {
        const auto* command = registry.find(steps[i].id);
        if (command == nullptr || command->flags.readOnly || command->flags.undoable)
            continue;
        CliFailure failure(exitFailed, "command_not_allowed",
                           "\"" + steps[i].id +
                               "\" is neither undoable nor readOnly: run-commands cannot roll it back, "
                               "so it is not allowed in a commands file",
                           "/id");
        failure.failedIndex = i;
        throw failure;
    }
}

Json commandRunCommands(Workspace& workspace, const ParsedArgs& args, const CliHooks& hooks)
{
    const auto commandsFile = fileFromArg(args.positionals[1]);
    const auto steps = loadCommands(commandsFile);

    if (hooks.registerExtraCommands)
        hooks.registerExtraCommands(workspace.registry, workspace.context);
    requireBatchCommands(workspace.registry, steps);

    workspace.openProject(fileFromArg(args.positionals[0]));

    const auto batch = workspace.registry.executeBatch(runCommandsTransactionName, steps);
    if (hooks.afterCommands && workspace.session->edit() != nullptr)
        hooks.afterCommands(*workspace.session->edit());
    if (!batch.ok)
    {
        CliFailure failure(exitFailed, batch.error.code, batch.error.message, batch.error.pointer);
        failure.failedIndex = batch.failedIndex;
        throw failure;
    }

    // Saved through the commands as well. In place by default (atomic, like project.save); --save-as writes a NEW
    // project folder and leaves the opened file alone.
    Json info;
    if (const auto* saveAs = args.value("--save-as"))
    {
        const auto target = fileFromArg(*saveAs);
        info = workspace.run("project.save_as", Json{{"folder", pathOf(target.getParentDirectory())},
                                                     {"name", target.getFileName().toStdString()}});
    }
    else
    {
        info = workspace.run("project.save", Json::object());
    }

    return Json{{"commands", steps.size()}, {"results", batch.results}, {"saved", info["path"]}};
}

bool readWhole(const fs::path& file, std::string& text)
{
    std::ifstream in(file, std::ios::binary);
    if (!in)
        return false;
    std::ostringstream content;
    content << in.rdbuf();
    text = content.str();
    return true;
}

Json commandExportTools(Workspace& workspace, const ParsedArgs& args)
{
    const auto* toolsArg = args.value("--out");
    const auto* docsArg = args.value("--docs");
    if (toolsArg == nullptr && docsArg == nullptr)
        throw usageError("export-tools needs --out <tools.json> and/or --docs <commands.md>");

    struct Target
    {
        const std::string* arg;
        const char* key;
        std::string content;
    };
    std::vector<Target> targets;
    if (toolsArg != nullptr)
        targets.push_back({toolsArg, "tools", core::exportToolsJson(workspace.registry)});
    if (docsArg != nullptr)
        targets.push_back({docsArg, "docs", core::exportCommandsMarkdown(workspace.registry)});

    if (args.has("--check"))
    {
        std::string stale;
        for (const auto& target : targets)
        {
            std::string current;
            if (!readWhole(fs::path(pathOf(fileFromArg(*target.arg))), current) || current != target.content)
                stale += (stale.empty() ? "" : ", ") + pathOf(fileFromArg(*target.arg));
        }
        if (!stale.empty())
            throw operationFailed("export_stale", "out of date or missing: " + stale +
                                                      " (regenerate with `tracklab-cli export-tools`)");
        return Json{{"up_to_date", true}};
    }

    Json result = Json{{"commands", workspace.registry.size()}};
    for (const auto& target : targets)
    {
        const auto file = fs::path(pathOf(fileFromArg(*target.arg)));
        std::error_code ec;
        if (file.has_parent_path())
            fs::create_directories(file.parent_path(), ec);
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        if (ec || !out || !(out << target.content) || !out.flush())
            throw operationFailed("write_failed", "cannot write " + file.string());
        result[target.key] = pathOf(fileFromArg(*target.arg));
    }
    return result;
}

//==============================================================================

void printJson(std::ostream& out, const Json& json)
{
    // Invalid UTF-8 (a path from the command line) must not make the output throw.
    out << json.dump(-1, ' ', false, Json::error_handler_t::replace) << '\n';
}

void printFailure(std::ostream& out, std::ostream& err, const CliFailure& failure)
{
    Json error = Json{{"code", failure.code()}, {"message", failure.what()}};
    if (!failure.pointer().empty())
        error["pointer"] = failure.pointer();
    Json json = Json{{"ok", false}, {"error", error}};
    if (failure.failedIndex)
        json["failed_index"] = *failure.failedIndex;
    printJson(out, json);
    err << "tracklab-cli: " << failure.code() << ": " << failure.what() << '\n';
}

constexpr const char* generalUsage =
    "usage: tracklab-cli [--engine-temp-dir <dir>] render | analyze | run-commands | export-tools | io ...  "
    "(tracklab-cli --version)";

Json dispatch(const std::vector<std::string>& args, const CliHooks& hooks)
{
    std::string engineTempDir;
    std::size_t next = 0;
    if (next < args.size() && args[next] == "--engine-temp-dir")
    {
        if (next + 1 >= args.size())
            throw usageError(std::string("option --engine-temp-dir needs a value. ") + generalUsage);
        engineTempDir = args[next + 1];
        next += 2;
    }
    if (next >= args.size())
        throw usageError(generalUsage);

    const auto& command = args[next];
    if (command == "--version")
    {
        if (next + 1 != args.size())
            throw usageError("--version takes no arguments");
        return Json{{"version", TRACKLAB_VERSION_STRING}};
    }

    if (command == "render")
    {
        const auto parsed = parseArgs(args, next + 1,
                                      makeSpec({}, {"--out", "--format"}, 1, 1,
                                               "usage: tracklab-cli render <project> --out <file> [--format wav24]"));
        if (parsed.value("--out") == nullptr)
            throw usageError("render needs --out <file>");
        Workspace workspace(WorkspaceOptions{engineTempDir});
        return commandRender(workspace, parsed);
    }
    if (command == "analyze")
    {
        const auto parsed = parseArgs(
            args, next + 1,
            makeSpec({"--loudness", "--truepeak", "--lra", "--json"}, {}, 1, 1,
                     "usage: tracklab-cli analyze <file|project> [--loudness] [--truepeak] [--lra] [--json]"));
        Workspace workspace(WorkspaceOptions{engineTempDir});
        return commandAnalyze(workspace, parsed, engineTempDir);
    }
    if (command == "run-commands")
    {
        const auto parsed =
            parseArgs(args, next + 1,
                      makeSpec({}, {"--save-as"}, 2, 2,
                               "usage: tracklab-cli run-commands <project> <commands.json> [--save-as <out>]"));
        Workspace workspace(WorkspaceOptions{engineTempDir});
        return commandRunCommands(workspace, parsed, hooks);
    }
    if (command == "export-tools")
    {
        const auto parsed = parseArgs(
            args, next + 1,
            makeSpec({"--check"}, {"--out", "--docs"}, 0, 0,
                     "usage: tracklab-cli export-tools [--check] [--out <tools.json>] [--docs <commands.md>]"));
        if (parsed.value("--out") == nullptr && parsed.value("--docs") == nullptr)
            throw usageError("export-tools needs --out <tools.json> and/or --docs <commands.md>");
        Workspace workspace(WorkspaceOptions{engineTempDir});
        return commandExportTools(workspace, parsed);
    }
    if (command == "io")
    {
        const auto parsed = parseArgs(args, next + 1,
                                      makeSpec({}, {"--settings-dir"}, 1, 2,
                                               "usage: tracklab-cli io <command-id> [<params-json>] "
                                               "[--settings-dir <dir>]"));
        const auto& id = parsed.positionals[0];
        // Only device commands: the hand test path must not open a back door to project.* or app.* commands.
        if (id.rfind("io.", 0) != 0)
            throw usageError("\"" + id +
                             "\" is not an io.* command; io runs io.list_device_types, io.list_devices, "
                             "io.get_device or io.set_device");
        Json params = Json::object();
        if (parsed.positionals.size() > 1)
        {
            // A broken params argument is a mistake of the call, found before the engine is even created.
            params = Json::parse(parsed.positionals[1], nullptr, /*allow_exceptions*/ false);
            if (params.is_discarded())
                throw usageError("the params are not valid JSON: " + parsed.positionals[1]);
            if (!params.is_object())
                throw usageError("the params have to be a JSON object, e.g. {\"buffer_size\":256}");
        }
        WorkspaceOptions options{engineTempDir};
        options.realDevices = true;
        options.hooks = &hooks;
        if (const auto* settingsDir = parsed.value("--settings-dir"))
            options.settingsDir = *settingsDir;
        Workspace workspace(options);
        return commandIo(workspace, id, params);
    }
    throw usageError("unknown command \"" + command + "\". " + generalUsage);
}

}  // namespace

int runCli(const std::vector<std::string>& args, std::ostream& out, std::ostream& err, const CliHooks& hooks)
{
    try
    {
        Json result = dispatch(args, hooks);
        Json json = Json{{"ok", true}};
        json.update(result);
        printJson(out, json);
        return exitOk;
    }
    catch (const CliFailure& failure)
    {
        printFailure(out, err, failure);
        return failure.exitCode();
    }
    catch (const std::exception& e)
    {
        printFailure(out, err, CliFailure(exitFailed, "internal_error", e.what()));
        return exitFailed;
    }
}

}  // namespace tracklab::cli
