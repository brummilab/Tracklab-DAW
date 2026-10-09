// Command registry v1 (M1-02): the single place where actions are registered and executed.
// GUI, shortcuts, Claude panel, MCP and CLI all call execute(); there is no second code path.
#pragma once

#include "core/command.h"
#include "core/edit_context.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tracklab::core
{

class CommandRegistry
{
public:
    CommandRegistry();
    ~CommandRegistry();
    CommandRegistry(CommandRegistry&&) noexcept;
    CommandRegistry& operator=(CommandRegistry&&) noexcept;
    CommandRegistry(const CommandRegistry&) = delete;
    CommandRegistry& operator=(const CommandRegistry&) = delete;

    /** Registers a command. On failure nothing changes. Checks, in this order:
        1. isValidCommandId(id)                        -> invalid_id            (pointer "/id")
        2. isValidToolName(toolNameFromId(id))         -> invalid_tool_name     (pointer "/id")
        3. handler is set                              -> missing_handler       (pointer "/handler")
        3b. titleDe and descriptionEn not empty        -> invalid_metadata      (pointer "/titleDe" or "/descriptionEn")
        4. readOnly not combined with undoable/destructive -> invalid_flags     (pointer "/flags")
        5. id not registered yet                       -> duplicate_id          (pointer "/id")
        6. tool name not used by another command       -> tool_name_collision   (pointer "/id")
        7. findSchemaSubsetViolation(paramsSchema)     -> invalid_schema        (pointer "/paramsSchema" + violation)
        8. findSchemaSubsetViolation(resultSchema)     -> invalid_schema        (pointer "/resultSchema" + violation)
        The schemas are compiled once here; a schema the validator cannot compile is also invalid_schema. */
    [[nodiscard]] RegisterResult registerCommand(Command command);

    std::size_t size() const noexcept;
    bool contains(std::string_view id) const;

    /** The command or nullptr. The pointer stays valid as long as the registry lives. */
    const Command* find(std::string_view id) const;

    /** All commands sorted by id (byte-wise), independent of the registration order. */
    std::vector<const Command*> list() const;

    /** Tool name of a registered id; nullopt if the id is unknown. */
    std::optional<std::string> toolNameForId(std::string_view id) const;

    /** Id of a registered tool name (reverse map, exact); nullopt if unknown. */
    std::optional<std::string> idForToolName(std::string_view toolName) const;

    /** Validates `params` against paramsSchema, runs the handler, validates its result against resultSchema.
        Must be called on the JUCE message thread, otherwise error not_on_message_thread (the handler is not run).
        Order: thread -> unknown_command -> invalid_params (handler not run) -> no_edit (undoable only, handler not run) ->
        handler -> invalid_result.
        The error of a validation is the FIRST problem the validator reports (pointer: the offending field; message:
        what was expected and what was received, see schema_validation.h).
        A handler that throws a std::exception gives handler_failed (the registry stays usable); one that throws a
        CommandFailure gives exactly its code, message and pointer (expected failures such as no_edit). */
    CommandResult execute(std::string_view id, const Json& params) const;

    //==========================================================================
    // Undo (M1-03, DESIGN Rev 3 section 3 "Undo")

    /** The project the commands work on; null = none (default). Not owned: it has to outlive the registry's use of it.
        Commands reach the Edit through the same EditContext (registerEditCommands(registry, context) etc.). */
    void setEditContext(EditContext* context) noexcept { editContextPtr = context; }
    EditContext* editContext() const noexcept { return editContextPtr; }

    /** execute() of an `undoable` command (flags.undoable) runs in exactly ONE undo transaction (core::Transaction)
        named titleDe of the command:
        - no entry for commands without the flag (readOnly, edit.undo, ...), and none if the command changed nothing;
        - no Edit in the EditContext (none set, or edit() null) -> error no_edit, the handler is not run;
        - a failed command (invalid_result, handler_failed, CommandFailure) leaves no trace: what the handler already
          changed is rolled back (Transaction::rollback), no undo entry, and the redo stack and the earlier undo
          history are as before (needs the Tracklab JUCE patch, see Transaction::rollback);
        - execute() called from inside a handler (a macro command) joins the transaction of the outer command
          instead of starting its own: one outer command = one undo step, named after the outer command. A nested
          command that fails is NOT rolled back on its own (its writes belong to the outer transaction); the outer
          handler decides: it lets the failure go (CommandFailure/exception) and the outer command is rolled back as a
          whole, or it swallows it and keeps the nested command's earlier writes;
        - edit.undo / edit.redo inside such a transaction (nested in a handler, or a step of a batch with an undoable
          step) -> error undo_in_transaction: they would cut the open transaction in two. */

    /** Runs the steps in order as ONE undo transaction named `nameDe` (a macro / a Claude turn). Every step is
        run like execute(), but without a transaction of its own, so the result is one undo step whatever the
        steps' flags.
        Up-front check: BEFORE any handler runs and before the transaction opens, every step is checked for what
        can be known without running it: unknown_command, invalid_params (schema), and no_edit (undoable step but no
        Edit). The step with the smallest index is reported (within a step in that order). A refused batch has
        written nothing: state, undo history and redo stack are untouched, and no transaction was ever open. The
        error carries `failedIndex` and the params-relative pointer of that step (not "/steps/<i>/...").
        Run-time errors (handler_failed, CommandFailure, invalid_result) can only be found by running: the steps
        already run are rolled back (Transaction::rollback: state as before, no undo entry, redo stack and earlier undo
        history as before) and the result carries the error and the index of the failing
        step; later steps are not run.
        An empty list is ok with no results. not_on_message_thread as for execute(). */
    BatchResult executeBatch(std::string_view nameDe, const std::vector<BatchStep>& steps) const;

private:
    struct Entry;  // the command plus its compiled schemas; hides the validator from this header

    /** Params check, handler, result check, without thread check and without a transaction: the common core of
        execute() and executeBatch(). A CommandFailure of the handler becomes its error 1:1, other exceptions
        handler_failed. */
    CommandResult runUnchecked(const Entry& entry, const Json& params) const;
    tracktion::Edit* currentEdit() const;  ///< the Edit of the EditContext, null if there is none

    std::map<std::string, std::unique_ptr<Entry>, std::less<>> commands;  // id -> entry (stable addresses)
    std::map<std::string, std::string, std::less<>> toolToId;             // tool name -> id
    EditContext* editContextPtr = nullptr;                                // not owned
};

}  // namespace tracklab::core
