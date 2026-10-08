// Schema subset, stricter rules added by the implementer (M1-02, lead decision 6): "type" is a single string,
// "$ref" must resolve inside the schema, every "required" name must be declared in "properties".
#include "core/core_test_helpers.h"

#include "core/schema_subset.h"

#include <string>

namespace
{

using namespace tracklab::core;
using namespace tracklab_test::core_helpers;

Json parse(const char* text)
{
    return Json::parse(text);
}

}  // namespace

TEST_SUITE("core")
{
    TEST_CASE("type must be a single type name, not a list")
    {
        const Json schema = parse(R"({"type": "object", "additionalProperties": false,
                                      "properties": {"x": {"type": ["string", "null"]}}})");
        const auto violation = findSchemaSubsetViolation(schema);
        REQUIRE(violation.has_value());
        CHECK(violation->code == error_code::invalidSchema);
        CHECK(violation->pointer == "/properties/x/type");
        CHECK(violation->message.find("type") != std::string::npos);
    }

    TEST_CASE("an unknown type name is a violation")
    {
        const Json schema = parse(R"({"type": "object", "additionalProperties": false,
                                      "properties": {"x": {"type": "text"}}})");
        const auto violation = findSchemaSubsetViolation(schema);
        REQUIRE(violation.has_value());
        CHECK(violation->pointer == "/properties/x/type");
    }

    TEST_CASE("every JSON Schema type name is accepted")
    {
        for (const char* type : {"null", "boolean", "array", "number", "integer", "string"})
        {
            CAPTURE(type);
            Json schema = objectSchema(parse(R"({"x": {}})"));
            schema["properties"]["x"]["type"] = type;
            CHECK_FALSE(findSchemaSubsetViolation(schema).has_value());
        }
    }

    TEST_CASE("a $ref that points to nothing is a violation at the $ref")
    {
        for (const char* ref : {"#/$defs/missing", "#/properties/nope", "#/$defs/a/b"})
        {
            CAPTURE(ref);
            Json schema = objectSchema(parse(R"({"x": {}})"));
            schema["$defs"] = parse(R"({"a": {"type": "string"}})");
            schema["properties"]["x"]["$ref"] = ref;
            const auto violation = findSchemaSubsetViolation(schema);
            REQUIRE(violation.has_value());
            CHECK(violation->code == error_code::invalidSchema);
            CHECK(violation->pointer == "/properties/x/$ref");
            CHECK(violation->message.find(ref) != std::string::npos);
        }
    }

    TEST_CASE("a $ref to a place inside the schema is fine, also through a chain of references")
    {
        const Json schema = parse(R"({
            "type": "object",
            "properties": {"x": {"$ref": "#/$defs/first"}, "y": {"$ref": "#/properties/x"}},
            "additionalProperties": false,
            "$defs": {"first": {"$ref": "#/$defs/second"}, "second": {"type": "integer"}}
        })");
        const auto violation = findSchemaSubsetViolation(schema);
        INFO((violation ? violation->pointer + ": " + violation->message : std::string("none")));
        CHECK_FALSE(violation.has_value());
    }

    TEST_CASE("references that only point at each other are a violation")
    {
        const Json schema = parse(R"({
            "type": "object",
            "properties": {"x": {"$ref": "#/$defs/a"}},
            "additionalProperties": false,
            "$defs": {"a": {"$ref": "#/$defs/b"}, "b": {"$ref": "#/$defs/a"}}
        })");
        const auto violation = findSchemaSubsetViolation(schema);
        REQUIRE(violation.has_value());
        CHECK(violation->code == error_code::invalidSchema);
        CHECK(violation->message.find("cycle") != std::string::npos);
    }

    TEST_CASE("a $ref that is not a string or not a JSON pointer is a violation")
    {
        Json notString = objectSchema(parse(R"({"x": {"$ref": 5}})"));
        REQUIRE(findSchemaSubsetViolation(notString).has_value());
        CHECK(findSchemaSubsetViolation(notString)->pointer == "/properties/x/$ref");

        Json badPointer = objectSchema(parse(R"({"x": {"$ref": "#/bad~2escape"}})"));
        REQUIRE(findSchemaSubsetViolation(badPointer).has_value());
        CHECK(findSchemaSubsetViolation(badPointer)->pointer == "/properties/x/$ref");
    }

    TEST_CASE("required names must be declared in properties")
    {
        // Declared in properties: fine.
        CHECK_FALSE(findSchemaSubsetViolation(objectSchema(parse(R"({"a": {"type": "string"}})"), Json::array({"a"})))
                        .has_value());

        // Not declared: the violation points at the entry of "required".
        const auto missing =
            findSchemaSubsetViolation(objectSchema(parse(R"({"a": {"type": "string"}})"), Json::array({"a", "b"})));
        REQUIRE(missing.has_value());
        CHECK(missing->code == error_code::invalidSchema);
        CHECK(missing->pointer == "/required/1");
        CHECK(missing->message.find("'b'") != std::string::npos);

        // No properties at all.
        const auto noProperties =
            findSchemaSubsetViolation(parse(R"({"type": "object", "required": ["a"], "additionalProperties": false})"));
        REQUIRE(noProperties.has_value());
        CHECK(noProperties->pointer == "/required/0");
    }

    TEST_CASE("required in a nested object is checked against the properties of that object")
    {
        // "outer" declares "a", the nested object does not.
        const Json schema = parse(R"({
            "type": "object",
            "properties": {
                "a": {"type": "string"},
                "inner": {"type": "object", "properties": {}, "required": ["a"], "additionalProperties": false}
            },
            "additionalProperties": false
        })");
        const auto violation = findSchemaSubsetViolation(schema);
        REQUIRE(violation.has_value());
        CHECK(violation->pointer == "/properties/inner/required/0");
    }

    TEST_CASE("required must be an array of strings")
    {
        for (const char* required : {R"("a")", "[1]", "{}"})
        {
            CAPTURE(required);
            Json schema = objectSchema(parse(R"({"a": {"type": "string"}})"));
            schema["required"] = parse(required);
            const auto violation = findSchemaSubsetViolation(schema);
            REQUIRE(violation.has_value());
            CHECK(violation->pointer.rfind("/required", 0) == 0);
        }
    }

    TEST_CASE("registerCommand refuses the stricter violations with a pointer into the command")
    {
        struct Case
        {
            const char* label;
            const char* schema;
            const char* pointer;
        };
        const Case cases[] = {
            {"type list",
             R"({"type": "object", "additionalProperties": false, "properties": {"x": {"type": ["string", "null"]}}})",
             "/paramsSchema/properties/x/type"},
            {"hanging $ref",
             R"({"type": "object", "additionalProperties": false, "properties": {"x": {"$ref": "#/$defs/none"}}})",
             "/paramsSchema/properties/x/$ref"},
            {"undeclared required",
             R"({"type": "object", "additionalProperties": false, "properties": {}, "required": ["x"]})",
             "/paramsSchema/required/0"},
        };
        for (const auto& c : cases)
        {
            CAPTURE(c.label);
            CommandRegistry registry;
            const auto outcome = registry.registerCommand(makeCommand("track.create", parse(c.schema)));
            CHECK_FALSE(outcome.ok);
            CHECK(outcome.error.code == error_code::invalidSchema);
            CHECK(outcome.error.pointer == c.pointer);
            CHECK(registry.size() == 0);
        }
    }

    TEST_CASE("the same violation in resultSchema is reported below /resultSchema")
    {
        CommandRegistry registry;
        const auto outcome = registry.registerCommand(makeCommand(
            "track.create", objectSchema(),
            parse(
                R"({"type": "object", "additionalProperties": false, "properties": {"x": {"$ref": "#/$defs/none"}}})")));
        CHECK_FALSE(outcome.ok);
        CHECK(outcome.error.code == error_code::invalidSchema);
        CHECK(outcome.error.pointer == "/resultSchema/properties/x/$ref");
    }

    //==========================================================================
    TEST_CASE("keywords next to $ref are refused, because the validator would ignore them silently")
    {
        for (const char* sibling : {R"("maximum": 10)", R"("minimum": 0)", R"("type": "integer")", R"("enum": [1, 2])",
                                    R"("properties": {})", R"("items": {"type": "string"})"})
        {
            CAPTURE(sibling);
            const std::string text = std::string(R"({"type": "object", "additionalProperties": false,
                "properties": {"x": {"$ref": "#/$defs/n", )") +
                                     sibling + R"(}},
                "$defs": {"n": {"type": "integer"}}})";
            const auto violation = findSchemaSubsetViolation(Json::parse(text));
            REQUIRE(violation.has_value());
            CHECK(violation->code == error_code::invalidSchema);
            CHECK(violation->pointer.rfind("/properties/x/", 0) == 0);
            CHECK(violation->pointer != "/properties/x/$ref");
            CHECK(violation->message.find("$ref") != std::string::npos);
        }
    }

    TEST_CASE("description and default may stand next to $ref")
    {
        const Json schema = parse(R"({"type": "object", "additionalProperties": false,
            "properties": {"x": {"$ref": "#/$defs/n", "description": "A count", "default": 3}},
            "$defs": {"n": {"type": "integer"}}})");
        CHECK_FALSE(findSchemaSubsetViolation(schema).has_value());
    }

    TEST_CASE("registerCommand refuses a keyword next to $ref with invalid_schema")
    {
        CommandRegistry registry;
        const auto outcome = registry.registerCommand(
            makeCommand("track.create", parse(R"({"type": "object", "additionalProperties": false,
                "properties": {"x": {"$ref": "#/$defs/n", "maximum": 10}},
                "$defs": {"n": {"type": "integer"}}})")));
        CHECK_FALSE(outcome.ok);
        CHECK(outcome.error.code == error_code::invalidSchema);
        CHECK(outcome.error.pointer == "/paramsSchema/properties/x/maximum");
        CHECK(registry.size() == 0);
    }
}
