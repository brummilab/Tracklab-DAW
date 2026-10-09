// core::Transaction (M1-03): RAII around Edit::UndoTransactionInhibitor + UndoManager::beginNewTransaction(name).
// Pending messages are pumped (settle) and the message loop runs longer than Tracktion's 350 ms transaction timer.
#include "core/undo_contract.h"
#include "core/undo_fixture.h"

#include "core/transaction.h"

namespace
{

using namespace tracklab_test::undo;
using tracklab::core::Transaction;

constexpr int pauseLongerThanTimerMs = 800;  // Tracktion's UndoTransactionTimer fires 350 ms after a change message

}  // namespace

TEST_SUITE("core")
{
    TEST_CASE("a Transaction bundles several changes into one undo step with its name")
    {
        UndoFixture f(tracklab::engine::defaultUndoLevels, false);
        const auto before = normalisedState(*f.edit);
        auto& um = f.undoManager();
        auto node = testNode(*f.edit);
        const std::string name = "Mehrere \xC3\x84nderungen";  // UTF-8 umlaut: must not be mangled

        {
            Transaction transaction(*f.edit, name);
            node.setProperty("a", 1, &um);
            node.setProperty("b", 2, &um);
            node.setProperty("c", 3, &um);
            node.appendChild(juce::ValueTree("CHILD"), &um);
        }
        settle(*f.edit);

        REQUIRE(f.undoNames().size() == 1);
        CHECK(f.undoNames().front() == name);
        CHECK(um.undo());
        settle(*f.edit);
        CHECK(normalisedState(*f.edit) == before);
        CHECK_FALSE(um.canUndo());
    }

    TEST_CASE("two Transactions in a row are two undo steps, the newest is undone first")
    {
        UndoFixture f(tracklab::engine::defaultUndoLevels, false);
        auto& um = f.undoManager();
        auto node = testNode(*f.edit);
        {
            Transaction first(*f.edit, "Erster");
            node.setProperty("a", 1, &um);
        }
        {
            Transaction second(*f.edit, "Zweiter");
            node.setProperty("b", 2, &um);
        }
        CHECK(f.undoNames() == std::vector<std::string>{"Zweiter", "Erster"});
    }

    TEST_CASE("a Transaction without a change leaves no undo entry")
    {
        UndoFixture f(tracklab::engine::defaultUndoLevels, false);
        {
            Transaction transaction(*f.edit, "Nichts");
        }
        settle(*f.edit);
        CHECK_FALSE(f.undoManager().canUndo());
        CHECK(f.undoNames().empty());
    }

    TEST_CASE("the 350 ms timer of Tracktion does not cut a Transaction")
    {
        UndoFixture f(tracklab::engine::defaultUndoLevels, false);
        const auto before = normalisedState(*f.edit);
        auto& um = f.undoManager();
        auto node = testNode(*f.edit);

        {
            Transaction transaction(*f.edit, "Mit Pause");
            node.setProperty("first", 1, &um);
            settle(*f.edit);                          // the change message starts the timer ...
            pumpMessageLoop(pauseLongerThanTimerMs);  // ... which would fire in here
            node.setProperty("second", 2, &um);
        }
        settle(*f.edit);

        CHECK(f.undoNames() == std::vector<std::string>{"Mit Pause"});
        CHECK(um.undo());
        settle(*f.edit);
        CHECK(normalisedState(*f.edit) == before);
    }

    TEST_CASE("control: without a Transaction the timer of Tracktion does cut a pause of more than 350 ms")
    {
        // Proves that the test above can fail: the very same sequence with a plain beginNewTransaction().
        UndoFixture f(tracklab::engine::defaultUndoLevels, false);
        auto& um = f.undoManager();
        auto node = testNode(*f.edit);

        um.beginNewTransaction("Ohne Schutz");
        node.setProperty("first", 1, &um);
        settle(*f.edit);
        pumpMessageLoop(pauseLongerThanTimerMs);
        node.setProperty("second", 2, &um);
        settle(*f.edit);

        CHECK(f.undoNames().size() == 2);
    }

    TEST_CASE("the timer cuts again once the Transaction has ended (the inhibitor is released)")
    {
        UndoFixture f(tracklab::engine::defaultUndoLevels, false);
        auto& um = f.undoManager();
        auto node = testNode(*f.edit);

        {
            Transaction transaction(*f.edit, "Erster");
            node.setProperty("a", 1, &um);
        }
        settle(*f.edit);
        pumpMessageLoop(pauseLongerThanTimerMs);  // no inhibitor any more: the timer starts a new transaction
        node.setProperty("b", 2, &um);
        settle(*f.edit);

        const auto names = f.undoNames();
        REQUIRE(names.size() == 2);
        CHECK(names.back() == "Erster");
    }

    TEST_CASE("rollback() takes back all changes of the Transaction and leaves no undo entry")
    {
        UndoFixture f(tracklab::engine::defaultUndoLevels, false);
        const auto before = normalisedState(*f.edit);
        auto& um = f.undoManager();
        auto node = testNode(*f.edit);

        {
            Transaction transaction(*f.edit, "Wird zur\xC3\xBC"
                                             "ckgenommen");
            node.setProperty("a", 1, &um);
            node.appendChild(juce::ValueTree("CHILD"), &um);
            transaction.rollback();
        }
        settle(*f.edit);

        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames().empty());
        CHECK_FALSE(um.canUndo());
    }

    TEST_CASE("rollback() keeps the undo steps before the Transaction and the redo stack")
    {
        UndoFixture f(tracklab::engine::defaultUndoLevels, false);
        auto& um = f.undoManager();
        auto node = testNode(*f.edit);
        {
            Transaction t(*f.edit, "Bleibt");
            node.setProperty("keep", 1, &um);
        }
        {
            Transaction t(*f.edit, "Redo-Kandidat");
            node.setProperty("redo_me", 1, &um);
        }
        REQUIRE(um.undo());  // "Redo-Kandidat" is now on the redo stack
        settle(*f.edit);
        const auto mid = normalisedState(*f.edit);

        {
            Transaction t(*f.edit, "Verworfen");
            node.setProperty("dropped", 1, &um);
            t.rollback();
        }
        settle(*f.edit);

        CHECK(normalisedState(*f.edit) == mid);
        CHECK(f.undoNames() == std::vector<std::string>{"Bleibt"});
        CHECK(f.redoNames() == std::vector<std::string>{"Redo-Kandidat"});
    }

    TEST_CASE("rollback() of a Transaction without a change does nothing")
    {
        UndoFixture f(tracklab::engine::defaultUndoLevels, false);
        auto& um = f.undoManager();
        auto node = testNode(*f.edit);
        {
            Transaction t(*f.edit, "Bleibt");
            node.setProperty("keep", 1, &um);
        }
        const auto before = normalisedState(*f.edit);
        {
            Transaction t(*f.edit, "Leer");
            t.rollback();
        }
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames() == std::vector<std::string>{"Bleibt"});
    }

    TEST_CASE("a Transaction inside a Transaction joins the outer one: one step with the outer name")
    {
        UndoFixture f(tracklab::engine::defaultUndoLevels, false);
        const auto before = normalisedState(*f.edit);
        auto& um = f.undoManager();
        auto node = testNode(*f.edit);
        {
            Transaction outer(*f.edit, "Aussen");
            node.setProperty("a", 1, &um);
            {
                Transaction inner(*f.edit, "Innen");
                node.setProperty("b", 2, &um);
            }
            node.setProperty("c", 3, &um);  // after the inner one ended: still the outer step
        }
        settle(*f.edit);

        CHECK(f.undoNames() == std::vector<std::string>{"Aussen"});
        CHECK(um.undo());
        settle(*f.edit);
        CHECK(normalisedState(*f.edit) == before);
    }

    TEST_CASE("rollback() of a joined inner Transaction does nothing, the outer rollback takes everything back")
    {
        UndoFixture f(tracklab::engine::defaultUndoLevels, false);
        const auto before = normalisedState(*f.edit);
        auto& um = f.undoManager();
        auto node = testNode(*f.edit);
        {
            Transaction outer(*f.edit, "Aussen");
            node.setProperty("a", 1, &um);
            {
                Transaction inner(*f.edit, "Innen");
                node.setProperty("b", 2, &um);
                inner.rollback();
            }
            CHECK(f.propertyValue("a") == 1);
            CHECK(f.propertyValue("b") == 2);  // the inner rollback did not touch anything
            outer.rollback();
        }
        settle(*f.edit);

        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames().empty());
    }

    TEST_CASE("after the outer Transaction ended, a new Transaction is a step of its own again")
    {
        UndoFixture f(tracklab::engine::defaultUndoLevels, false);
        auto& um = f.undoManager();
        auto node = testNode(*f.edit);
        {
            Transaction outer(*f.edit, "Aussen");
            node.setProperty("a", 1, &um);
            Transaction inner(*f.edit, "Innen");
        }
        {
            Transaction next(*f.edit, "Danach");
            node.setProperty("b", 2, &um);
        }
        CHECK(f.undoNames() == std::vector<std::string>{"Danach", "Aussen"});
    }
}
