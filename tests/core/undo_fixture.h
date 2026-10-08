// Fixture of the undo tests (M1-03): a headless engine, an Edit made by the Tracklab edit factory, an EditContext, and a
// registry with the edit commands plus small test commands that change a private node of the Edit state.
// No audio device, no real user folder (testOptions()), no network.
#pragma once

#include "core/command_registry.h"
#include "core/core_test_helpers.h"
#include "core/edit_commands.h"
#include "core/edit_context.h"
#include "engine/edit_factory.h"
#include "engine/engine_factory.h"

#include "engine/engine_test_options.h"
#include "test_support.h"

#include <juce_events/juce_events.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Readable failure output for the lists of undo step names (doctest prints {?} otherwise).
namespace doctest
{
template <> struct StringMaker<std::vector<std::string>>
{
    static String convert(const std::vector<std::string>& names)
    {
        std::string text = "[";
        for (std::size_t i = 0; i < names.size(); ++i)
            text += (i == 0 ? "\"" : ", \"") + names[i] + "\"";
        return String((text + "]").c_str());
    }
};
}  // namespace doctest

namespace tracklab_test::undo
{

namespace te = tracktion;
using tracklab::core::Command;
using tracklab::core::CommandRegistry;
using tracklab::core::EditContext;
using tracklab::core::Json;

/** The node of the Edit state that the test commands change. Created without undo, so it is part of the baseline. */
inline const juce::Identifier testNodeType{"TRACKLAB_TEST"};

inline juce::ValueTree testNode(te::Edit& edit)
{
    return edit.state.getChildWithName(testNodeType);
}

/** Lets pending messages through, as the tests of the card have to: Edit updates and the UndoManager's change
    messages (which start Tracktion's 350 ms transaction timer). */
inline void settle(te::Edit& edit)
{
    edit.dispatchPendingUpdatesSynchronously();
    edit.getUndoManager().dispatchPendingMessages();
}

/** Runs the JUCE message loop (timers, async callbacks) for `ms` milliseconds. */
inline void pumpMessageLoop(int ms)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}

inline std::vector<std::string> toStd(const juce::StringArray& names)
{
    std::vector<std::string> out;
    for (const auto& name : names)
        out.push_back(name.toStdString());
    return out;
}

//==============================================================================
// Test commands. Group 1: behave as an undoable command has to.

inline Json valueResult(int value)
{
    return Json{{"value", value}};
}

inline te::Edit& editOf(EditContext& context)
{
    if (context.edit() == nullptr)
        throw std::runtime_error("test command without an Edit");
    return *context.edit();
}

inline Command makeUndoable(const std::string& id, const std::string& titleDe, Json paramsSchema = Json::object())
{
    using namespace core_helpers;
    Command command = makeCommand(id, paramsSchema.empty() ? objectSchema() : std::move(paramsSchema));
    command.titleDe = titleDe;
    command.flags.undoable = true;
    return command;
}

/** test.set_property {name, value}      undoable, title "Eigenschaft \xC3\xA4ndern" (umlaut: UTF-8 must survive)
    test.add_child {type}                undoable, appends a child node
    test.set_many {count, round}         undoable, one command, `count` writes
    test.set_two_with_pause              undoable, two writes with 500 ms of message loop in between
    test.macro                           undoable, calls execute("test.set_property") twice from its handler
    test.read_value {name}               readOnly
    test.nothing                         no flags at all (like edit.undo) */
inline void registerGoodTestCommands(CommandRegistry& registry, EditContext& context)
{
    using namespace core_helpers;

    {
        Command c =
            makeUndoable("test.set_property", "Eigenschaft \xC3\xA4ndern",
                         objectSchema(Json::parse(R"({"name": {"type": "string"}, "value": {"type": "integer"}})"),
                                      Json::array({"name", "value"})));
        c.handler = [&context](const Json& p)
        {
            auto& edit = editOf(context);
            testNode(edit).setProperty(juce::Identifier(p["name"].get<std::string>()), p["value"].get<int>(),
                                       &edit.getUndoManager());
            return valueResult(p["value"].get<int>());
        };
        registerOrFail(registry, c);
    }
    {
        Command c = makeUndoable("test.add_child", "Knoten anlegen",
                                 objectSchema(Json::parse(R"({"type": {"type": "string"}})"), Json::array({"type"})));
        c.handler = [&context](const Json& p)
        {
            auto& edit = editOf(context);
            auto node = testNode(edit);
            node.appendChild(juce::ValueTree(juce::Identifier(p["type"].get<std::string>())), &edit.getUndoManager());
            return valueResult(node.getNumChildren());
        };
        registerOrFail(registry, c);
    }
    {
        Command c = makeUndoable("test.set_many", "Mehrere Eigenschaften setzen",
                                 objectSchema(Json::parse(R"({"count": {"type": "integer", "minimum": 1},
                                                              "round": {"type": "integer"}})"),
                                              Json::array({"count", "round"})));
        c.handler = [&context](const Json& p)
        {
            auto& edit = editOf(context);
            auto node = testNode(edit);
            for (int i = 0; i < p["count"].get<int>(); ++i)
                node.setProperty(juce::Identifier("many" + juce::String(i)), p["round"].get<int>(),
                                 &edit.getUndoManager());
            return valueResult(p["count"].get<int>());
        };
        registerOrFail(registry, c);
    }
    {
        Command c = makeUndoable("test.set_two_with_pause", "Zwei Werte mit Pause");
        c.handler = [&context](const Json&)
        {
            auto& edit = editOf(context);
            testNode(edit).setProperty("first", 1, &edit.getUndoManager());
            pumpMessageLoop(500);  // longer than Tracktion's 350 ms transaction timer
            testNode(edit).setProperty("second", 2, &edit.getUndoManager());
            return valueResult(2);
        };
        registerOrFail(registry, c);
    }
    {
        Command c = makeUndoable("test.macro", "Makro");
        c.handler = [&registry](const Json&)
        {
            for (const char* name : {"macro_a", "macro_b"})
            {
                const auto result = registry.execute("test.set_property", Json{{"name", name}, {"value", 1}});
                if (!result.ok)
                    throw std::runtime_error("macro step failed: " + result.error.message);
            }
            return valueResult(2);
        };
        registerOrFail(registry, c);
    }
    {
        Command c = makeCommand("test.read_value",
                                objectSchema(Json::parse(R"({"name": {"type": "string"}})"), Json::array({"name"})));
        c.flags.readOnly = true;
        c.handler = [&context](const Json& p)
        {
            auto& edit = editOf(context);
            return valueResult(
                static_cast<int>(testNode(edit).getProperty(juce::Identifier(p["name"].get<std::string>()), -1)));
        };
        registerOrFail(registry, c);
    }
    {
        Command c = makeCommand("test.nothing");  // no flags, changes nothing
        registerOrFail(registry, c);
    }
}

/** Group 2: undoable commands that fail after they changed something. */
inline void registerFailingTestCommands(CommandRegistry& registry, EditContext& context)
{
    using namespace core_helpers;
    {
        Command c = makeUndoable("test.fail_after_write", "Schreiben und scheitern");
        c.handler = [&context](const Json&) -> Json
        {
            auto& edit = editOf(context);
            testNode(edit).setProperty("partial", 1, &edit.getUndoManager());
            throw std::runtime_error("failing on purpose");
        };
        registerOrFail(registry, c);
    }
    {
        Command c = makeUndoable("test.bad_result_after_write", "Schreiben und falsches Ergebnis");
        c.handler = [&context](const Json&)
        {
            auto& edit = editOf(context);
            testNode(edit).setProperty("partial", 1, &edit.getUndoManager());
            return Json{{"value", "not an integer"}};  // violates the result schema
        };
        registerOrFail(registry, c);
    }
}

/** Group 3: undoable commands that break the rule "undoable = writes through the UndoManager" (negative tests). */
inline void registerMisbehavingTestCommands(CommandRegistry& registry, EditContext& context)
{
    using namespace core_helpers;
    {
        Command c = makeUndoable("test.write_past_undo", "Am Undo vorbei");
        c.handler = [&context](const Json&)
        {
            testNode(editOf(context)).setProperty("sneaky", 1, nullptr);  // not undoable
            return valueResult(1);
        };
        registerOrFail(registry, c);
    }
    {
        Command c = makeUndoable("test.write_half_past_undo", "Halb am Undo vorbei");
        c.handler = [&context](const Json&)
        {
            auto& edit = editOf(context);
            testNode(edit).setProperty("tracked", 1, &edit.getUndoManager());
            testNode(edit).setProperty("sneaky", 1, nullptr);  // this half is not undoable
            return valueResult(1);
        };
        registerOrFail(registry, c);
    }
}

//==============================================================================
/** Engine + Edit + EditContext + registry. Member order = construction order; the Edit dies before the engine. */
struct UndoFixture
{
    explicit UndoFixture(int undoLevels = tracklab::engine::defaultUndoLevels, bool withTestCommands = true)
    {
        engine = tracklab::engine::createEngine(testOptions());
        REQUIRE(engine != nullptr);
        edit = tracklab::engine::createEdit(*engine, tracklab::engine::EditOptions{.undoLevels = undoLevels});
        REQUIRE(edit != nullptr);

        edit->state.getOrCreateChildWithName(testNodeType, nullptr);  // baseline, not undoable
        context.setEdit(edit.get());
        registry.setEditContext(&context);
        const auto outcome = tracklab::core::registerEditCommands(registry, context);
        INFO("registerEditCommands: " << outcome.error.code << " / " << outcome.error.message);
        REQUIRE(outcome.ok);
        if (withTestCommands)
            registerGoodTestCommands(registry, context);

        // Tracktion attaches its transaction timer to the UndoManager asynchronously: let that happen now.
        pumpMessageLoop(30);
        settle(*edit);
        edit->getUndoManager().clearUndoHistory();
        edit->getUndoManager().beginNewTransaction();
    }

    UndoFixture(const UndoFixture&) = delete;
    UndoFixture& operator=(const UndoFixture&) = delete;

    juce::UndoManager& undoManager() { return edit->getUndoManager(); }

    /** Names of the undo steps, the next one to be undone first. */
    std::vector<std::string> undoNames() { return toStd(undoManager().getUndoDescriptions()); }
    std::vector<std::string> redoNames() { return toStd(undoManager().getRedoDescriptions()); }

    /** Runs a command and requires success. */
    tracklab::core::CommandResult run(const std::string& id, const Json& params = Json::object())
    {
        auto result = registry.execute(id, params);
        INFO(id << ": " << result.error.code << " / " << result.error.message);
        REQUIRE(result.ok);
        return result;
    }

    Json setProperty(const std::string& name, int value)
    {
        return run("test.set_property", Json{{"name", name}, {"value", value}}).result;
    }

    int propertyValue(const std::string& name)
    {
        return static_cast<int>(testNode(*edit).getProperty(juce::Identifier(name), -1));
    }

    std::unique_ptr<te::Engine> engine;
    std::unique_ptr<te::Edit> edit;
    EditContext context;
    CommandRegistry registry;
};

}  // namespace tracklab_test::undo
