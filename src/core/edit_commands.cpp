#include "core/edit_commands.h"

#include "core/transaction.h"

#include <tracktion_engine/tracktion_engine.h>

#include <string>
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

tracktion::Edit& requireEdit(const EditContext& context)
{
    if (context.edit() == nullptr)
        throw CommandFailure(error_code::noEdit, "no project is open");
    return *context.edit();
}

/** Shared by edit.undo and edit.redo. The name is read before the step is taken: afterwards it belongs to the other
    stack. An empty history is not an error, a shortcut on it is no mistake of the caller. */
Json historyStep(const EditContext& context, bool undo)
{
    auto& edit = requireEdit(context);
    // UndoManager::undo()/redo() start a new transaction: inside an open one (a batch or a macro command) they would
    // cut it in two. The wording is meant for Claude, who can then run undo as a separate call.
    if (Transaction::isOpen(edit))
        throw CommandFailure(error_code::undoInTransaction,
                             std::string(undo ? "undo" : "redo") +
                                 " is not possible inside a batch or macro that changes the project; call edit." +
                                 (undo ? "undo" : "redo") + " on its own");
    auto& manager = edit.getUndoManager();
    const bool possible = undo ? manager.canUndo() : manager.canRedo();
    const juce::String name = undo ? manager.getUndoDescription() : manager.getRedoDescription();
    const bool done = possible && (undo ? manager.undo() : manager.redo());
    return Json{{"done", done}, {"description", done ? name.toStdString() : std::string()}};
}

}  // namespace

RegisterResult registerEditCommands(CommandRegistry& registry, EditContext& context)
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
    undo.handler = [&context](const Json&) { return historyStep(context, true); };
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
    redo.handler = [&context](const Json&) { return historyStep(context, false); };
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
    state.handler = [&context](const Json&)
    {
        auto& manager = requireEdit(context).getUndoManager();
        return Json{{"can_undo", manager.canUndo()},
                    {"can_redo", manager.canRedo()},
                    {"undo_description", manager.getUndoDescription().toStdString()},
                    {"redo_description", manager.getRedoDescription().toStdString()}};
    };
    return registry.registerCommand(std::move(state));
}

}  // namespace tracklab::core
