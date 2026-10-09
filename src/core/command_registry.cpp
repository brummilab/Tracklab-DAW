#include "core/command_registry.h"

#include "core/schema_subset.h"
#include "core/schema_validation.h"
#include "core/tool_names.h"
#include "core/transaction.h"

#include <juce_events/juce_events.h>
#include <tracktion_engine/tracktion_engine.h>

#include <exception>
#include <utility>

namespace tracklab::core
{

struct CommandRegistry::Entry
{
    Command command;
    CompiledSchema params;
    CompiledSchema result;
};

CommandRegistry::CommandRegistry() = default;
CommandRegistry::~CommandRegistry() = default;
CommandRegistry::CommandRegistry(CommandRegistry&&) noexcept = default;
CommandRegistry& CommandRegistry::operator=(CommandRegistry&&) noexcept = default;

namespace
{

RegisterResult refuse(std::string_view code, std::string message, std::string pointer)
{
    RegisterResult result;
    result.error = CommandError{std::string(code), std::move(message), std::move(pointer)};
    return result;
}

CommandResult fail(std::string_view code, std::string message, std::string pointer = {})
{
    CommandResult result;
    result.error = CommandError{std::string(code), std::move(message), std::move(pointer)};
    return result;
}

/** Subset check plus compilation of one schema; `where` is "/paramsSchema" or "/resultSchema". */
std::optional<RegisterResult> prepareSchema(const Json& schema, CompiledSchema& compiled, const std::string& where)
{
    if (auto violation = findSchemaSubsetViolation(schema))
        return refuse(violation->code, "invalid " + where.substr(1) + ": " + violation->message,
                      where + violation->pointer);
    // The subset is far smaller than what the validator accepts, but it can still refuse a schema it cannot load.
    if (auto problem = compiled.compile(schema))
        return refuse(error_code::invalidSchema, "invalid " + where.substr(1) + ": " + *problem, where);
    return std::nullopt;
}

}  // namespace

RegisterResult CommandRegistry::registerCommand(Command command)
{
    const std::string toolName = toolNameFromId(command.id);

    if (!isValidCommandId(command.id))
        return refuse(error_code::invalidId,
                      "command id '" + command.id +
                          "' is invalid: use at least two segments separated by '.', each segment lowercase "
                          "[a-z][a-z0-9_]* (e.g. track.create)",
                      "/id");
    if (!isValidToolName(toolName))
        return refuse(error_code::invalidToolName,
                      "tool name '" + toolName + "' of command '" + command.id +
                          "' is not allowed: at most 128 characters",
                      "/id");
    if (!command.handler)
        return refuse(error_code::missingHandler, "command '" + command.id + "' has no handler", "/handler");
    // Both texts are shown to people (menu, command list) and to Claude (tool description): empty is a mistake.
    if (command.titleDe.empty())
        return refuse(error_code::invalidMetadata, "command '" + command.id + "' has an empty titleDe", "/titleDe");
    if (command.descriptionEn.empty())
        return refuse(error_code::invalidMetadata, "command '" + command.id + "' has an empty descriptionEn",
                      "/descriptionEn");
    if (command.flags.readOnly && (command.flags.undoable || command.flags.destructive))
        return refuse(error_code::invalidFlags,
                      "command '" + command.id + "': readOnly cannot be combined with undoable or destructive",
                      "/flags");
    if (commands.contains(command.id))
        return refuse(error_code::duplicateId, "command '" + command.id + "' is already registered", "/id");
    if (const auto clash = toolToId.find(toolName); clash != toolToId.end())
        return refuse(error_code::toolNameCollision,
                      "tool name '" + toolName + "' of command '" + command.id + "' is already used by command '" +
                          clash->second + "'",
                      "/id");

    auto entry = std::make_unique<Entry>();
    if (auto refused = prepareSchema(command.paramsSchema, entry->params, "/paramsSchema"))
        return *refused;
    if (auto refused = prepareSchema(command.resultSchema, entry->result, "/resultSchema"))
        return *refused;

    // Everything is checked: from here on nothing can fail, so a refused registration never leaves traces.
    const std::string id = command.id;
    entry->command = std::move(command);
    toolToId.emplace(toolName, id);
    commands.emplace(id, std::move(entry));

    RegisterResult result;
    result.ok = true;
    return result;
}

std::size_t CommandRegistry::size() const noexcept
{
    return commands.size();
}

bool CommandRegistry::contains(std::string_view id) const
{
    return commands.find(id) != commands.end();
}

const Command* CommandRegistry::find(std::string_view id) const
{
    const auto it = commands.find(id);
    return it == commands.end() ? nullptr : &it->second->command;
}

std::vector<const Command*> CommandRegistry::list() const
{
    std::vector<const Command*> all;
    all.reserve(commands.size());
    for (const auto& [id, entry] : commands)  // std::map: sorted by id
        all.push_back(&entry->command);
    return all;
}

std::optional<std::string> CommandRegistry::toolNameForId(std::string_view id) const
{
    if (!contains(id))
        return std::nullopt;
    return toolNameFromId(id);
}

std::optional<std::string> CommandRegistry::idForToolName(std::string_view toolName) const
{
    const auto it = toolToId.find(toolName);
    if (it == toolToId.end())
        return std::nullopt;
    return it->second;
}

CommandResult CommandRegistry::runUnchecked(const Entry& entry, const Json& params) const
{
    const std::string& id = entry.command.id;
    if (const auto problem = entry.params.validate(params))
        return fail(error_code::invalidParams, problem->message, problem->pointer);

    Json output;
    try
    {
        output = entry.command.handler(params);
    }
    catch (const CommandFailure& failure)
    {
        // An expected, domain-level failure: the handler chose code, message and pointer, pass them on unchanged.
        return fail(failure.code(), failure.what(), failure.pointer());
    }
    catch (const std::exception& error)
    {
        return fail(error_code::handlerFailed, "command '" + id + "' failed: " + error.what());
    }
    catch (...)
    {
        return fail(error_code::handlerFailed, "command '" + id + "' failed with an unknown exception");
    }

    // A handler that breaks its own contract is a bug of ours; refuse the result instead of passing it on to Claude.
    if (const auto problem = entry.result.validate(output))
        return fail(error_code::invalidResult, "command '" + id + "' returned an invalid result: " + problem->message,
                    problem->pointer);

    CommandResult result;
    result.ok = true;
    result.result = std::move(output);
    return result;
}

tracktion::Edit* CommandRegistry::currentEdit() const
{
    return editContextPtr != nullptr ? editContextPtr->edit() : nullptr;
}

CommandResult CommandRegistry::execute(std::string_view id, const Json& params) const
{
    // Handlers touch the project and the GUI, which belong to the message thread. Not a jassert: a caller on the
    // wrong thread (MCP server, CLI worker) gets an error it can report instead of a debug-only crash.
    // existsAndIsCurrentThread() never creates the MessageManager, so a worker cannot become the message thread.
    if (!juce::MessageManager::existsAndIsCurrentThread())
        return fail(error_code::notOnMessageThread, "commands run on the message thread only; '" + std::string(id) +
                                                        "' was called from another thread");

    const auto it = commands.find(id);
    if (it == commands.end())
        return fail(error_code::unknownCommand, "no command '" + std::string(id) + "' is registered");
    const Entry& entry = *it->second;

    if (!entry.command.flags.undoable)
        return runUnchecked(entry, params);

    // A call that is refused for its params never reaches the project, so it is not worth an Edit or a transaction.
    if (const auto problem = entry.params.validate(params))
        return fail(error_code::invalidParams, problem->message, problem->pointer);

    tracktion::Edit* edit = currentEdit();
    if (edit == nullptr)
        return fail(error_code::noEdit, "command '" + std::string(id) + "' needs an open project");

    // One undoable command = one undo step named after the command. Called from inside a handler (a macro), the
    // Transaction joins the outer one, so the macro stays one step.
    Transaction transaction(*edit, entry.command.titleDe);
    auto result = runUnchecked(entry, params);
    // A failure must leave no trace: what the handler wrote before it failed is taken back, no undo entry remains.
    if (!result.ok)
        transaction.rollback();
    return result;
}

BatchResult CommandRegistry::executeBatch(std::string_view nameDe, const std::vector<BatchStep>& steps) const
{
    BatchResult batch;
    const auto failWith = [&batch](CommandError error, std::size_t index)
    {
        batch.ok = false;
        batch.results.clear();
        batch.error = std::move(error);
        batch.failedIndex = index;
        return batch;
    };

    if (!juce::MessageManager::existsAndIsCurrentThread())
        return failWith(CommandError{std::string(error_code::notOnMessageThread),
                                     "commands run on the message thread only; the batch '" + std::string(nameDe) +
                                         "' was called from another thread",
                                     {}},
                        0);

    // Up-front check of EVERY step before any handler runs or a transaction opens: whatever can be known without
    // running (unknown id, params schema, no Edit for an undoable step) refuses the whole batch with no write at all.
    // Why: a batch that is bound to fail should not write at all, not even write and take it back (the rollback is
    // exact, see Transaction::rollback, but a refusal leaves no trace in the manager). The step with the smallest index is
    // reported (within a step in the order of execute(): unknown_command, invalid_params, no_edit), so the answer
    // does not depend on which problem happens to be found first.
    tracktion::Edit* edit = currentEdit();
    bool anyUndoable = false;
    for (std::size_t i = 0; i < steps.size(); ++i)
    {
        const auto it = commands.find(steps[i].id);
        if (it == commands.end())
            return failWith(CommandError{std::string(error_code::unknownCommand),
                                         "no command '" + steps[i].id + "' is registered",
                                         {}},
                            i);
        const Entry& entry = *it->second;
        if (const auto problem = entry.params.validate(steps[i].params))
            return failWith(CommandError{std::string(error_code::invalidParams), problem->message, problem->pointer},
                            i);
        if (entry.command.flags.undoable)
        {
            if (edit == nullptr)
                return failWith(CommandError{std::string(error_code::noEdit),
                                             "command '" + steps[i].id + "' needs an open project",
                                             {}},
                                i);
            anyUndoable = true;
        }
    }

    // Only a batch with an undoable step needs a transaction (and so can leave an undo entry).
    std::optional<Transaction> transaction;
    if (anyUndoable)
        transaction.emplace(*edit, std::string(nameDe));

    for (std::size_t i = 0; i < steps.size(); ++i)
    {
        // Found and params-checked above: what can still fail here is a run-time error, and that is rolled back.
        auto step = runUnchecked(*commands.find(steps[i].id)->second, steps[i].params);
        if (!step.ok)
        {
            // All or nothing: the steps before and the failing step's own writes are taken back.
            if (transaction)
                transaction->rollback();
            return failWith(std::move(step.error), i);
        }
        batch.results.push_back(std::move(step.result));
    }

    batch.ok = true;
    return batch;
}

}  // namespace tracklab::core
