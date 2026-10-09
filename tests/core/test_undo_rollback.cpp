// Rollback and the undo history (M1-03, review round 1).
// JUCE's UndoManager::undoCurrentTransactionOnly() appends a "stash" of former redo steps to the redo stack. The stash
// is only emptied when a step is performed while redo steps exist, so it can hold steps that were discarded long ago.
// A rollback must never bring such steps back; and edit.undo/edit.redo must not run inside an open Transaction.
#include "core/undo_contract.h"
#include "core/transaction.h"
#include "core/undo_fixture.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::undo;
using namespace tracklab::core;

const std::string setPropertyTitle = "Eigenschaft \xC3\xA4ndern";
const char* const undoInTransactionCode = "undo_in_transaction";

/** A, undo, B: A was discarded from the redo stack by B. Returns the normalised state after B. */
std::string makeDiscardedRedoStep(UndoFixture& f)
{
    f.setProperty("a", 1);
    settle(*f.edit);
    REQUIRE(f.undoManager().undo());
    settle(*f.edit);
    REQUIRE(f.redoNames() == std::vector<std::string>{setPropertyTitle});
    f.setProperty("b", 2);
    settle(*f.edit);
    REQUIRE(f.redoNames().empty());
    return normalisedState(*f.edit);
}

/** Undoable command that asks the registry for edit.undo / edit.redo from inside its own handler. */
void registerHistoryFromHandler(UndoFixture& f)
{
    using namespace tracklab_test::core_helpers;
    for (const char* which : {"undo", "redo"})
    {
        const std::string historyId = std::string("edit.") + which;
        Command c = makeUndoable(std::string("test.") + which + "_inside", "Schritt in Transaktion");
        c.handler = [&f, historyId](const Json&) -> Json
        {
            testNode(*f.edit).setProperty("before_history", 1, &f.edit->getUndoManager());
            const auto step = f.registry.execute(historyId, Json::object());
            if (!step.ok)
                throw CommandFailure(step.error.code, step.error.message, step.error.pointer);
            return valueResult(1);
        };
        registerOrFail(f.registry, c);
    }
}

}  // namespace

TEST_SUITE("core")
{
    TEST_CASE("a failing undoable command after 'A, undo, B' does not bring the discarded step A back")
    {
        UndoFixture f;
        registerFailingTestCommands(f.registry, f.context);
        const auto afterB = makeDiscardedRedoStep(f);

        const auto result = f.registry.execute("test.fail_after_write", Json::object());
        settle(*f.edit);

        CHECK_FALSE(result.ok);
        CHECK(f.redoNames().empty());
        CHECK_FALSE(f.undoManager().canRedo());
        CHECK(normalisedState(*f.edit) == afterB);
        CHECK(f.propertyValue("partial") == -1);
        CHECK(f.propertyValue("a") == -1);
        // The history stays consistent: redo must not be able to re-apply A on top of B.
        CHECK_FALSE(f.undoManager().redo());
        CHECK(normalisedState(*f.edit) == afterB);
    }

    TEST_CASE("a failing batch after 'A, undo, B' does not bring the discarded step A back")
    {
        UndoFixture f;
        const auto afterB = makeDiscardedRedoStep(f);

        const auto result = f.registry.executeBatch(
            "Beispiel", {BatchStep{"test.set_property", Json{{"name", "c"}, {"value", 3}}}, BatchStep{"test.missing", Json::object()}});
        settle(*f.edit);

        REQUIRE_FALSE(result.ok);
        CHECK(result.failedIndex == 1);
        CHECK(f.redoNames().empty());
        CHECK(normalisedState(*f.edit) == afterB);
        CHECK_FALSE(f.undoManager().redo());
        CHECK(normalisedState(*f.edit) == afterB);
    }

    TEST_CASE("a rollback after clearUndoHistory() does not bring steps back that were cleared")
    {
        UndoFixture f;
        registerFailingTestCommands(f.registry, f.context);
        makeDiscardedRedoStep(f);
        // Stale redo steps may sit in JUCE's stash; clearing the history does not touch them.
        f.setProperty("c", 3);
        settle(*f.edit);
        REQUIRE(f.undoManager().undo());
        settle(*f.edit);
        f.setProperty("d", 4);
        settle(*f.edit);
        f.undoManager().clearUndoHistory();
        f.undoManager().beginNewTransaction();
        const auto before = normalisedState(*f.edit);

        const auto result = f.registry.execute("test.fail_after_write", Json::object());
        settle(*f.edit);

        CHECK_FALSE(result.ok);
        CHECK(f.redoNames().empty());
        CHECK(normalisedState(*f.edit) == before);
        CHECK_FALSE(f.undoManager().redo());
        CHECK(normalisedState(*f.edit) == before);
    }

    TEST_CASE("Transaction::rollback() with a stale stash leaves no redo step")
    {
        UndoFixture f(tracklab::engine::defaultUndoLevels, false);
        auto& um = f.undoManager();
        auto node = testNode(*f.edit);
        {
            Transaction a(*f.edit, "A");
            node.setProperty("a", 1, &um);
        }
        REQUIRE(um.undo());
        {
            Transaction b(*f.edit, "B");
            node.setProperty("b", 2, &um);
        }
        settle(*f.edit);
        const auto afterB = normalisedState(*f.edit);
        {
            Transaction c(*f.edit, "C");
            node.setProperty("c", 3, &um);
            c.rollback();
        }
        settle(*f.edit);

        CHECK(normalisedState(*f.edit) == afterB);
        CHECK_FALSE(um.canRedo());
    }

    TEST_CASE("a rollback keeps the redo steps that existed when the Transaction began, and they still work")
    {
        UndoFixture f;
        registerFailingTestCommands(f.registry, f.context);
        f.setProperty("a", 1);
        settle(*f.edit);
        REQUIRE(f.undoManager().undo());
        settle(*f.edit);
        const auto before = normalisedState(*f.edit);

        const auto result = f.registry.execute("test.fail_after_write", Json::object());
        settle(*f.edit);

        CHECK_FALSE(result.ok);
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.redoNames() == std::vector<std::string>{setPropertyTitle});
        CHECK(f.undoManager().redo());
        settle(*f.edit);
        CHECK(f.propertyValue("a") == 1);
    }

    TEST_CASE("edit.undo in a batch with an undoable step gives undo_in_transaction and rolls the batch back")
    {
        UndoFixture f;
        f.setProperty("a", 1);
        settle(*f.edit);
        const auto before = normalisedState(*f.edit);
        const auto undoNamesBefore = f.undoNames();

        const auto result = f.registry.executeBatch(
            "Beispiel", {BatchStep{"test.set_property", Json{{"name", "b"}, {"value", 2}}}, BatchStep{"edit.undo", Json::object()}});
        settle(*f.edit);

        REQUIRE_FALSE(result.ok);
        CHECK(result.error.code == undoInTransactionCode);
        CHECK_FALSE(result.error.message.empty());
        CHECK(result.failedIndex == 1);
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames() == undoNamesBefore);
    }

    TEST_CASE("edit.redo in a batch with an undoable step gives undo_in_transaction")
    {
        UndoFixture f;
        f.setProperty("a", 1);
        settle(*f.edit);
        REQUIRE(f.undoManager().undo());
        settle(*f.edit);
        const auto before = normalisedState(*f.edit);

        const auto result = f.registry.executeBatch(
            "Beispiel", {BatchStep{"test.set_property", Json{{"name", "b"}, {"value", 2}}}, BatchStep{"edit.redo", Json::object()}});
        settle(*f.edit);

        REQUIRE_FALSE(result.ok);
        CHECK(result.error.code == undoInTransactionCode);
        CHECK(normalisedState(*f.edit) == before);
    }

    TEST_CASE("edit.undo and edit.redo from the handler of an undoable command give undo_in_transaction")
    {
        UndoFixture f;
        registerHistoryFromHandler(f);
        f.setProperty("a", 1);
        settle(*f.edit);
        const auto before = normalisedState(*f.edit);
        const auto undoNamesBefore = f.undoNames();

        for (const char* id : {"test.undo_inside", "test.redo_inside"})
        {
            const auto result = f.registry.execute(id, Json::object());
            settle(*f.edit);
            INFO(id);
            CHECK_FALSE(result.ok);
            CHECK(result.error.code == undoInTransactionCode);
            CHECK(normalisedState(*f.edit) == before);
            CHECK(f.undoNames() == undoNamesBefore);
        }
    }

    TEST_CASE("edit.undo and edit.redo still work after a Transaction has ended, and in a batch without undoable step")
    {
        UndoFixture f;
        f.setProperty("a", 1);
        settle(*f.edit);

        const auto batch = f.registry.executeBatch("Nur Abfrage", {BatchStep{"edit.undo", Json::object()}});
        REQUIRE(batch.ok);
        CHECK(batch.results[0]["done"] == true);
        CHECK(f.propertyValue("a") == -1);

        f.run("edit.redo");
        CHECK(f.propertyValue("a") == 1);
    }
}
