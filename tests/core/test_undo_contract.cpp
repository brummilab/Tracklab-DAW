// Undo/redo contract for ALL undoable commands (M1-03, helper: undo_contract.h) and its negative tests.
//
// Later cards: every new undoable command is registered in builtInRegistry() below and gets its samples in
// builtInSamples(). A built-in undoable command without a sample turns "every built-in undoable command honours the
// contract" red.
#include "core/undo_contract.h"
#include "core/undo_fixture.h"

#include "core/app_commands.h"

#include <string>

namespace
{

using namespace tracklab_test::undo;
using namespace tracklab::core;

/** The commands of the app that touch the project: what the app registers (src/core/register*Commands). */
void registerBuiltInCommands(CommandRegistry& registry, EditContext& context)
{
    REQUIRE(registerAppCommands(registry, "0.0.0").ok);
    REQUIRE(registerEditCommands(registry, context).ok);
}

/** Samples of the built-in undoable commands. None yet: edit.undo/redo manipulate the history, app.version reads. */
Samples builtInSamples()
{
    return {};
}

bool mentions(const Report& report, const std::string& part)
{
    for (const auto& problem : report.problems)
        if (problem.find(part) != std::string::npos)
            return true;
    return false;
}

}  // namespace

TEST_SUITE("core")
{
    //==========================================================================
    // The normaliser
    TEST_CASE("the normaliser ignores volatile properties but sees every real change")
    {
        UndoFixture f(tracklab::engine::defaultUndoLevels, false);
        const auto base = normalisedState(*f.edit);

        f.edit->state.setProperty("modifiedBy", "Someone Else", nullptr);
        f.edit->state.setProperty("lastSignificantChange", "ffff", nullptr);
        testNode(*f.edit).setProperty("modifiedBy", "nested", nullptr);
        CHECK(normalisedState(*f.edit) == base);

        testNode(*f.edit).setProperty("real", 1, nullptr);
        CHECK(normalisedState(*f.edit) != base);
    }

    TEST_CASE("the normaliser works on a copy: the Edit keeps its volatile properties")
    {
        UndoFixture f(tracklab::engine::defaultUndoLevels, false);
        f.edit->state.setProperty("modifiedBy", "Beispiel", nullptr);

        (void)normalisedState(*f.edit);

        CHECK(f.edit->state.getProperty("modifiedBy").toString() == "Beispiel");
    }

    //==========================================================================
    // The contract
    TEST_CASE("every registered undoable command honours the undo/redo contract")
    {
        UndoFixture f;  // edit commands + the well-behaved test commands
        const auto samples = goodTestSamples();

        int undoable = 0;
        for (const Command* command : f.registry.list())
            undoable += command->flags.undoable ? 1 : 0;
        REQUIRE(undoable >= 5);  // the check is not vacuous

        const auto report = checkAllUndoableCommands(f.registry, *f.edit, samples);
        requireContract(report);
    }

    TEST_CASE("every undoable built-in command of the app has contract samples and honours the contract")
    {
        UndoFixture f(tracklab::engine::defaultUndoLevels, false);
        CommandRegistry registry;
        registry.setEditContext(&f.context);
        registerBuiltInCommands(registry, f.context);

        const auto report = checkAllUndoableCommands(registry, *f.edit, builtInSamples());
        requireContract(report);
    }

    //==========================================================================
    // Negative tests: the contract must catch violations
    TEST_CASE("an undoable command that writes past the UndoManager breaks the contract")
    {
        UndoFixture f(tracklab::engine::defaultUndoLevels, false);
        registerMisbehavingTestCommands(f.registry, f.context);

        Report report;
        checkCommand(report, f.registry, *f.edit, "test.write_past_undo", Sample{Json::object(), {}});

        CHECK_FALSE(report.ok());
        CHECK(mentions(report, "test.write_past_undo"));
    }

    TEST_CASE("an undoable command that writes only partly through the UndoManager breaks the contract")
    {
        UndoFixture f(tracklab::engine::defaultUndoLevels, false);
        registerMisbehavingTestCommands(f.registry, f.context);

        Report report;
        checkCommand(report, f.registry, *f.edit, "test.write_half_past_undo", Sample{Json::object(), {}});

        CHECK_FALSE(report.ok());
        CHECK(mentions(report, "did not restore the state"));
    }

    TEST_CASE("the generic check over a registry with a misbehaving command is red")
    {
        UndoFixture f;
        registerMisbehavingTestCommands(f.registry, f.context);
        auto samples = goodTestSamples();
        samples["test.write_past_undo"] = {Sample{Json::object(), {}}};
        samples["test.write_half_past_undo"] = {Sample{Json::object(), {}}};

        const auto report = checkAllUndoableCommands(f.registry, *f.edit, samples);

        CHECK_FALSE(report.ok());
        CHECK(mentions(report, "test.write_past_undo"));
        CHECK(mentions(report, "test.write_half_past_undo"));
        CHECK_FALSE(mentions(report, "test.set_property"));  // the well-behaved commands are not blamed
    }

    TEST_CASE("an undoable command without a sample is reported, so that no command slips past the contract")
    {
        UndoFixture f;
        auto samples = goodTestSamples();
        samples.erase("test.add_child");

        const auto report = checkAllUndoableCommands(f.registry, *f.edit, samples);

        CHECK_FALSE(report.ok());
        CHECK(mentions(report, "test.add_child: undoable command without a contract sample"));
    }

    TEST_CASE("a sample that changes nothing is reported")
    {
        UndoFixture f;

        Report report;
        checkCommand(report, f.registry, *f.edit, "test.set_property",
                     Sample{Json{{"name", "same"}, {"value", 4}},
                            [](te::Edit& e) { testNode(e).setProperty("same", 4, nullptr); }});

        CHECK_FALSE(report.ok());
        CHECK(mentions(report, "changed nothing"));
    }

    TEST_CASE("a sample for a command that is not registered is reported")
    {
        UndoFixture f;
        auto samples = goodTestSamples();
        samples["test.gone"] = {Sample{Json::object(), {}}};

        const auto report = checkAllUndoableCommands(f.registry, *f.edit, samples);

        CHECK_FALSE(report.ok());
        CHECK(mentions(report, "test.gone"));
    }
}
