// core::CommandFailure (M1-03, lead decision 4): a handler reports an expected, domain-level failure with its own code,
// message and pointer; the registry passes them on 1:1 (not as handler_failed) and still rolls an undoable command back.
#include "core/undo_contract.h"
#include "core/undo_fixture.h"

#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::undo;
using namespace tracklab::core;

Command failingWith(const std::string& id, bool undoable)
{
    auto command = undoable ? makeUndoable(id, "Fachlicher Fehler") : tracklab_test::core_helpers::makeCommand(id);
    command.handler = [](const Json&) -> Json
    { throw CommandFailure("track_not_found", "no track with id 7", "/track_id"); };
    return command;
}

}  // namespace

TEST_SUITE("core")
{
    TEST_CASE("a CommandFailure of a handler becomes the error 1:1: code, message and pointer")
    {
        CommandRegistry registry;
        tracklab_test::core_helpers::registerOrFail(registry, failingWith("test.domain_failure", false));

        const auto result = registry.execute("test.domain_failure", Json::object());

        CHECK_FALSE(result.ok);
        CHECK(result.error.code == "track_not_found");
        CHECK(result.error.message == "no track with id 7");
        CHECK(result.error.pointer == "/track_id");
    }

    TEST_CASE("a CommandFailure without a pointer gives an empty pointer")
    {
        CommandRegistry registry;
        auto command = tracklab_test::core_helpers::makeCommand("test.no_pointer");
        command.handler = [](const Json&) -> Json { throw CommandFailure("project_locked", "the project is locked"); };
        tracklab_test::core_helpers::registerOrFail(registry, command);

        const auto result = registry.execute("test.no_pointer", Json::object());

        CHECK(result.error.code == "project_locked");
        CHECK(result.error.pointer.empty());
    }

    TEST_CASE("a CommandFailure is a std::exception with its message as what()")
    {
        const CommandFailure failure("x_failed", "text", "/p");
        const std::exception& base = failure;
        CHECK(std::string(base.what()) == "text");
        CHECK(failure.code() == "x_failed");
        CHECK(failure.pointer() == "/p");
    }

    TEST_CASE("any other exception is still handler_failed")
    {
        UndoFixture f;
        registerFailingTestCommands(f.registry, f.context);
        const auto result = f.registry.execute("test.fail_after_write", Json::object());
        CHECK(result.error.code == error_code::handlerFailed);
    }

    TEST_CASE("a CommandFailure of an undoable command rolls the command back like any failure")
    {
        UndoFixture f;
        auto command = makeUndoable("test.write_then_domain_failure", "Schreiben, dann Fachfehler");
        command.handler = [&f](const Json&) -> Json
        {
            testNode(*f.edit).setProperty("partial", 1, &f.undoManager());
            throw CommandFailure("track_not_found", "no track with id 7", "/track_id");
        };
        tracklab_test::core_helpers::registerOrFail(f.registry, command);
        f.setProperty("alpha", 1);
        REQUIRE(f.undoManager().undo());  // redo stack: [set_property]
        settle(*f.edit);
        const auto before = normalisedState(*f.edit);

        const auto result = f.registry.execute("test.write_then_domain_failure", Json::object());
        settle(*f.edit);

        CHECK(result.error.code == "track_not_found");
        CHECK(result.error.pointer == "/track_id");
        CHECK(f.propertyValue("partial") == -1);
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames().empty());
        CHECK(f.redoNames().size() == 1);
    }

    TEST_CASE("a CommandFailure in a batch step gives the code, pointer and index of that step")
    {
        UndoFixture f;
        tracklab_test::core_helpers::registerOrFail(f.registry, failingWith("test.domain_failure", true));
        const auto before = normalisedState(*f.edit);

        const auto result = f.registry.executeBatch("Claude: Fehler",
                                                    {BatchStep{"test.set_property", Json{{"name", "a"}, {"value", 1}}},
                                                     BatchStep{"test.domain_failure", Json::object()}});
        settle(*f.edit);

        CHECK_FALSE(result.ok);
        CHECK(result.failedIndex == 1);
        CHECK(result.error.code == "track_not_found");
        CHECK(result.error.pointer == "/track_id");
        CHECK(normalisedState(*f.edit) == before);
        CHECK(f.undoNames().empty());
    }

    TEST_CASE("no_edit is reported through CommandFailure by the edit commands: code, and a message that names it")
    {
        UndoFixture f;
        f.context.setEdit(nullptr);

        const auto result = f.registry.execute("edit.undo", Json::object());

        CHECK(result.error.code == error_code::noEdit);
        CHECK_FALSE(result.error.message.empty());
        CHECK(result.error.pointer.empty());
    }
}
