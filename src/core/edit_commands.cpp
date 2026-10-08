#include "core/edit_commands.h"

#include <stdexcept>
#include <utility>

namespace tracklab::core
{

namespace
{

const char* const noParamsSchema = R"({"type": "object", "properties": {}, "additionalProperties": false})";

const char* const stepResultSchema = R"({
    "type": "object",
    "properties": {"done": {"type": "boolean"}, "description": {"type": "string"}},
    "required": ["done", "description"],
    "additionalProperties": false
})";

// STUB (test-writer, M1-03): metadata and schemas are final, the handlers are implemented with the card.
Json notImplemented(const Json&)
{
    throw std::logic_error("edit command not implemented yet");
}

}  // namespace

RegisterResult registerEditCommands(CommandRegistry& registry, EditContext&)
{
    Command undo;
    undo.id = "edit.undo";
    undo.titleDe = "R\xC3\xBC"
                   "ckg\xC3\xA4ngig";
    undo.descriptionEn = "Undoes the last undo step of the project. Does nothing (done=false) if there is none.";
    undo.paramsSchema = Json::parse(noParamsSchema);
    undo.resultSchema = Json::parse(stepResultSchema);
    undo.shortcut = "Ctrl+Z";
    undo.menuPath = "Bearbeiten";
    undo.handler = notImplemented;
    if (auto outcome = registry.registerCommand(std::move(undo)); !outcome.ok)
        return outcome;

    Command redo;
    redo.id = "edit.redo";
    redo.titleDe = "Wiederholen";
    redo.descriptionEn = "Redoes the last undone step of the project. Does nothing (done=false) if there is none.";
    redo.paramsSchema = Json::parse(noParamsSchema);
    redo.resultSchema = Json::parse(stepResultSchema);
    redo.shortcut = "Ctrl+Shift+Z";
    redo.menuPath = "Bearbeiten";
    redo.handler = notImplemented;
    if (auto outcome = registry.registerCommand(std::move(redo)); !outcome.ok)
        return outcome;

    Command state;
    state.id = "edit.get_undo_state";
    state.titleDe = "Undo-Status abfragen";
    state.descriptionEn = "Returns whether undo and redo are possible and the names of the next undo and redo step.";
    state.paramsSchema = Json::parse(noParamsSchema);
    state.resultSchema = Json::parse(R"({
        "type": "object",
        "properties": {
            "can_undo": {"type": "boolean"},
            "can_redo": {"type": "boolean"},
            "undo_description": {"type": "string"},
            "redo_description": {"type": "string"}
        },
        "required": ["can_undo", "can_redo", "undo_description", "redo_description"],
        "additionalProperties": false
    })");
    state.flags.readOnly = true;
    state.handler = notImplemented;
    return registry.registerCommand(std::move(state));
}

}  // namespace tracklab::core
