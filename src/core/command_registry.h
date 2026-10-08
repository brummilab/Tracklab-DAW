// Command registry v1 (M1-02): the single place where actions are registered and executed.
// GUI, shortcuts, Claude panel, MCP and CLI all call execute(); there is no second code path.
#pragma once

#include "core/command.h"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tracklab::core
{

class CommandRegistry
{
public:
    /** Registers a command. On failure nothing changes. Checks, in this order:
        1. isValidCommandId(id)                        -> invalid_id            (pointer "/id")
        2. isValidToolName(toolNameFromId(id))         -> invalid_tool_name     (pointer "/id")
        3. handler is set                              -> missing_handler       (pointer "/handler")
        4. id not registered yet                       -> duplicate_id          (pointer "/id")
        5. tool name not used by another command       -> tool_name_collision   (pointer "/id")
        6. findSchemaSubsetViolation(paramsSchema)     -> invalid_schema        (pointer "/paramsSchema" + violation)
        7. findSchemaSubsetViolation(resultSchema)     -> invalid_schema        (pointer "/resultSchema" + violation) */
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
        Order: thread -> unknown_command -> invalid_params (handler not run) -> handler -> invalid_result.
        The error of a validation is the FIRST problem the validator reports (pointer: where; message: what). */
    CommandResult execute(std::string_view id, const Json& params) const;

private:
    std::map<std::string, Command, std::less<>> commands;      // id -> command (stable addresses)
    std::map<std::string, std::string, std::less<>> toolToId;  // tool name -> id
};

}  // namespace tracklab::core
