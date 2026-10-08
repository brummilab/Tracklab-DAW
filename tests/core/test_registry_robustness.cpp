// Command registry (M1-02, lead decision 7): a handler that throws, and flag combinations that make no sense.
#include "core/core_test_helpers.h"

#include <stdexcept>
#include <string>

namespace
{

using namespace tracklab::core;
using namespace tracklab_test::core_helpers;

}  // namespace

TEST_SUITE("core")
{
    TEST_CASE("a handler that throws a std::exception gives handler_failed with its message")
    {
        CommandRegistry registry;
        Command command = makeCommand("track.create");
        command.handler = [](const Json&) -> Json { throw std::runtime_error("disk on fire"); };
        registerOrFail(registry, command);

        const auto result = registry.execute("track.create", Json::object());
        CHECK_FALSE(result.ok);
        CHECK(result.error.code == error_code::handlerFailed);
        CHECK(result.error.message.find("disk on fire") != std::string::npos);
        CHECK(result.error.message.find("track.create") != std::string::npos);
        CHECK(result.error.pointer.empty());
        CHECK(result.toJson()["error"]["code"] == "handler_failed");
    }

    TEST_CASE("exceptions of the JSON library inside a handler are handled the same way")
    {
        CommandRegistry registry;
        Command command = makeCommand("track.create");
        command.handler = [](const Json& params) -> Json { return Json{{"value", params.at("missing").get<int>()}}; };
        registerOrFail(registry, command);

        const auto result = registry.execute("track.create", Json::object());
        CHECK_FALSE(result.ok);
        CHECK(result.error.code == error_code::handlerFailed);
        CHECK_FALSE(result.error.message.empty());
    }

    TEST_CASE("a handler that throws something else is also reported, not propagated")
    {
        CommandRegistry registry;
        Command command = makeCommand("track.create");
        command.handler = [](const Json&) -> Json { throw 42; };  // NOLINT: the point of this test
        registerOrFail(registry, command);

        const auto result = registry.execute("track.create", Json::object());
        CHECK_FALSE(result.ok);
        CHECK(result.error.code == error_code::handlerFailed);
        CHECK_FALSE(result.error.message.empty());
    }

    TEST_CASE("the registry stays usable after a failing handler")
    {
        CommandRegistry registry;
        bool fail = true;
        Command command = makeCommand("track.create");
        command.handler = [&fail](const Json&) -> Json
        {
            if (fail)
                throw std::logic_error("first call fails");
            return Json{{"value", 5}};
        };
        registerOrFail(registry, command);

        CHECK_FALSE(registry.execute("track.create", Json::object()).ok);
        fail = false;
        const auto second = registry.execute("track.create", Json::object());
        REQUIRE(second.ok);
        CHECK(second.result == Json{{"value", 5}});
    }

    //==========================================================================
    TEST_CASE("readOnly together with undoable or destructive is refused with invalid_flags")
    {
        for (const CommandFlags flags :
             {CommandFlags{.readOnly = true, .undoable = true}, CommandFlags{.readOnly = true, .destructive = true},
              CommandFlags{.readOnly = true, .undoable = true, .destructive = true}})
        {
            CommandRegistry registry;
            Command command = makeCommand("track.create");
            command.flags = flags;
            const auto outcome = registry.registerCommand(command);
            CHECK_FALSE(outcome.ok);
            CHECK(outcome.error.code == error_code::invalidFlags);
            CHECK(outcome.error.pointer == "/flags");
            CHECK_FALSE(outcome.error.message.empty());
            CHECK(registry.size() == 0);
            // Nothing stays behind: the id is still free.
            command.flags = CommandFlags{.undoable = true};
            CHECK(registry.registerCommand(command).ok);
        }
    }

    TEST_CASE("sensible flag combinations are accepted")
    {
        const CommandFlags combinations[] = {CommandFlags{},
                                             CommandFlags{.readOnly = true},
                                             CommandFlags{.readOnly = true, .longRunning = true},
                                             CommandFlags{.undoable = true},
                                             CommandFlags{.undoable = true, .destructive = true},
                                             CommandFlags{.destructive = true},
                                             CommandFlags{.undoable = true, .longRunning = true}};
        for (const auto& flags : combinations)
        {
            CommandRegistry registry;
            Command command = makeCommand("track.create");
            command.flags = flags;
            const auto outcome = registry.registerCommand(command);
            INFO(outcome.error.code << ": " << outcome.error.message);
            CHECK(outcome.ok);
        }
    }
}
