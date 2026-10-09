// O-09 part C: detects a missing Tracklab JUCE patch (third_party/patches/juce-undomanager-stale-stash.patch).
// Pure JUCE, no Tracklab code: juce::UndoManager::undoCurrentTransactionOnly() appends a "stash" of former redo
// transactions to the redo stack. Unpatched JUCE 9.0.3 replaces the stash only when redo transactions exist at the
// time of perform(), and clearUndoHistory() does not empty it, so a stale stash comes back as a ghost redo.
// These tests are red on an unpatched JUCE and green with the patch; if one of them fails after a JUCE pin update,
// the patch is missing or no longer applies. The examples are the minimal ones of team/research/juce-undo-stash.
#include "test_support.h"

#include <juce_data_structures/juce_data_structures.h>

#include <string>

namespace
{

/** Trivial action: appends its letter to a shared log on perform, removes it again on undo. */
class LetterAction final : public juce::UndoableAction
{
public:
    LetterAction(std::string& logToChange, char letterToAppend) : log(logToChange), letter(letterToAppend) {}

    bool perform() override
    {
        log.push_back(letter);
        return true;
    }

    bool undo() override
    {
        log.pop_back();
        return true;
    }

private:
    std::string& log;
    char letter;
};

void step(juce::UndoManager& manager, std::string& log, char letter)
{
    manager.beginNewTransaction();
    REQUIRE(manager.perform(new LetterAction(log, letter)));
}

}  // namespace

TEST_SUITE("engine")
{
    TEST_CASE("missing Tracklab JUCE patch: a stale stash must not come back as redo after undoCurrentTransactionOnly")
    {
        std::string log;
        juce::UndoManager manager;

        step(manager, log, 'A');
        step(manager, log, 'B');
        REQUIRE(manager.undo());  // redo stack: [B]
        REQUIRE(log == "A");
        step(manager, log, 'C');  // B is discarded (moved to JUCE's stash), the redo stack is empty
        REQUIRE_FALSE(manager.canRedo());
        step(manager, log, 'D');  // nothing to stash: an unpatched JUCE keeps the old stash
        REQUIRE(log == "ACD");

        REQUIRE(manager.undoCurrentTransactionOnly());

        CHECK(log == "AC");
        CHECK_FALSE(manager.canRedo());  // unpatched JUCE: true, redo() would apply the discarded B after C
        CHECK(manager.getRedoDescriptions().isEmpty());
        CHECK_FALSE(manager.redo());
        CHECK(log == "AC");
    }

    TEST_CASE("missing Tracklab JUCE patch: clearUndoHistory() must not leave discarded redo steps in the stash")
    {
        std::string log;
        juce::UndoManager manager;

        step(manager, log, 'A');
        step(manager, log, 'B');
        REQUIRE(manager.undo());
        step(manager, log, 'C');  // B moves to the stash
        manager.clearUndoHistory();
        REQUIRE_FALSE(manager.canUndo());
        step(manager, log, 'D');
        REQUIRE(manager.canUndo());

        REQUIRE(manager.undoCurrentTransactionOnly());

        CHECK(log == "AC");
        CHECK_FALSE(manager.canRedo());  // unpatched JUCE: true, B comes back from before the clear
        CHECK_FALSE(manager.redo());
        CHECK(log == "AC");
    }

    TEST_CASE("a redo stack that exists when a transaction begins is still restored by undoCurrentTransactionOnly")
    {
        // Guards the other direction: the patch must not break the stash mechanism that Tracklab's rollback relies on.
        std::string log;
        juce::UndoManager manager;

        step(manager, log, 'A');
        step(manager, log, 'B');
        REQUIRE(manager.undo());  // redo stack: [B]
        REQUIRE(manager.canRedo());
        step(manager, log, 'C');  // B moves to the stash

        REQUIRE(manager.undoCurrentTransactionOnly());

        CHECK(log == "A");
        CHECK(manager.canRedo());
        CHECK(manager.redo());
        CHECK(log == "AB");
    }
}
