// Command registry (M1-02): execute() with validation of the parameters (all five kinds of error, with JSON pointer)
// and of the result, the structured error format and the message-thread rule.
#include "core/core_test_helpers.h"

#include <juce_events/juce_events.h>

#include <atomic>
#include <string>
#include <thread>

namespace
{

using namespace tracklab::core;
using namespace tracklab_test::core_helpers;

/** A registry with "track.create" (rich params, result {"value": integer}) that counts handler calls. */
struct Fixture
{
    Fixture()
    {
        Command command = makeCommand("track.create", richParamsSchema());
        command.handler = [this](const Json& params)
        {
            ++calls;
            lastParams = params;
            return nextResult;
        };
        registerOrFail(registry, command);
    }

    CommandResult run(const Json& params) const { return registry.execute("track.create", params); }

    CommandRegistry registry;
    mutable int calls = 0;
    mutable Json lastParams;
    Json nextResult = Json{{"value", 7}};
};

/** The five error cases of the mini-spike: params, expected pointer, a word the message has to contain. */
struct ParamCase
{
    const char* label;
    const char* params;
    const char* pointer;
    const char* messageHas;  // "" = any non-empty message
};

constexpr ParamCase kParamCases[] = {
    {"wrong type", R"({"name": 5})", "/name", ""},
    {"missing required property", R"({"gain_db": 1})", "", "name"},
    {"additional property", R"({"name": "a", "bogus": 1})", "", "bogus"},
    {"enum violation", R"({"name": "a", "mode": "surround"})", "/mode", ""},
    {"above maximum", R"({"name": "a", "gain_db": 99})", "/gain_db", ""},
    {"below minimum", R"({"name": "a", "gain_db": -99})", "/gain_db", ""},
    {"integer below minimum", R"({"name": "a", "count": -1})", "/count", ""},
    {"float where integer is required", R"({"name": "a", "count": 1.5})", "/count", ""},
    {"wrong type inside an array", R"({"name": "a", "tags": ["x", 3]})", "/tags/1", ""},
    {"wrong type in a nested object", R"({"name": "a", "opts": {"level": "high"}})", "/opts/level", ""},
    {"range violation in a nested object", R"({"name": "a", "opts": {"level": 11}})", "/opts/level", ""},
    {"additional property in a nested object", R"({"name": "a", "opts": {"extra": 1}})", "/opts", "extra"},
};

}  // namespace

TEST_SUITE("core")
{
    TEST_CASE("executing an unknown command returns unknown_command and runs nothing")
    {
        const Fixture f;
        const auto result = f.registry.execute("track.delete", Json::object());
        CHECK_FALSE(result.ok);
        CHECK(result.error.code == error_code::unknownCommand);
        CHECK(result.error.pointer.empty());
        CHECK(result.error.message.find("track.delete") != std::string::npos);
        CHECK(f.calls == 0);
    }

    TEST_CASE("valid params run the handler once with exactly these params and return its result")
    {
        const Fixture f;
        const Json params = Json::parse(R"({"name": "Gitarre", "gain_db": -6.5, "mode": "mono", "count": 2,
                                           "tags": ["a", "b"], "opts": {"level": 3}})");
        const auto result = f.run(params);
        INFO(result.error.code << ": " << result.error.message << " @ " << result.error.pointer);
        REQUIRE(result.ok);
        CHECK(result.result == Json{{"value", 7}});
        CHECK(f.calls == 1);
        CHECK(f.lastParams == params);
    }

    TEST_CASE("a number schema accepts integers, the bounds themselves are valid")
    {
        const Fixture f;
        CHECK(f.run(Json::parse(R"({"name": "a", "gain_db": 1})")).ok);
        CHECK(f.run(Json::parse(R"({"name": "a", "gain_db": 12})")).ok);
        CHECK(f.run(Json::parse(R"({"name": "a", "gain_db": -60})")).ok);
        CHECK(f.run(Json::parse(R"({"name": "a", "opts": {"level": 0}})")).ok);
        CHECK(f.run(Json::parse(R"({"name": "a", "opts": {"level": 10}})")).ok);
    }

    //==========================================================================
    TEST_CASE("invalid params: structured error with code, message and JSON pointer; the handler does not run")
    {
        for (const auto& c : kParamCases)
        {
            CAPTURE(c.label);
            const Fixture f;
            const auto result = f.run(Json::parse(c.params));
            CHECK_FALSE(result.ok);
            CHECK(result.error.code == error_code::invalidParams);
            CHECK(result.error.pointer == c.pointer);
            CHECK_FALSE(result.error.message.empty());
            if (std::string(c.messageHas) != "")
                CHECK(result.error.message.find(c.messageHas) != std::string::npos);
            CHECK(f.calls == 0);
        }
    }

    TEST_CASE("params that are not an object are refused at the root")
    {
        for (const char* text : {"null", "[]", "\"x\"", "5", "true"})
        {
            CAPTURE(text);
            const Fixture f;
            const auto result = f.run(Json::parse(text));
            CHECK_FALSE(result.ok);
            CHECK(result.error.code == error_code::invalidParams);
            CHECK(result.error.pointer.empty());
            CHECK(f.calls == 0);
        }
    }

    TEST_CASE("a command without parameters accepts {} and refuses any property")
    {
        CommandRegistry registry;
        registerOrFail(registry, makeCommand("app.ping"));  // params: empty object schema
        CHECK(registry.execute("app.ping", Json::object()).ok);

        const auto result = registry.execute("app.ping", Json{{"x", 1}});
        CHECK_FALSE(result.ok);
        CHECK(result.error.code == error_code::invalidParams);
        CHECK(result.error.message.find('x') != std::string::npos);
    }

    TEST_CASE("validation is deterministic: the same bad params give the same error")
    {
        const Fixture f;
        const Json params = Json::parse(R"({"name": 5, "gain_db": 99, "bogus": 1, "mode": "x"})");
        const auto first = f.run(params);
        const auto second = f.run(params);
        CHECK_FALSE(first.ok);
        CHECK(first.toJson() == second.toJson());
    }

    TEST_CASE("$ref/$defs and anyOf schemas are validated")
    {
        CommandRegistry registry;
        Json schema = Json::parse(R"({
            "type": "object",
            "properties": {
                "pos": {"$ref": "#/$defs/position"},
                "either": {"anyOf": [{"type": "string"}, {"type": "integer"}]}
            },
            "additionalProperties": false,
            "$defs": {"position": {"type": "integer", "minimum": 0}}
        })");
        registerOrFail(registry, makeCommand("transport.seek", schema));

        CHECK(registry.execute("transport.seek", Json::parse(R"({"pos": 5, "either": "x"})")).ok);
        CHECK(registry.execute("transport.seek", Json::parse(R"({"either": 3})")).ok);

        const auto badRef = registry.execute("transport.seek", Json::parse(R"({"pos": -1})"));
        CHECK_FALSE(badRef.ok);
        CHECK(badRef.error.code == error_code::invalidParams);
        CHECK(badRef.error.pointer == "/pos");

        const auto badAnyOf = registry.execute("transport.seek", Json::parse(R"({"either": true})"));
        CHECK_FALSE(badAnyOf.ok);
        CHECK(badAnyOf.error.code == error_code::invalidParams);
        CHECK(badAnyOf.error.pointer == "/either");
    }

    //==========================================================================
    TEST_CASE("the handler result is validated against resultSchema")
    {
        struct ResultCase
        {
            const char* label;
            const char* result;
            const char* pointer;
        };
        const ResultCase cases[] = {
            {"wrong type", R"({"value": "seven"})", "/value"},
            {"missing required property", R"({})", ""},
            {"additional property", R"({"value": 1, "debug": true})", ""},
            {"not an object", R"([1])", ""},
            {"null", "null", ""},
        };
        for (const auto& c : cases)
        {
            CAPTURE(c.label);
            Fixture f;
            f.nextResult = Json::parse(c.result);
            const auto result = f.run(Json{{"name", "a"}});
            CHECK(f.calls == 1);  // the handler ran, its result was refused
            CHECK_FALSE(result.ok);
            CHECK(result.error.code == error_code::invalidResult);
            CHECK(result.error.pointer == c.pointer);
            CHECK_FALSE(result.error.message.empty());
        }
    }

    TEST_CASE("a valid result passes through unchanged")
    {
        Fixture f;
        f.nextResult = Json{{"value", 123}};
        const auto result = f.run(Json{{"name", "a"}});
        REQUIRE(result.ok);
        CHECK(result.result == Json{{"value", 123}});
    }

    //==========================================================================
    TEST_CASE("an error serialises to {ok:false,error:{code,message,pointer}} with exactly these keys")
    {
        const Fixture f;
        const Json json = f.run(Json::parse(R"({"name": 5})")).toJson();
        REQUIRE(json.is_object());
        CHECK(json.size() == 2);
        CHECK(json["ok"] == false);
        REQUIRE(json["error"].is_object());
        CHECK(json["error"].size() == 3);
        CHECK(json["error"]["code"] == "invalid_params");
        CHECK(json["error"]["pointer"] == "/name");
        CHECK(json["error"]["message"].is_string());
        CHECK_FALSE(json["error"]["message"].get<std::string>().empty());
    }

    TEST_CASE("a success serialises to {ok:true,result:<result>}")
    {
        const Fixture f;
        const Json json = f.run(Json{{"name", "a"}}).toJson();
        CHECK(json == Json::parse(R"({"ok": true, "result": {"value": 7}})"));
    }

    TEST_CASE("CommandError serialises to code, message and pointer")
    {
        CommandError error;
        error.code = "invalid_params";
        error.message = "boom";
        error.pointer = "/a/b";
        CHECK(error.toJson() == Json::parse(R"({"code": "invalid_params", "message": "boom", "pointer": "/a/b"})"));
    }

    //==========================================================================
    TEST_CASE("execute() runs the handler on the message thread (the thread of the test runner)")
    {
        std::thread::id handlerThread;
        bool handlerOnMessageThread = false;
        CommandRegistry registry;
        Command command = makeCommand("app.ping");
        command.handler = [&](const Json&)
        {
            handlerThread = std::this_thread::get_id();
            handlerOnMessageThread = juce::MessageManager::getInstance()->isThisTheMessageThread();
            return Json{{"value", 1}};
        };
        registerOrFail(registry, command);

        REQUIRE(juce::MessageManager::getInstance()->isThisTheMessageThread());
        const auto result = registry.execute("app.ping", Json::object());
        CHECK(result.ok);
        CHECK(handlerThread == std::this_thread::get_id());
        CHECK(handlerOnMessageThread);
    }

    TEST_CASE("execute() from another thread is refused with not_on_message_thread and runs no handler")
    {
        std::atomic<int> calls{0};
        CommandRegistry registry;
        Command command = makeCommand("app.ping");
        command.handler = [&](const Json&)
        {
            ++calls;
            return Json{{"value", 1}};
        };
        registerOrFail(registry, command);

        CommandResult fromWorker;
        std::thread worker([&] { fromWorker = registry.execute("app.ping", Json::object()); });
        worker.join();

        CHECK_FALSE(fromWorker.ok);
        CHECK(fromWorker.error.code == error_code::notOnMessageThread);
        CHECK_FALSE(fromWorker.error.message.empty());
        CHECK(calls == 0);

        // The registry is still usable on the message thread afterwards.
        CHECK(registry.execute("app.ping", Json::object()).ok);
        CHECK(calls == 1);
    }

    TEST_CASE("the thread rule is checked first: an unknown command from another thread is a thread error")
    {
        CommandRegistry registry;
        CommandResult fromWorker;
        std::thread worker([&] { fromWorker = registry.execute("nothing.here", Json::object()); });
        worker.join();
        CHECK_FALSE(fromWorker.ok);
        CHECK(fromWorker.error.code == error_code::notOnMessageThread);
    }
}
