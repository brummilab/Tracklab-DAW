// O-09 part C: a rollback keeps the undo history (needs the Tracklab JUCE patch for the stale redo stash).
// Case "A, undo, B, then an undoable command fails at run time after it wrote": the failed transaction is taken back,
// the state is as before, redo is empty (A stays discarded), and B is still in the undo history and can be undone.
// Before the patch Transaction::rollback() answered this case by clearing the whole history; these tests are red then.
#include "core/transaction.h"
#include "core/undo_contract.h"
#include "core/undo_fixture.h"

#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::undo;
using namespace tracklab::core;

const std::string setPropertyTitle = "Eigenschaft \xC3\xA4ndern";

/** A, undo, B: A was discarded from the redo stack by B. Undo history afterwards: [B]. */
void makeDiscardedRedoStep(UndoFixture& f)
{
    f.setProperty("a", 1);
    settle(*f.edit);
    REQUIRE(f.undoManager().undo());
    settle(*f.edit);
    f.setProperty("b", 2);
    settle(*f.edit);
    REQUIRE(f.redoNames().empty());
    REQUIRE(f.undoNames() == std::vector<std::string>{setPropertyTitle});
}

/** The history after a rollback: unchanged state, only B in the undo history, no redo, and B can be taken back. */
void checkHistoryKept(UndoFixture& f, const std::string& stateAfterB)
{
    CHECK(normalisedState(*f.edit) == stateAfterB);
    CHECK(f.propertyValue("partial") == -1);
    CHECK(f.propertyValue("c") == -1);
    CHECK(f.undoNames() == std::vector<std::string>{setPropertyTitle});  // B is still there, nothing else
    CHECK(f.redoNames().empty());
    CHECK_FALSE(f.undoManager().canRedo());  // A stays discarded

    CHECK(f.undoManager().undo());  // B can be taken back
    settle(*f.edit);
    CHECK(f.propertyValue("b") == -1);
    CHECK(f.propertyValue("a") == -1);
    CHECK(f.undoNames().empty());
    CHECK(f.redoNames() == std::vector<std::string>{setPropertyTitle});  // and redone: exactly B, never A
    CHECK(f.undoManager().redo());
    settle(*f.edit);
    CHECK(f.propertyValue("b") == 2);
    CHECK(f.propertyValue("a") == -1);
    CHECK(normalisedState(*f.edit) == stateAfterB);
}

}  // namespace

TEST_SUITE("core")
{
    TEST_CASE("a command failing at run time after a write keeps the undo history after 'A, undo, B'")
    {
        UndoFixture f;
        registerFailingTestCommands(f.registry, f.context);
        makeDiscardedRedoStep(f);
        const auto afterB = normalisedState(*f.edit);

        const auto result = f.registry.execute("test.fail_after_write", Json::object());
        settle(*f.edit);

        REQUIRE_FALSE(result.ok);
        checkHistoryKept(f, afterB);
    }

    TEST_CASE("a command with a result violating its schema, after a write, keeps the undo history after 'A, undo, B'")
    {
        UndoFixture f;
        registerFailingTestCommands(f.registry, f.context);
        makeDiscardedRedoStep(f);
        const auto afterB = normalisedState(*f.edit);

        const auto result = f.registry.execute("test.bad_result_after_write", Json::object());
        settle(*f.edit);

        REQUIRE_FALSE(result.ok);
        checkHistoryKept(f, afterB);
    }

    TEST_CASE("a batch failing at run time (not up front) after writes keeps the undo history after 'A, undo, B'")
    {
        UndoFixture f;
        registerFailingTestCommands(f.registry, f.context);
        makeDiscardedRedoStep(f);
        const auto afterB = normalisedState(*f.edit);

        // Both steps are known and valid, so the up-front check passes; step 1 throws after step 0 and itself wrote.
        const auto result =
            f.registry.executeBatch("Beispiel", {BatchStep{"test.set_property", Json{{"name", "c"}, {"value", 3}}},
                                                 BatchStep{"test.fail_after_write", Json::object()}});
        settle(*f.edit);

        REQUIRE_FALSE(result.ok);
        CHECK(result.failedIndex == 1);
        checkHistoryKept(f, afterB);
    }

    TEST_CASE("Transaction::rollback() keeps the undo history after 'A, undo, B'")
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
        REQUIRE(f.undoNames() == std::vector<std::string>{"B"});
        const auto afterB = normalisedState(*f.edit);

        {
            Transaction c(*f.edit, "C");
            node.setProperty("c", 3, &um);
            c.rollback();
        }
        settle(*f.edit);

        CHECK(normalisedState(*f.edit) == afterB);
        CHECK(f.undoNames() == std::vector<std::string>{"B"});
        CHECK_FALSE(um.canRedo());
        CHECK(um.undo());  // B can be taken back
        settle(*f.edit);
        CHECK(f.propertyValue("b") == -1);
        CHECK(f.propertyValue("a") == -1);
        CHECK(f.redoNames() == std::vector<std::string>{"B"});
    }

    TEST_CASE("a rollback after clearUndoHistory() keeps what was done after the clear, and no stale redo step")
    {
        UndoFixture f;
        registerFailingTestCommands(f.registry, f.context);
        makeDiscardedRedoStep(f);  // JUCE's stash now holds A
        f.setProperty("c", 3);
        settle(*f.edit);
        REQUIRE(f.undoManager().undo());  // redo: [C]
        settle(*f.edit);
        f.setProperty("d", 4);  // C moves to the stash
        settle(*f.edit);
        f.undoManager().clearUndoHistory();
        f.undoManager().beginNewTransaction();
        f.setProperty("e", 5);  // new history: [E]
        settle(*f.edit);
        const auto afterE = normalisedState(*f.edit);

        const auto result = f.registry.execute("test.fail_after_write", Json::object());
        settle(*f.edit);

        REQUIRE_FALSE(result.ok);
        CHECK(normalisedState(*f.edit) == afterE);
        CHECK(f.undoNames() == std::vector<std::string>{setPropertyTitle});  // E is kept
        CHECK(f.redoNames().empty());
        CHECK_FALSE(f.undoManager().canRedo());
        CHECK(f.undoManager().undo());
        settle(*f.edit);
        CHECK(f.propertyValue("e") == -1);
        CHECK(f.propertyValue("d") == 4);
        CHECK(f.propertyValue("c") == -1);
    }
}
