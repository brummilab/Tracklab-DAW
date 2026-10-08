// CommandRegistry::execute and undo (M1-03): every undoable command runs in exactly one transaction named after its
// titleDe; readOnly and flag-less commands leave the undo history alone; a failed command leaves no trace.
#include "core/undo_contract.h"
#include "core/undo_fixture.h"

#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::undo;
using namespace tracklab::core;

const std::string setPropertyTitle = "Eigenschaft \xC3\xA4ndern";

}  // namespace

TEST_SUITE("core")
{
    TEST_CASE("an undoable command runs in one undo step named after its titleDe")
    {
        UndoFixture f;
        const auto before = normalisedState(*f.edit);

        f.setProperty("alpha", 5);
        settle(*f.edit);

        CHECK(f.propertyValue("alpha") == 5);
        CHECK(f.undoNames() == std::vector<std::string>{setPropertyTitle});
        CHECK(f.undoManager().undo());
        settle(*f.edit);
        CHECK(normalisedState(*f.edit) == before);
    }

    TEST_CASE("a command with many writes is one undo step")
    {
        UndoFixture f;
        const auto before = normalisedState(*f.edit);

        f.run("test.set_many", Json{{"count", 25}, {"round", 1}});
        settle(*f.edit);

        CHECK(f.undoNames() == std::vector<std::string>{"Mehrere Eigenschaften setzen"});
        CHECK(f.undoManager().undo());
        settle(*f.edit);
        CHECK(normalisedState(*f.edit) == before);
    }

    TEST_CASE("commands in a row are separate undo steps, the newest is undone first")
    {
        UndoFixture f;
        f.setProperty("alpha", 1);
        f.run("test.add_child", Json{{"type", "CHILD"}});
        f.setProperty("beta", 2);
        settle(*f.edit);

        CHECK(f.undoNames() == std::vector<std::string>{setPropertyTitle, "Knoten anlegen", setPropertyTitle});

        REQUIRE(f.undoManager().undo());
        CHECK(f.propertyValue("beta") == -1);
        CHECK(f.propertyValue("alpha") == 1);
        CHECK(testNode(*f.edit).getNumChildren() == 1);
    }

    TEST_CASE("the 350 ms timer does not cut a command whose handler pauses")
    {
        UndoFixture f;
        const auto before = normalisedState(*f.edit);

        f.run("test.set_two_with_pause");  // message loop runs 500 ms between its two writes
        settle(*f.edit);

        CHECK(f.undoNames() == std::vector<std::string>{"Zwei Werte mit Pause"});
        CHECK(f.undoManager().undo());
        settle(*f.edit);
        CHECK(normalisedState(*f.edit) == before);
    }

    TEST_CASE("a command that executes other commands from its handler is ONE undo step named after the outer command")
    {
        UndoFixture f;
        const auto before = normalisedState(*f.edit);

        f.run("test.macro");
        settle(*f.edit);

        CHECK(f.propertyValue("macro_a") == 1);
        CHECK(f.propertyValue("macro_b") == 1);
        CHECK(f.undoNames() == std::vector<std::string>{"Makro"});
        CHECK(f.undoManager().undo());
        settle(*f.edit);
        CHECK(normalisedState(*f.edit) == before);
    }

    TEST_CASE("a readOnly command creates no undo entry")
    {
        UndoFixture f;
        const auto result = f.run("test.read_value", Json{{"name", "alpha"}});
        settle(*f.edit);

        CHECK(result.result == Json{{"value", -1}});
        CHECK_FALSE(f.undoManager().canUndo());
        CHECK(f.undoNames().empty());
    }

    TEST_CASE("a readOnly command neither adds a step nor touches the undo and redo stacks")
    {
        UndoFixture f;
        f.setProperty("alpha", 1);
        f.setProperty("beta", 2);
        REQUIRE(f.undoManager().undo());  // "beta" is on the redo stack
        settle(*f.edit);
        const auto undoBefore = f.undoNames();
        const auto redoBefore = f.redoNames();
        REQUIRE(undoBefore.size() == 1);
        REQUIRE(redoBefore.size() == 1);

        f.run("test.read_value", Json{{"name", "alpha"}});
        settle(*f.edit);

        CHECK(f.undoNames() == undoBefore);
        CHECK(f.redoNames() == redoBefore);
    }

    TEST_CASE("a command without flags creates no undo entry and keeps the redo stack")
    {
        UndoFixture f;
        f.setProperty("alpha", 1);
        REQUIRE(f.undoManager().undo());
        settle(*f.edit);

        f.run("test.nothing");
        settle(*f.edit);

        CHECK_FALSE(f.undoManager().canUndo());
        CHECK(f.redoNames() == std::vector<std::string>{setPropertyTitle});
    }

    TEST_CASE("a refused call (invalid params, unknown command) creates no undo entry and keeps the redo stack")
    {
        UndoFixture f;
        f.setProperty("alpha", 1);
        REQUIRE(f.undoManager().undo());
        settle(*f.edit);

        const auto invalid = f.registry.execute("test.set_property", Json{{"name", "alpha"}});  // "value" missing
        const auto unknown = f.registry.execute("test.does_not_exist", Json::object());
        settle(*f.edit);

        CHECK(invalid.error.code == error_code::invalidParams);
        CHECK(unknown.error.code == error_code::unknownCommand);
        CHECK_FALSE(f.undoManager().canUndo());
        CHECK(f.redoNames() == std::vector<std::string>{setPropertyTitle});
    }

    TEST_CASE("an undoable command without an Edit gives no_edit and does not run its handler")
    {
        int calls = 0;
        auto counting = makeUndoable("test.counting", "Z\xC3\xA4hlen");
        counting.handler = [&calls](const Json&)
        {
            ++calls;
            return valueResult(1);
        };

        SUBCASE("EditContext without an Edit")
        {
            UndoFixture f;
            REQUIRE(f.registry.registerCommand(counting).ok);
            f.context.setEdit(nullptr);

            const auto result = f.registry.execute("test.counting", Json::object());
            CHECK_FALSE(result.ok);
            CHECK(result.error.code == error_code::noEdit);
            CHECK(calls == 0);
        }

        SUBCASE("no EditContext at all")
        {
            CommandRegistry registry;
            REQUIRE(registry.registerCommand(counting).ok);
            REQUIRE(registry.editContext() == nullptr);

            const auto result = registry.execute("test.counting", Json::object());
            CHECK_FALSE(result.ok);
            CHECK(result.error.code == error_code::noEdit);
            CHECK(calls == 0);
        }
    }

    TEST_CASE("a readOnly command runs without an Edit")
    {
        CommandRegistry registry;
        auto readOnly = tracklab_test::core_helpers::makeCommand("test.reading");
        readOnly.flags.readOnly = true;
        REQUIRE(registry.registerCommand(readOnly).ok);

        const auto result = registry.execute("test.reading", Json::object());
        CHECK(result.ok);
    }

    TEST_CASE("the registry uses the Edit of its EditContext at the time of the call")
    {
        UndoFixture f;
        auto other = tracklab::engine::createEdit(*f.engine);
        REQUIRE(other != nullptr);
        other->state.getOrCreateChildWithName(testNodeType, nullptr);
        settle(*other);
        other->getUndoManager().clearUndoHistory();

        f.context.setEdit(other.get());  // one registry, pointed at another project
        f.setProperty("alpha", 9);
        settle(*other);

        CHECK(static_cast<int>(testNode(*other).getProperty("alpha", -1)) == 9);
        CHECK(toStd(other->getUndoManager().getUndoDescriptions()) == std::vector<std::string>{setPropertyTitle});
        CHECK(f.undoNames().empty());
        CHECK(f.propertyValue("alpha") == -1);
        f.context.setEdit(nullptr);  // before `other` is destroyed
    }

    TEST_CASE("a handler that throws after it wrote leaves no trace: state restored, no undo step, redo kept")
    {
        UndoFixture f;
        registerFailingTestCommands(f.registry, f.context);
        f.setProperty("alpha", 1);
        REQUIRE(f.undoManager().undo());  // redo stack: [set_property]
        settle(*f.edit);
        const auto before = normalisedState(*f.edit);

        const auto result = f.registry.execute("test.fail_after_write", Json::object());
        settle(*f.edit);

        CHECK_FALSE(result.ok);
        CHECK(result.error.code == error_code::handlerFailed);
        CHECK(f.propertyValue("partial") == -1);
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames().empty());
        CHECK(f.redoNames() == std::vector<std::string>{setPropertyTitle});
    }

    TEST_CASE("a result that violates the result schema leaves no trace either")
    {
        UndoFixture f;
        registerFailingTestCommands(f.registry, f.context);
        f.setProperty("alpha", 1);
        settle(*f.edit);
        const auto before = normalisedState(*f.edit);

        const auto result = f.registry.execute("test.bad_result_after_write", Json::object());
        settle(*f.edit);

        CHECK_FALSE(result.ok);
        CHECK(result.error.code == error_code::invalidResult);
        CHECK(f.propertyValue("partial") == -1);
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames() == std::vector<std::string>{setPropertyTitle});  // only the earlier, successful command
    }

    TEST_CASE("a successful command after a failed one is a normal undo step")
    {
        UndoFixture f;
        registerFailingTestCommands(f.registry, f.context);
        (void)f.registry.execute("test.fail_after_write", Json::object());

        f.setProperty("alpha", 3);
        settle(*f.edit);

        CHECK(f.undoNames() == std::vector<std::string>{setPropertyTitle});
        CHECK(f.propertyValue("partial") == -1);
    }

    TEST_CASE("a new command clears the redo stack")
    {
        UndoFixture f;
        f.setProperty("alpha", 1);
        REQUIRE(f.undoManager().undo());
        REQUIRE(f.undoManager().canRedo());

        f.setProperty("beta", 2);
        settle(*f.edit);

        CHECK_FALSE(f.undoManager().canRedo());
    }
}
