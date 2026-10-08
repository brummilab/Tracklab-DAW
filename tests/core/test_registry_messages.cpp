// Command registry (M1-02, lead decision 4): validation errors are written for Claude. The pointer names the
// offending field (also for a missing or an unknown property), the message says what was expected and what was
// received. These are the exact texts Claude will see, so they are pinned here.
#include "core/core_test_helpers.h"

#include <string>

namespace
{

using namespace tracklab::core;
using namespace tracklab_test::core_helpers;

CommandResult run(const Json& paramsSchema, const char* params)
{
    CommandRegistry registry;
    registerOrFail(registry, makeCommand("track.create", paramsSchema));
    return registry.execute("track.create", Json::parse(params));
}

CommandResult runRich(const char* params)
{
    return run(richParamsSchema(), params);
}

}  // namespace

TEST_SUITE("core")
{
    TEST_CASE("wrong type: expected type and received value")
    {
        const auto result = runRich(R"({"name": 5})");
        CHECK(result.error.pointer == "/name");
        CHECK(result.error.message == "expected string, got integer 5");

        CHECK(runRich(R"({"name": "a", "count": 1.5})").error.message == "expected integer, got number 1.5");
        CHECK(runRich(R"({"name": "a", "tags": ["x", 3]})").error.message == "expected string, got integer 3");
        CHECK(runRich(R"({"name": "a", "tags": "x"})").error.message == "expected array, got string \"x\"");
        CHECK(runRich(R"({"name": null})").error.message == "expected string, got null");
        CHECK(runRich(R"({"name": ["x"]})").error.message == "expected string, got array");
    }

    TEST_CASE("params that are not an object: expected object at the root")
    {
        CHECK(runRich("null").error.message == "expected object, got null");
        CHECK(runRich("[1]").error.message == "expected object, got array");
        CHECK(runRich("true").error.message == "expected object, got boolean true");
        CHECK(runRich("true").error.pointer.empty());
    }

    TEST_CASE("missing required property: pointer to the field, message with its type")
    {
        const auto result = runRich(R"({"gain_db": 1})");
        CHECK(result.error.pointer == "/name");
        CHECK(result.error.message == "missing required property 'name' (expected string)");
    }

    TEST_CASE("missing required property in a nested object points into that object")
    {
        const Json schema = Json::parse(R"({
            "type": "object",
            "properties": {"opts": {"type": "object", "properties": {"level": {"type": "integer"}},
                                    "required": ["level"], "additionalProperties": false}},
            "additionalProperties": false
        })");
        const auto result = run(schema, R"({"opts": {}})");
        CHECK(result.error.pointer == "/opts/level");
        CHECK(result.error.message == "missing required property 'level' (expected integer)");
    }

    TEST_CASE("unknown property: pointer to the field, message lists the allowed properties")
    {
        const auto result = runRich(R"({"name": "a", "bogus": 1})");
        CHECK(result.error.pointer == "/bogus");
        CHECK(result.error.message == "unknown property 'bogus'; allowed: count, gain_db, mode, name, opts, tags");

        const auto nested = runRich(R"({"name": "a", "opts": {"extra": 1}})");
        CHECK(nested.error.pointer == "/opts/extra");
        CHECK(nested.error.message == "unknown property 'extra'; allowed: level");
    }

    TEST_CASE("unknown property of a command without parameters says that it takes none")
    {
        const auto result = run(objectSchema(), R"({"x": 1})");
        CHECK(result.error.pointer == "/x");
        CHECK(result.error.message == "unknown property 'x'; this object takes no properties");
    }

    TEST_CASE("property names with '/' and '~' are escaped in the pointer (RFC 6901)")
    {
        const Json schema = objectSchema(Json::parse(R"({"a/b": {"type": "string"}, "c~d": {"type": "string"}})"),
                                         Json::array({"a/b"}));
        CHECK(run(schema, "{}").error.pointer == "/a~1b");
        CHECK(run(schema, R"({"a/b": 1})").error.pointer == "/a~1b");
        CHECK(run(schema, R"({"a/b": "x", "c~d": 1})").error.pointer == "/c~0d");
        CHECK(run(schema, R"({"a/b": "x", "e/f": 1})").error.pointer == "/e~1f");
    }

    TEST_CASE("enum violation lists the allowed values")
    {
        const auto result = runRich(R"({"name": "a", "mode": "surround"})");
        CHECK(result.error.pointer == "/mode");
        CHECK(result.error.message == R"(expected one of ["mono","stereo"], got string "surround")");
    }

    TEST_CASE("range violation names the limit and the received value")
    {
        CHECK(runRich(R"({"name": "a", "gain_db": 99})").error.message == "expected number <= 12, got 99");
        CHECK(runRich(R"({"name": "a", "gain_db": -99.5})").error.message == "expected number >= -60, got -99.5");
        CHECK(runRich(R"({"name": "a", "count": -1})").error.message == "expected integer >= 0, got -1");
        const auto nested = runRich(R"({"name": "a", "opts": {"level": 11}})");
        CHECK(nested.error.pointer == "/opts/level");
        CHECK(nested.error.message == "expected integer <= 10, got 11");
    }

    TEST_CASE("anyOf lists the alternatives; a $ref is followed to its schema")
    {
        const Json schema = Json::parse(R"({
            "type": "object",
            "properties": {"either": {"anyOf": [{"type": "string"}, {"$ref": "#/$defs/count"}]},
                           "pos": {"$ref": "#/$defs/count"}},
            "additionalProperties": false,
            "$defs": {"count": {"type": "integer", "minimum": 0}}
        })");
        const auto either = run(schema, R"({"either": true})");
        CHECK(either.error.pointer == "/either");
        CHECK(either.error.message == "expected string | integer, got boolean true");

        const auto pos = run(schema, R"({"pos": -3})");
        CHECK(pos.error.pointer == "/pos");
        CHECK(pos.error.message == "expected integer >= 0, got -3");
    }

    TEST_CASE("long values are shortened, without cutting a UTF-8 character in half")
    {
        std::string longText;
        for (int i = 0; i < 100; ++i)
            longText += "\xC3\xA4";  // 'a' with umlaut, 2 bytes each
        const auto result = run(objectSchema(Json::parse(R"({"x": {"type": "integer"}})")),
                                (R"({"x": ")" + longText + R"("})").c_str());
        const std::string& message = result.error.message;
        CHECK(message.rfind("expected integer, got string \"", 0) == 0);
        CHECK(message.size() < 100);
        CHECK(message.find("...") != std::string::npos);
        // The whole error must still serialise: nlohmann::json throws on invalid UTF-8.
        CHECK_NOTHROW(static_cast<void>(result.toJson().dump()));
    }

    TEST_CASE("result errors are written the same way as parameter errors")
    {
        CommandRegistry registry;
        Command command = makeCommand("track.create");
        command.handler = [](const Json&) { return Json{{"value", "seven"}}; };
        registerOrFail(registry, command);

        const auto result = registry.execute("track.create", Json::object());
        CHECK(result.error.code == error_code::invalidResult);
        CHECK(result.error.pointer == "/value");
        CHECK(result.error.message.find("expected integer, got string \"seven\"") != std::string::npos);
        CHECK(result.error.message.find("track.create") != std::string::npos);
    }
}
