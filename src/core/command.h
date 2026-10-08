// Command model of Tracklab (M1-02): the types every input path (GUI, shortcuts, Claude panel, MCP, CLI) shares.
// A Command is one action with a JSON Schema for its parameters and its result; see command_registry.h.
#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tracklab::core
{

using Json = nlohmann::json;

/** Machine-readable values of CommandError::code. */
namespace error_code
{
// Registration (CommandRegistry::registerCommand)
inline constexpr std::string_view invalidId = "invalid_id";               ///< id does not match the id rules
inline constexpr std::string_view invalidToolName = "invalid_tool_name";  ///< derived tool name breaks the API rules
inline constexpr std::string_view duplicateId = "duplicate_id";           ///< id already registered
inline constexpr std::string_view toolNameCollision = "tool_name_collision";  ///< another id maps to the same tool name
inline constexpr std::string_view invalidSchema = "invalid_schema";           ///< schema outside the allowed subset
inline constexpr std::string_view missingHandler = "missing_handler";         ///< Command::handler is empty
inline constexpr std::string_view invalidMetadata = "invalid_metadata";       ///< titleDe or descriptionEn is empty
inline constexpr std::string_view invalidFlags = "invalid_flags";  ///< readOnly together with undoable/destructive
// Execution (CommandRegistry::execute)
inline constexpr std::string_view unknownCommand = "unknown_command";  ///< no command with this id
inline constexpr std::string_view invalidParams = "invalid_params";    ///< params violate paramsSchema
inline constexpr std::string_view invalidResult = "invalid_result";    ///< handler result violates resultSchema
inline constexpr std::string_view handlerFailed = "handler_failed";    ///< the handler threw a std::exception
inline constexpr std::string_view notOnMessageThread = "not_on_message_thread";  ///< called from another thread
inline constexpr std::string_view noEdit = "no_edit";  ///< an undoable command needs an open project (M1-03)
}  // namespace error_code

/** Structured error, serialised as {"code":..., "message":..., "pointer":...}. All three are strings.
    `pointer` is an RFC 6901 JSON Pointer ("" = the whole document) to the offending place:
    - execution errors: into the params / the result,
    - registration errors: into the Command ("/id", "/paramsSchema/properties/x/pattern", "/resultSchema", ...). */
struct CommandError
{
    std::string code;
    std::string message;
    std::string pointer;

    /** {"code":..., "message":..., "pointer":...} */
    Json toJson() const;
};

/** What a handler throws for an expected, domain-level failure (no project open, unknown track id, ...): the registry
    reports it 1:1 as CommandError{code, message, pointer} instead of the generic handler_failed. `code` is a stable
    machine-readable string (error_code::... or a command's own); `pointer` is an RFC 6901 JSON Pointer into the params
    ("" = none in particular). It is a std::runtime_error (what() = message), so code that only knows
    std::exception still sees a sensible text. Like any failure of an undoable command it rolls the command back. */
class CommandFailure : public std::runtime_error
{
public:
    CommandFailure(std::string_view code, const std::string& message, std::string pointer = {})
        : std::runtime_error(message), failureCode(code), failurePointer(std::move(pointer))
    {
    }

    const std::string& code() const noexcept { return failureCode; }
    const std::string& pointer() const noexcept { return failurePointer; }

private:
    std::string failureCode;
    std::string failurePointer;
};

/** Outcome of CommandRegistry::execute. */
struct CommandResult
{
    bool ok = false;
    Json result;         ///< valid if ok (validated against resultSchema)
    CommandError error;  ///< valid if !ok

    /** ok: {"ok":true,"result":<result>}; otherwise {"ok":false,"error":{"code","message","pointer"}}. */
    Json toJson() const;
};

/** One step of CommandRegistry::executeBatch. */
struct BatchStep
{
    std::string id;
    Json params = Json::object();
};

/** Outcome of CommandRegistry::executeBatch (M1-03). */
struct BatchResult
{
    bool ok = false;
    std::vector<Json> results;    ///< ok: one validated result per step, in order
    CommandError error;           ///< !ok: the error of the failing step (same codes as execute)
    std::size_t failedIndex = 0;  ///< !ok: index of the failing step in the list
};

/** Outcome of CommandRegistry::registerCommand. */
struct RegisterResult
{
    bool ok = false;
    CommandError error;  ///< valid if !ok
};

struct CommandFlags
{
    bool readOnly = false;     ///< does not change the project
    bool undoable = false;     ///< changes the project through the undo manager
    bool destructive = false;  ///< always needs a confirmation (also for Claude), see CLAUDE.md
    bool longRunning = false;  ///< runs as a job, may report progress

    friend bool operator==(const CommandFlags&, const CommandFlags&) = default;
};

struct Command
{
    /** "<namespace>.<name>", e.g. "track.create" or "assistant.propose_plan". Rules: isValidCommandId(). */
    std::string id;
    std::string titleDe;        ///< Menu / action list (German)
    std::string descriptionEn;  ///< Tool description for Claude/MCP (English)

    /** Both schemas are JSON Schema draft 7 restricted to the subset of schema_subset.h; the root is an object. */
    Json paramsSchema;
    Json resultSchema;

    CommandFlags flags;
    std::string shortcut;  ///< Default shortcut, may be empty
    std::string menuPath;  ///< e.g. "Spur/Neu", may be empty

    /** Runs on the message thread with params that passed paramsSchema; the returned Json is checked against
        resultSchema. */
    std::function<Json(const Json& params)> handler;
};

}  // namespace tracklab::core
