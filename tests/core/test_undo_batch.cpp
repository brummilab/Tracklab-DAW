// CommandRegistry::executeBatch (M1-03): a list of commands (macro, Claude turn) is ONE undo transaction; an error in
// the middle takes back everything already done and reports the index of the failing step.
#include "core/undo_contract.h"
#include "core/undo_fixture.h"

#include <string>
#include <thread>
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

}  // namespace

TEST_SUITE("core")
{
    TEST_CASE("a batch of undoable commands is one undo step with the name of the batch")
    {
        UndoFixture f;
        const auto before = normalisedState(*f.edit);

        const auto result = f.registry.executeBatch(
            turnName, {setStep("a", 1), setStep("b", 2), BatchStep{"test.add_child", Json{{"type", "CHILD"}}}});
        settle(*f.edit);

        INFO(result.error.code << ": " << result.error.message << " (step " << result.failedIndex << ")");
        REQUIRE(result.ok);
        REQUIRE(result.results.size() == 3);
        CHECK(result.results[0] == Json{{"value", 1}});
        CHECK(result.results[1] == Json{{"value", 2}});
        CHECK(result.results[2] == Json{{"value", 1}});
        CHECK(f.propertyValue("a") == 1);
        CHECK(f.propertyValue("b") == 2);

        CHECK(f.undoNames() == std::vector<std::string>{turnName});
        CHECK(f.undoManager().undo());
        settle(*f.edit);
        CHECK(normalisedState(*f.edit) == before);
        CHECK_FALSE(f.undoManager().canUndo());
    }

    TEST_CASE("a batch step sees the changes of the steps before it, readOnly steps are allowed")
    {
        UndoFixture f;

        const auto result =
            f.registry.executeBatch(turnName, {setStep("x", 5), BatchStep{"test.read_value", Json{{"name", "x"}}}});

        REQUIRE(result.ok);
        REQUIRE(result.results.size() == 2);
        CHECK(result.results[1] == Json{{"value", 5}});
    }

    TEST_CASE("a batch that holds only readOnly steps creates no undo entry")
    {
        UndoFixture f;
        const auto result = f.registry.executeBatch(
            turnName, {BatchStep{"test.read_value", Json{{"name", "x"}}}, BatchStep{"test.nothing", Json::object()}});
        settle(*f.edit);

        REQUIRE(result.ok);
        CHECK(result.results.size() == 2);
        CHECK_FALSE(f.undoManager().canUndo());
    }

    TEST_CASE("an empty batch is ok, has no results and creates no undo entry")
    {
        UndoFixture f;
        const auto result = f.registry.executeBatch(turnName, {});
        settle(*f.edit);

        CHECK(result.ok);
        CHECK(result.results.empty());
        CHECK_FALSE(f.undoManager().canUndo());
    }

    TEST_CASE("a step that pauses longer than the 350 ms timer does not cut the batch")
    {
        UndoFixture f;
        const auto before = normalisedState(*f.edit);

        const auto result = f.registry.executeBatch(
            turnName, {setStep("a", 1), BatchStep{"test.set_two_with_pause", Json::object()}, setStep("c", 3)});
        settle(*f.edit);

        REQUIRE(result.ok);
        CHECK(f.undoNames() == std::vector<std::string>{turnName});
        CHECK(f.undoManager().undo());
        settle(*f.edit);
        CHECK(normalisedState(*f.edit) == before);
    }

    TEST_CASE("a macro command inside a batch joins the batch transaction")
    {
        UndoFixture f;
        const auto before = normalisedState(*f.edit);

        const auto result =
            f.registry.executeBatch(turnName, {setStep("a", 1), BatchStep{"test.macro", Json::object()}});
        settle(*f.edit);

        REQUIRE(result.ok);
        CHECK(f.undoNames() == std::vector<std::string>{turnName});
        CHECK(f.undoManager().undo());
        settle(*f.edit);
        CHECK(normalisedState(*f.edit) == before);
    }

    //==========================================================================
    // Atomicity
    TEST_CASE("a handler error in the middle takes back the earlier steps and reports the index")
    {
        UndoFixture f;
        registerFailingTestCommands(f.registry, f.context);
        const auto before = normalisedState(*f.edit);

        const auto result =
            f.registry.executeBatch(turnName, {setStep("a", 1), setStep("b", 2),
                                               BatchStep{"test.fail_after_write", Json::object()}, setStep("c", 3)});
        settle(*f.edit);

        CHECK_FALSE(result.ok);
        CHECK(result.failedIndex == 2);
        CHECK(result.error.code == error_code::handlerFailed);
        CHECK(result.error.message.find("failing on purpose") != std::string::npos);
        CHECK(f.propertyValue("a") == -1);
        CHECK(f.propertyValue("b") == -1);
        CHECK(f.propertyValue("partial") == -1);  // the failing step's own write is taken back too
        CHECK(f.propertyValue("c") == -1);        // later steps are not run
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames().empty());
        CHECK_FALSE(f.undoManager().canUndo());
    }

    TEST_CASE("invalid params of a step: invalid_params with the index, earlier steps taken back")
    {
        UndoFixture f;
        const auto before = normalisedState(*f.edit);

        const auto result = f.registry.executeBatch(
            turnName, {setStep("a", 1), BatchStep{"test.set_property", Json{{"name", "b"}}}, setStep("c", 3)});
        settle(*f.edit);

        CHECK_FALSE(result.ok);
        CHECK(result.failedIndex == 1);
        CHECK(result.error.code == error_code::invalidParams);
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames().empty());
    }

    TEST_CASE("an unknown command: unknown_command with its index, nothing changed")
    {
        UndoFixture f;
        const auto before = normalisedState(*f.edit);

        SUBCASE("first step")
        {
            const auto result =
                f.registry.executeBatch(turnName, {BatchStep{"test.nope", Json::object()}, setStep("a", 1)});
            CHECK_FALSE(result.ok);
            CHECK(result.failedIndex == 0);
            CHECK(result.error.code == error_code::unknownCommand);
        }

        SUBCASE("last step")
        {
            const auto result = f.registry.executeBatch(
                turnName, {setStep("a", 1), setStep("b", 2), BatchStep{"test.nope", Json::object()}});
            CHECK_FALSE(result.ok);
            CHECK(result.failedIndex == 2);
            CHECK(result.error.code == error_code::unknownCommand);
        }

        settle(*f.edit);
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames().empty());
    }

    TEST_CASE("a step result that violates its result schema fails the batch with invalid_result")
    {
        UndoFixture f;
        registerFailingTestCommands(f.registry, f.context);
        const auto before = normalisedState(*f.edit);

        const auto result = f.registry.executeBatch(
            turnName, {setStep("a", 1), BatchStep{"test.bad_result_after_write", Json::object()}});
        settle(*f.edit);

        CHECK_FALSE(result.ok);
        CHECK(result.failedIndex == 1);
        CHECK(result.error.code == error_code::invalidResult);
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames().empty());
    }

    TEST_CASE("a failing first step that already wrote is taken back")
    {
        UndoFixture f;
        registerFailingTestCommands(f.registry, f.context);
        const auto before = normalisedState(*f.edit);

        const auto result = f.registry.executeBatch(turnName, {BatchStep{"test.fail_after_write", Json::object()}});
        settle(*f.edit);

        CHECK_FALSE(result.ok);
        CHECK(result.failedIndex == 0);
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames().empty());
    }

    TEST_CASE("a failed batch leaves the undo steps before it and the redo stack as they were")
    {
        UndoFixture f;
        registerFailingTestCommands(f.registry, f.context);
        f.setProperty("kept", 1);
        f.setProperty("redo_me", 2);
        REQUIRE(f.undoManager().undo());  // undo: [set_property], redo: [set_property]
        settle(*f.edit);
        const auto before = normalisedState(*f.edit);

        const auto result =
            f.registry.executeBatch(turnName, {setStep("a", 1), BatchStep{"test.fail_after_write", Json::object()}});
        settle(*f.edit);

        CHECK_FALSE(result.ok);
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames() == std::vector<std::string>{setPropertyTitle});
        CHECK(f.redoNames() == std::vector<std::string>{setPropertyTitle});
        CHECK(f.undoManager().redo());
        settle(*f.edit);
        CHECK(f.propertyValue("redo_me") == 2);
    }

    TEST_CASE("the registry works normally after a failed batch")
    {
        UndoFixture f;
        registerFailingTestCommands(f.registry, f.context);
        (void)f.registry.executeBatch(turnName, {setStep("a", 1), BatchStep{"test.fail_after_write", Json::object()}});

        const auto result = f.registry.executeBatch(turnName, {setStep("b", 2)});
        settle(*f.edit);

        CHECK(result.ok);
        CHECK(f.undoNames() == std::vector<std::string>{turnName});
        CHECK(f.propertyValue("a") == -1);
        CHECK(f.propertyValue("b") == 2);
    }

    //==========================================================================
    // Preconditions
    TEST_CASE("a batch with an undoable step but no Edit gives no_edit and runs no handler")
    {
        int calls = 0;
        auto counting = makeUndoable("test.counting", "Z\xC3\xA4hlen");
        counting.handler = [&calls](const Json&)
        {
            ++calls;
            return valueResult(1);
        };
        CommandRegistry registry;  // no EditContext
        REQUIRE(registry.registerCommand(counting).ok);

        const auto result = registry.executeBatch(turnName, {BatchStep{"test.counting", Json::object()}});

        CHECK_FALSE(result.ok);
        CHECK(result.error.code == error_code::noEdit);
        CHECK(result.failedIndex == 0);
        CHECK(calls == 0);
    }

    TEST_CASE("executeBatch from another thread is refused with not_on_message_thread and runs no step")
    {
        UndoFixture f;
        BatchResult fromWorker;
        std::thread worker([&] { fromWorker = f.registry.executeBatch(turnName, {setStep("a", 1)}); });
        worker.join();

        CHECK_FALSE(fromWorker.ok);
        CHECK(fromWorker.error.code == error_code::notOnMessageThread);
        CHECK(f.propertyValue("a") == -1);
        CHECK_FALSE(f.undoManager().canUndo());
    }
}
