// Schema subset rule (M1-02): command schemas may only use
//   type, properties, required, additionalProperties (false), enum, minimum, maximum, items, anyOf, $ref, $defs,
//   description, default
// and every object schema needs "additionalProperties": false. Checked by findSchemaSubsetViolation() and enforced by
// CommandRegistry::registerCommand() for paramsSchema and resultSchema.
#include "core/core_test_helpers.h"

#include "core/schema_subset.h"

#include <string>
#include <utility>
#include <vector>

namespace
{

using namespace tracklab::core;
using namespace tracklab_test::core_helpers;

Json parse(const char* text)
{
    return Json::parse(text);
}

/** A params schema that uses every allowed keyword once. */
Json fullSubsetSchema()
{
    return parse(R"({
        "type": "object",
        "description": "Everything that is allowed",
        "properties": {
            "text": {"type": "string", "description": "A text", "default": "x"},
            "num": {"type": "number", "minimum": 0, "maximum": 1},
            "kind": {"type": "string", "enum": ["a", "b"]},
            "list": {"type": "array", "items": {"type": "object", "properties": {}, "additionalProperties": false}},
            "pick": {"anyOf": [{"type": "string"}, {"type": "null"}]},
            "ref": {"$ref": "#/$defs/thing"}
        },
        "required": ["text"],
        "additionalProperties": false,
        "$defs": {
            "thing": {"type": "object", "properties": {"n": {"type": "integer"}}, "additionalProperties": false}
        }
    })");
}

/** Schema with the extra keyword inside "properties.x" (wrapped into a valid object schema). */
Json withPropertyKeyword(const std::string& keyword, const Json& value)
{
    Json schema = objectSchema(Json::parse(R"({"x": {"type": "string"}})"));
    schema["properties"]["x"][keyword] = value;
    return schema;
}

}  // namespace

TEST_SUITE("core")
{
    TEST_CASE("a schema that uses every allowed keyword has no violation")
    {
        const auto violation = findSchemaSubsetViolation(fullSubsetSchema());
        CHECK_FALSE(violation.has_value());
    }

    TEST_CASE("an object without parameters is a valid schema")
    {
        CHECK_FALSE(findSchemaSubsetViolation(objectSchema()).has_value());
    }

    TEST_CASE("every keyword outside the subset is a violation, reported with its JSON pointer")
    {
        const std::pair<const char*, const char*> unsupported[] = {
            {"pattern", R"("^a")"},
            {"format", R"("date-time")"},
            {"minLength", "1"},
            {"maxLength", "9"},
            {"minItems", "1"},
            {"maxItems", "3"},
            {"uniqueItems", "true"},
            {"exclusiveMinimum", "0"},
            {"exclusiveMaximum", "1"},
            {"multipleOf", "2"},
            {"const", R"("a")"},
            {"oneOf", R"([{"type": "string"}])"},
            {"allOf", R"([{"type": "string"}])"},
            {"not", R"({"type": "string"})"},
            {"if", R"({"type": "string"})"},
            {"then", R"({"type": "string"})"},
            {"else", R"({"type": "string"})"},
            {"patternProperties", R"({})"},
            {"propertyNames", R"({"type": "string"})"},
            {"minProperties", "1"},
            {"maxProperties", "1"},
            {"contains", R"({"type": "string"})"},
            {"additionalItems", "false"},
            {"dependencies", R"({})"},
            {"definitions", R"({})"},
            {"title", R"("T")"},
            {"examples", R"(["a"])"},
            {"$id", R"("urn:x")"},
            {"$schema", R"("http://json-schema.org/draft-07/schema#")"},
            {"$comment", R"("c")"},
            {"readOnly", "true"},
        };
        for (const auto& [keyword, value] : unsupported)
        {
            CAPTURE(keyword);
            const auto violation = findSchemaSubsetViolation(withPropertyKeyword(keyword, parse(value)));
            REQUIRE(violation.has_value());
            CHECK(violation->code == error_code::invalidSchema);
            CHECK(violation->pointer == std::string("/properties/x/") + keyword);
            CHECK(violation->message.find(keyword) != std::string::npos);
        }
    }

    TEST_CASE("unsupported keywords are found at the root and in every kind of sub-schema")
    {
        // Root
        Json root = objectSchema();
        root["pattern"] = "x";
        REQUIRE(findSchemaSubsetViolation(root).has_value());
        CHECK(findSchemaSubsetViolation(root)->pointer == "/pattern");

        // items
        Json items =
            objectSchema(Json::parse(R"({"list": {"type": "array", "items": {"type": "string", "format": "uri"}}})"));

        REQUIRE(findSchemaSubsetViolation(items).has_value());
        CHECK(findSchemaSubsetViolation(items)->pointer == "/properties/list/items/format");

        // anyOf
        Json anyOf =
            objectSchema(Json::parse(R"({"x": {"anyOf": [{"type": "string"}, {"type": "string", "minLength": 1}]}})"));

        REQUIRE(findSchemaSubsetViolation(anyOf).has_value());
        CHECK(findSchemaSubsetViolation(anyOf)->pointer == "/properties/x/anyOf/1/minLength");

        // $defs
        Json defs = objectSchema();
        defs["$defs"] = Json::parse(R"({"thing": {"type": "string", "pattern": "a"}})");
        REQUIRE(findSchemaSubsetViolation(defs).has_value());
        CHECK(findSchemaSubsetViolation(defs)->pointer == "/$defs/thing/pattern");

        // nested object
        Json nested = objectSchema();
        nested["properties"]["opts"] = objectSchema(Json::parse(R"({"y": {"type": "string", "format": "x"}})"));
        REQUIRE(findSchemaSubsetViolation(nested).has_value());
        CHECK(findSchemaSubsetViolation(nested)->pointer == "/properties/opts/properties/y/format");
    }

    TEST_CASE("an object schema without additionalProperties:false is a violation")
    {
        struct Case
        {
            const char* label;
            const char* schema;
            const char* pointer;
        };
        const Case cases[] = {
            {"root", R"({"type": "object", "properties": {}})", ""},
            {"nested property", R"({"type": "object", "additionalProperties": false, "properties":
                {"opts": {"type": "object", "properties": {}}}})",
             "/properties/opts"},
            {"object in items", R"({"type": "object", "additionalProperties": false, "properties":
                {"list": {"type": "array", "items": {"type": "object", "properties": {}}}}})",
             "/properties/list/items"},
            {"object in anyOf", R"({"type": "object", "additionalProperties": false, "properties":
                {"x": {"anyOf": [{"type": "string"}, {"type": "object", "properties": {}}]}}})",
             "/properties/x/anyOf/1"},
            {"object in $defs", R"({"type": "object", "additionalProperties": false, "properties": {},
                "$defs": {"thing": {"type": "object", "properties": {}}}})",
             "/$defs/thing"},
        };
        for (const auto& c : cases)
        {
            CAPTURE(c.label);
            const auto violation = findSchemaSubsetViolation(parse(c.schema));
            REQUIRE(violation.has_value());
            CHECK(violation->code == error_code::invalidSchema);
            CHECK(violation->pointer == c.pointer);
            CHECK(violation->message.find("additionalProperties") != std::string::npos);
        }
    }

    TEST_CASE("additionalProperties must be exactly false")
    {
        for (const char* value : {"true", "{}", R"({"type": "string"})"})
        {
            CAPTURE(value);
            Json schema = objectSchema();
            schema["additionalProperties"] = parse(value);
            const auto violation = findSchemaSubsetViolation(schema);
            REQUIRE(violation.has_value());
            CHECK(violation->code == error_code::invalidSchema);
            CHECK(violation->pointer == "/additionalProperties");
        }
    }

    TEST_CASE("property names, $defs names, enum values and defaults are data, not keywords")
    {
        // Properties called like keywords; an enum and a default that contain keyword-looking objects.
        const Json schema = parse(R"({
            "type": "object",
            "properties": {
                "pattern": {"type": "string"},
                "format": {"type": "string"},
                "properties": {"type": "string"},
                "kind": {"type": "object", "properties": {}, "additionalProperties": false,
                         "default": {"pattern": "a", "oneOf": [1]}},
                "choice": {"enum": [{"pattern": "a"}, {"type": "object"}]}
            },
            "additionalProperties": false,
            "$defs": {"pattern": {"type": "string"}, "format": {"type": "string"}}
        })");
        const auto violation = findSchemaSubsetViolation(schema);
        INFO((violation ? violation->pointer + ": " + violation->message : std::string("none")));
        CHECK_FALSE(violation.has_value());
    }

    TEST_CASE("only internal references are allowed")
    {
        const Json internal = parse(R"({"type": "object", "properties": {"a": {"$ref": "#/$defs/t"}},
                                        "additionalProperties": false, "$defs": {"t": {"type": "string"}}})");
        CHECK_FALSE(findSchemaSubsetViolation(internal).has_value());

        for (const char* ref : {"http://example.com/schema.json", "other.json#/$defs/t", "file:///etc/passwd"})
        {
            CAPTURE(ref);
            Json external = objectSchema();
            external["properties"]["a"]["$ref"] = ref;
            const auto violation = findSchemaSubsetViolation(external);
            REQUIRE(violation.has_value());
            CHECK(violation->code == error_code::invalidSchema);
            CHECK(violation->pointer == "/properties/a/$ref");
        }
    }

    TEST_CASE("a schema that is not an object schema at the root is a violation")
    {
        for (const char* text :
             {R"({"type": "string"})", R"({"type": "array", "items": {"type": "string"}})", "true", "[]", "null", "{}"})
        {
            CAPTURE(text);
            const auto violation = findSchemaSubsetViolation(parse(text));
            REQUIRE(violation.has_value());
            CHECK(violation->code == error_code::invalidSchema);
            CHECK(violation->pointer.empty());
        }
    }

    //==========================================================================
    TEST_CASE("registerCommand refuses a bad paramsSchema with invalid_schema and a pointer into the command")
    {
        CommandRegistry registry;
        Command command = makeCommand("track.create");
        command.paramsSchema = withPropertyKeyword("pattern", "^a");
        const auto outcome = registry.registerCommand(command);
        CHECK_FALSE(outcome.ok);
        CHECK(outcome.error.code == error_code::invalidSchema);
        CHECK(outcome.error.pointer == "/paramsSchema/properties/x/pattern");
        CHECK(outcome.error.message.find("pattern") != std::string::npos);
        CHECK(registry.size() == 0);
        CHECK_FALSE(registry.contains("track.create"));
    }

    TEST_CASE("registerCommand refuses a bad resultSchema with invalid_schema and a pointer into the command")
    {
        CommandRegistry registry;
        Command command = makeCommand("track.create");
        command.resultSchema = parse(R"({"type": "object", "properties": {"value": {"type": "integer"}}})");
        const auto outcome = registry.registerCommand(command);
        CHECK_FALSE(outcome.ok);
        CHECK(outcome.error.code == error_code::invalidSchema);
        CHECK(outcome.error.pointer == "/resultSchema");
        CHECK(registry.size() == 0);
    }

    TEST_CASE("registerCommand refuses an object without additionalProperties:false in either schema")
    {
        for (const char* which : {"paramsSchema", "resultSchema"})
        {
            CAPTURE(which);
            CommandRegistry registry;
            Command command = makeCommand("track.create");
            const Json bad = parse(R"({"type": "object", "properties": {"a": {"type": "string"}}})");
            (std::string(which) == "paramsSchema" ? command.paramsSchema : command.resultSchema) = bad;
            const auto outcome = registry.registerCommand(command);
            CHECK_FALSE(outcome.ok);
            CHECK(outcome.error.code == error_code::invalidSchema);
            CHECK(outcome.error.pointer == std::string("/") + which);
        }
    }

    TEST_CASE("registerCommand accepts a schema that uses every allowed keyword")
    {
        CommandRegistry registry;
        const auto outcome = registry.registerCommand(makeCommand("track.create", fullSubsetSchema()));
        INFO(outcome.error.code << ": " << outcome.error.message << " @ " << outcome.error.pointer);
        CHECK(outcome.ok);
    }
}
