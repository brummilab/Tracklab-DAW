// edit.undo, edit.redo, edit.get_undo_state (M1-03): the undo history as commands of the registry.
#include "core/undo_contract.h"
#include "core/undo_fixture.h"

#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::undo;
using namespace tracklab::core;

const std::string setPropertyTitle = "Eigenschaft \xC3\xA4ndern";

Json undoState(UndoFixture& f)
{
    return f.run("edit.get_undo_state").result;
}

}  // namespace

TEST_SUITE("core")
{
    TEST_CASE("the edit commands are registered with their flags: undo and redo have none, get_undo_state is readOnly")
    {
        UndoFixture f;

        const Command* undo = f.registry.find("edit.undo");
        const Command* redo = f.registry.find("edit.redo");
        const Command* state = f.registry.find("edit.get_undo_state");
        REQUIRE(undo != nullptr);
        REQUIRE(redo != nullptr);
        REQUIRE(state != nullptr);

        // edit.undo/redo manipulate the history itself: they must not open a transaction (no `undoable`).
        CHECK(undo->flags == CommandFlags{});
        CHECK(redo->flags == CommandFlags{});
        CHECK(state->flags == CommandFlags{.readOnly = true});
        for (const Command* command : {undo, redo, state})
        {
            CHECK_FALSE(command->titleDe.empty());
            CHECK_FALSE(command->descriptionEn.empty());
        }
        CHECK(f.registry.toolNameForId("edit.get_undo_state") == "edit_get_undo_state");
    }

    TEST_CASE("registering the edit commands twice on one registry gives duplicate_id")
    {
        UndoFixture f;
        const auto again = registerEditCommands(f.registry, f.context);
        CHECK_FALSE(again.ok);
        CHECK(again.error.code == error_code::duplicateId);
    }

    TEST_CASE("edit.get_undo_state of a fresh project: nothing to undo or redo")
    {
        UndoFixture f;
        CHECK(undoState(f) == Json::parse(R"({"can_undo": false, "can_redo": false,
                                              "undo_description": "", "redo_description": ""})"));
    }

    TEST_CASE("edit.get_undo_state names the next undo step, and after an undo the next redo step")
    {
        UndoFixture f;
        f.setProperty("alpha", 1);
        f.run("test.add_child", Json{{"type", "CHILD"}});
        settle(*f.edit);

        CHECK(undoState(f) == Json{{"can_undo", true},
                                   {"can_redo", false},
                                   {"undo_description", "Knoten anlegen"},
                                   {"redo_description", ""}});

        REQUIRE(f.undoManager().undo());
        settle(*f.edit);
        CHECK(undoState(f) == Json{{"can_undo", true},
                                   {"can_redo", true},
                                   {"undo_description", setPropertyTitle},
                                   {"redo_description", "Knoten anlegen"}});
    }

    TEST_CASE("edit.get_undo_state creates no undo entry and does not touch the redo stack")
    {
        UndoFixture f;
        f.setProperty("alpha", 1);
        f.setProperty("beta", 2);
        REQUIRE(f.undoManager().undo());
        settle(*f.edit);
        const auto undoBefore = f.undoNames();
        const auto redoBefore = f.redoNames();

        (void)undoState(f);
        (void)undoState(f);
        settle(*f.edit);

        CHECK(f.undoNames() == undoBefore);
        CHECK(f.redoNames() == redoBefore);
    }

    TEST_CASE("edit.undo takes back the last command and reports its name")
    {
        UndoFixture f;
        const auto before = normalisedState(*f.edit);
        f.setProperty("alpha", 5);
        settle(*f.edit);

        const auto result = f.run("edit.undo");
        settle(*f.edit);

        CHECK(result.result == Json{{"done", true}, {"description", setPropertyTitle}});
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.propertyValue("alpha") == -1);
    }

    TEST_CASE("edit.redo repeats the undone command and reports its name")
    {
        UndoFixture f;
        f.setProperty("alpha", 5);
        const auto afterCommand = normalisedState(*f.edit);
        settle(*f.edit);
        f.run("edit.undo");
        settle(*f.edit);

        const auto result = f.run("edit.redo");
        settle(*f.edit);

        CHECK(result.result == Json{{"done", true}, {"description", setPropertyTitle}});
        CHECK(normalisedState(*f.edit) == afterCommand);
        CHECK(f.propertyValue("alpha") == 5);
    }

    TEST_CASE("edit.undo and edit.redo walk through the history step by step")
    {
        UndoFixture f;
        const auto start = normalisedState(*f.edit);
        f.setProperty("alpha", 1);
        const auto afterA = normalisedState(*f.edit);
        f.run("test.add_child", Json{{"type", "CHILD"}});
        const auto afterB = normalisedState(*f.edit);
        settle(*f.edit);

        const auto undoB = f.run("edit.undo");
        settle(*f.edit);
        CHECK(undoB.result["description"] == "Knoten anlegen");
        CHECK(normalisedState(*f.edit) == afterA);

        const auto undoA = f.run("edit.undo");
        settle(*f.edit);
        CHECK(undoA.result["description"] == setPropertyTitle);
        CHECK(normalisedState(*f.edit) == start);

        const auto redoA = f.run("edit.redo");
        settle(*f.edit);
        CHECK(redoA.result["description"] == setPropertyTitle);
        CHECK(normalisedState(*f.edit) == afterA);

        const auto redoB = f.run("edit.redo");
        settle(*f.edit);
        CHECK(redoB.result["description"] == "Knoten anlegen");
        CHECK(normalisedState(*f.edit) == afterB);
    }

    TEST_CASE("edit.undo and edit.redo themselves create no undo entry")
    {
        UndoFixture f;
        f.setProperty("alpha", 1);
        settle(*f.edit);

        f.run("edit.undo");
        settle(*f.edit);
        CHECK(f.undoNames().empty());
        CHECK(f.redoNames() == std::vector<std::string>{setPropertyTitle});

        f.run("edit.redo");
        settle(*f.edit);
        CHECK(f.undoNames() == std::vector<std::string>{setPropertyTitle});
        CHECK(f.redoNames().empty());
    }

    TEST_CASE("edit.undo takes back a whole batch in one step")
    {
        UndoFixture f;
        const auto before = normalisedState(*f.edit);
        const auto batch = f.registry.executeBatch("Claude: Beispiel",
                                                   {BatchStep{"test.set_property", Json{{"name", "a"}, {"value", 1}}},
                                                    BatchStep{"test.set_property", Json{{"name", "b"}, {"value", 2}}}});
        REQUIRE(batch.ok);
        settle(*f.edit);

        const auto result = f.run("edit.undo");
        settle(*f.edit);

        CHECK(result.result == Json{{"done", true}, {"description", "Claude: Beispiel"}});
        CHECK(normalisedState(*f.edit) == before);
    }

    TEST_CASE("edit.undo with an empty history is ok, does nothing and says done=false")
    {
        // Decision of the tests (open point for the lead): no error, because Ctrl+Z on an empty history is not a
        // mistake of the caller.
        UndoFixture f;
        const auto before = normalisedState(*f.edit);

        const auto result = f.registry.execute("edit.undo", Json::object());
        settle(*f.edit);

        REQUIRE(result.ok);
        CHECK(result.result == Json{{"done", false}, {"description", ""}});
        CHECK(normalisedState(*f.edit) == before);
        CHECK_FALSE(f.undoManager().canUndo());
        CHECK_FALSE(f.undoManager().canRedo());
    }

    TEST_CASE("edit.redo with an empty redo stack is ok, does nothing and says done=false")
    {
        UndoFixture f;
        f.setProperty("alpha", 1);  // something to undo, but nothing to redo
        settle(*f.edit);
        const auto before = normalisedState(*f.edit);

        const auto result = f.registry.execute("edit.redo", Json::object());
        settle(*f.edit);

        REQUIRE(result.ok);
        CHECK(result.result == Json{{"done", false}, {"description", ""}});
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames() == std::vector<std::string>{setPropertyTitle});
    }

    TEST_CASE("after a new command there is nothing to redo")
    {
        UndoFixture f;
        f.setProperty("alpha", 1);
        f.run("edit.undo");
        f.setProperty("beta", 2);
        settle(*f.edit);

        const auto result = f.run("edit.redo");

        CHECK(result.result == Json{{"done", false}, {"description", ""}});
        CHECK(f.propertyValue("alpha") == -1);
        CHECK(f.propertyValue("beta") == 2);
    }

    TEST_CASE("the edit commands take no parameters")
    {
        UndoFixture f;
        for (const char* id : {"edit.undo", "edit.redo", "edit.get_undo_state"})
        {
            CAPTURE(id);
            const auto result = f.registry.execute(id, Json{{"steps", 2}});
            CHECK_FALSE(result.ok);
            CHECK(result.error.code == error_code::invalidParams);
        }
    }

    TEST_CASE("the edit commands need an open project: no_edit")
    {
        UndoFixture f;
        f.context.setEdit(nullptr);
        for (const char* id : {"edit.undo", "edit.redo", "edit.get_undo_state"})
        {
            CAPTURE(id);
            const auto result = f.registry.execute(id, Json::object());
            CHECK_FALSE(result.ok);
            CHECK(result.error.code == error_code::noEdit);
        }
    }

    TEST_CASE("edit.undo is not recorded when a command that was the last step failed afterwards")
    {
        // The step list stays consistent through failures: a failed undoable command is no step, so edit.undo reverts
        // the command before it.
        UndoFixture f;
        registerFailingTestCommands(f.registry, f.context);
        f.setProperty("alpha", 1);
        (void)f.registry.execute("test.fail_after_write", Json::object());
        settle(*f.edit);

        const auto result = f.run("edit.undo");

        CHECK(result.result == Json{{"done", true}, {"description", setPropertyTitle}});
        CHECK(f.propertyValue("alpha") == -1);
    }
}
