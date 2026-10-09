// CommandRegistry::executeBatch up-front check (O-09 part B): before the first step runs, every step is checked for a
// known command id and valid params (schema). A bad step k >= 1 refuses the whole batch: no handler runs, nothing is
// written, the undo history (including the redo stack) is untouched. This also holds after "A, undo, B", where JUCE's
// stash of former redo steps would be stale (the Tracklab JUCE patch empties it; a refused batch does not even need
// a rollback).
// Error addressing is the existing format: `failedIndex` names the step, `error.pointer` points into that step's params.
#include "core/undo_contract.h"
#include "core/undo_fixture.h"

#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::undo;
using namespace tracklab::core;

const std::string setPropertyTitle = "Eigenschaft \xC3\xA4ndern";
const std::string turnName = "Claude: Beispiel \xC3\xBC"
                             "bernehmen";

BatchStep setStep(const std::string& name, int value)
{
    return BatchStep{"test.set_property", Json{{"name", name}, {"value", value}}};
}

BatchStep unknownStep()
{
    return BatchStep{"test.nope", Json::object()};
}

/** test.set_property with a string where an integer is required: invalid_params at "/value". */
BatchStep badParamsStep()
{
    return BatchStep{"test.set_property", Json{{"name", "bad"}, {"value", "not a number"}}};
}

BatchStep countingWrite()
{
    return BatchStep{"test.counting_write", Json::object()};
}

BatchStep countingRead()
{
    return BatchStep{"test.counting_read", Json::object()};
}

/** Counts its calls and (undoable) writes through the UndoManager, so a call that slips through is visible twice. */
struct CountingCommands
{
    int writes = 0;
    int reads = 0;

    void registerIn(UndoFixture& f)
    {
        using namespace tracklab_test::core_helpers;
        {
            Command c = makeUndoable("test.counting_write", "Z\xC3\xA4hlen und schreiben");
            c.handler = [this, &f](const Json&)
            {
                ++writes;
                testNode(*f.edit).setProperty("counted", writes, &f.edit->getUndoManager());
                return valueResult(writes);
            };
            registerOrFail(f.registry, c);
        }
        {
            Command c = makeCommand("test.counting_read");
            c.flags.readOnly = true;
            c.handler = [this](const Json&)
            {
                ++reads;
                return valueResult(reads);
            };
            registerOrFail(f.registry, c);
        }
    }
};

/** A, undo, B: A was discarded from the redo stack by B (unpatched JUCE would keep a stale copy in its stash). */
void makeDiscardedRedoStep(UndoFixture& f)
{
    f.setProperty("a", 1);
    settle(*f.edit);
    REQUIRE(f.undoManager().undo());
    settle(*f.edit);
    f.setProperty("b", 2);
    settle(*f.edit);
    REQUIRE(f.redoNames().empty());
}

}  // namespace

TEST_SUITE("core")
{
    TEST_CASE("an unknown command in step k >= 1 refuses the batch before any step runs")
    {
        UndoFixture f;
        CountingCommands counter;
        counter.registerIn(f);
        const auto before = normalisedState(*f.edit);

        SUBCASE("step 1")
        {
            const auto result = f.registry.executeBatch(turnName, {countingWrite(), unknownStep(), setStep("a", 1)});
            CHECK_FALSE(result.ok);
            CHECK(result.failedIndex == 1);
            CHECK(result.error.code == error_code::unknownCommand);
            CHECK(result.results.empty());
        }

        SUBCASE("last step")
        {
            const auto result =
                f.registry.executeBatch(turnName, {countingWrite(), countingRead(), countingWrite(), unknownStep()});
            CHECK_FALSE(result.ok);
            CHECK(result.failedIndex == 3);
            CHECK(result.error.code == error_code::unknownCommand);
        }
        settle(*f.edit);

        CHECK(counter.writes == 0);  // the up-front check ran first: no handler was called
        CHECK(counter.reads == 0);
        CHECK(f.propertyValue("counted") == -1);
        CHECK(f.propertyValue("a") == -1);
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames().empty());
    }

    TEST_CASE("invalid params in step k >= 1 refuse the batch before any step runs, pointer into that step's params")
    {
        UndoFixture f;
        CountingCommands counter;
        counter.registerIn(f);
        const auto before = normalisedState(*f.edit);

        const auto result =
            f.registry.executeBatch(turnName, {countingWrite(), setStep("a", 1), badParamsStep(), setStep("c", 3)});
        settle(*f.edit);

        CHECK_FALSE(result.ok);
        CHECK(result.failedIndex == 2);
        CHECK(result.error.code == error_code::invalidParams);
        CHECK(result.error.pointer == "/value");
        CHECK(counter.writes == 0);
        CHECK(f.propertyValue("a") == -1);
        CHECK(f.propertyValue("c") == -1);
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames().empty());
    }

    TEST_CASE("the up-front check also holds for a batch whose earlier steps are readOnly (no transaction)")
    {
        UndoFixture f;
        CountingCommands counter;
        counter.registerIn(f);

        const auto result = f.registry.executeBatch(turnName, {countingRead(), countingRead(), unknownStep()});

        CHECK_FALSE(result.ok);
        CHECK(result.failedIndex == 2);
        CHECK(result.error.code == error_code::unknownCommand);
        CHECK(counter.reads == 0);
    }

    TEST_CASE("with several bad steps the first one is reported")
    {
        UndoFixture f;

        SUBCASE("invalid params before unknown command")
        {
            const auto result = f.registry.executeBatch(turnName, {setStep("a", 1), badParamsStep(), unknownStep()});
            CHECK(result.failedIndex == 1);
            CHECK(result.error.code == error_code::invalidParams);
        }

        SUBCASE("unknown command before invalid params")
        {
            const auto result = f.registry.executeBatch(turnName, {setStep("a", 1), unknownStep(), badParamsStep()});
            CHECK(result.failedIndex == 1);
            CHECK(result.error.code == error_code::unknownCommand);
        }
        settle(*f.edit);
        CHECK(f.propertyValue("a") == -1);
    }

    TEST_CASE("the up-front check reports the smallest step index across problem kinds: no_edit in step 0 beats an "
              "unknown command in step 1")
    {
        int calls = 0;
        auto counting = makeUndoable("test.counting", "Z\xC3\xA4hlen");
        counting.handler = [&calls](const Json&)
        {
            ++calls;
            return valueResult(1);
        };
        CommandRegistry registry;  // no EditContext, so an undoable step has no Edit
        tracklab_test::core_helpers::registerOrFail(registry, counting);

        SUBCASE("undoable step 0 without an Edit, unknown step 1")
        {
            const auto result =
                registry.executeBatch(turnName, {BatchStep{"test.counting", Json::object()}, unknownStep()});
            CHECK_FALSE(result.ok);
            CHECK(result.failedIndex == 0);
            CHECK(result.error.code == error_code::noEdit);
        }

        SUBCASE("unknown step 0 beats an undoable step 1 without an Edit")
        {
            const auto result =
                registry.executeBatch(turnName, {unknownStep(), BatchStep{"test.counting", Json::object()}});
            CHECK_FALSE(result.ok);
            CHECK(result.failedIndex == 0);
            CHECK(result.error.code == error_code::unknownCommand);
        }
        CHECK(calls == 0);
    }

    //==========================================================================
    // History
    TEST_CASE("after 'A, undo, B' a batch refused up front loses no undo history and brings no stale redo back")
    {
        UndoFixture f;
        makeDiscardedRedoStep(f);
        const auto afterB = normalisedState(*f.edit);
        REQUIRE(f.undoNames() == std::vector<std::string>{setPropertyTitle});

        SUBCASE("unknown command in step 1")
        {
            const auto result = f.registry.executeBatch(turnName, {setStep("c", 3), unknownStep()});
            REQUIRE_FALSE(result.ok);
            CHECK(result.failedIndex == 1);
        }

        SUBCASE("invalid params in step 1")
        {
            const auto result = f.registry.executeBatch(turnName, {setStep("c", 3), badParamsStep()});
            REQUIRE_FALSE(result.ok);
            CHECK(result.failedIndex == 1);
        }
        settle(*f.edit);

        CHECK(normalisedState(*f.edit) == afterB);
        CHECK(f.undoNames() == std::vector<std::string>{setPropertyTitle});  // B is still there
        CHECK(f.redoNames().empty());
        CHECK_FALSE(f.undoManager().canRedo());  // A stays discarded
        CHECK(f.undoManager().undo());           // B can still be taken back
        settle(*f.edit);
        CHECK(f.propertyValue("b") == -1);
        CHECK(f.propertyValue("a") == -1);
    }

    TEST_CASE("a batch refused up front leaves a pending redo stack exactly as it was")
    {
        UndoFixture f;
        f.setProperty("kept", 1);
        f.setProperty("redo_me", 2);
        REQUIRE(f.undoManager().undo());
        settle(*f.edit);
        const auto before = normalisedState(*f.edit);

        const auto result = f.registry.executeBatch(turnName, {setStep("a", 1), unknownStep()});
        settle(*f.edit);

        REQUIRE_FALSE(result.ok);
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames() == std::vector<std::string>{setPropertyTitle});
        CHECK(f.redoNames() == std::vector<std::string>{setPropertyTitle});
        CHECK(f.undoManager().redo());
        settle(*f.edit);
        CHECK(f.propertyValue("redo_me") == 2);
    }

    TEST_CASE("a batch refused up front is not a transaction: the next command starts a clean undo step")
    {
        UndoFixture f;
        (void)f.registry.executeBatch(turnName, {setStep("a", 1), unknownStep()});

        f.setProperty("b", 2);
        settle(*f.edit);

        CHECK(f.undoNames() == std::vector<std::string>{setPropertyTitle});
        CHECK(f.propertyValue("a") == -1);
        CHECK(f.propertyValue("b") == 2);
    }

    //==========================================================================
    // Runtime errors (after a successful up-front check) behave as before: rollback
    TEST_CASE("a handler error at run time still runs the earlier steps and then takes everything back")
    {
        UndoFixture f;
        registerFailingTestCommands(f.registry, f.context);
        CountingCommands counter;
        counter.registerIn(f);
        f.setProperty("kept", 1);
        settle(*f.edit);
        const auto before = normalisedState(*f.edit);

        const auto result =
            f.registry.executeBatch(turnName, {countingWrite(), setStep("a", 1),
                                               BatchStep{"test.fail_after_write", Json::object()}, countingWrite()});
        settle(*f.edit);

        CHECK_FALSE(result.ok);
        CHECK(result.failedIndex == 2);
        CHECK(result.error.code == error_code::handlerFailed);
        CHECK(result.results.empty());
        CHECK(counter.writes == 1);  // step 0 ran (the check passed), step 3 did not
        CHECK(f.propertyValue("counted") == -1);
        CHECK(f.propertyValue("a") == -1);
        CHECK(f.propertyValue("partial") == -1);
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames() == std::vector<std::string>{setPropertyTitle});  // only the earlier command
    }

    TEST_CASE("a result that violates its schema at run time still takes the batch back (invalid_result)")
    {
        UndoFixture f;
        registerFailingTestCommands(f.registry, f.context);
        const auto before = normalisedState(*f.edit);

        const auto result = f.registry.executeBatch(
            turnName, {setStep("a", 1), BatchStep{"test.bad_result_after_write", Json::object()}, setStep("c", 3)});
        settle(*f.edit);

        CHECK_FALSE(result.ok);
        CHECK(result.failedIndex == 1);
        CHECK(result.error.code == error_code::invalidResult);
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames().empty());
    }

    TEST_CASE("a valid batch is unchanged by the up-front check: all results in order, one undo step")
    {
        UndoFixture f;
        CountingCommands counter;
        counter.registerIn(f);

        const auto result = f.registry.executeBatch(turnName, {countingWrite(), setStep("a", 1), countingRead()});
        settle(*f.edit);

        REQUIRE(result.ok);
        REQUIRE(result.results.size() == 3);
        CHECK(result.results[0] == Json{{"value", 1}});
        CHECK(result.results[1] == Json{{"value", 1}});
        CHECK(result.results[2] == Json{{"value", 1}});
        CHECK(f.undoNames() == std::vector<std::string>{turnName});
    }
}
