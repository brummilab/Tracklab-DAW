#include "core/schema_subset.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace tracklab::core
{

namespace
{

using Pointer = Json::json_pointer;

constexpr std::array<std::string_view, 13> kAllowedKeywords{
    "type",  "properties", "required", "additionalProperties", "enum",   "minimum", "maximum", "items",
    "anyOf", "$ref",       "$defs",    "description",          "default"};

constexpr std::array<std::string_view, 7> kTypeNames{"null",   "boolean", "object", "array",
                                                     "number", "integer", "string"};

/** Schemas are tiny; the cap only protects the recursion against a hostile or generated schema. */
constexpr int kMaxDepth = 32;

CommandError violation(const Pointer& at, std::string message)
{
    return CommandError{std::string(error_code::invalidSchema), std::move(message), at.to_string()};
}

bool isInternalRef(const std::string& ref)
{
    return ref.starts_with("#/");
}

/** Checks that an internal "$ref" points at a schema inside `root` and is not part of a pure reference cycle. */
std::optional<CommandError> checkRef(const Json& root, const Json& ref, const Pointer& at)
{
    const Pointer refAt = at / "$ref";
    if (!ref.is_string())
        return violation(refAt, "'$ref' must be a string");
    const auto& text = ref.get_ref<const std::string&>();
    if (!isInternalRef(text))
        return violation(refAt, "only internal references (\"#/...\") are allowed, got '" + text + "'");

    try
    {
        Pointer target(text.substr(1));
        // Follow a chain of references; more hops than this can only be a cycle.
        for (int hops = 0; hops <= kMaxDepth; ++hops)
        {
            if (!root.contains(target))
                return violation(refAt, "'$ref' " + text + " points to nothing in this schema");
            const Json& node = root.at(target);
            if (!node.is_object())
                return violation(refAt, "'$ref' " + text + " does not point to a schema");
            const auto next = node.find("$ref");
            if (next == node.end() || !next->is_string() || !isInternalRef(next->get_ref<const std::string&>()))
                return std::nullopt;
            target = Pointer(next->get_ref<const std::string&>().substr(1));
        }
        return violation(refAt, "'$ref' " + text + " is part of a reference cycle");
    }
    catch (const Json::exception&)
    {
        return violation(refAt, "'$ref' " + text + " is not a valid JSON pointer");
    }
}

std::optional<CommandError> checkSchema(const Json& root, const Json& node, const Pointer& at, int depth);

std::optional<CommandError> checkSchemaMap(const Json& root, const Json& map, const Pointer& at, int depth)
{
    if (!map.is_object())
        return violation(at, "'" + at.back() + "' must be an object that maps names to schemas");
    for (const auto& [name, schema] : map.items())
        if (auto found = checkSchema(root, schema, at / name, depth + 1))
            return found;
    return std::nullopt;
}

/** The keyword checks of one schema node, without looking at the sub-schemas. */
std::optional<CommandError> checkKeywords(const Json& node, const Pointer& at, bool isRoot)
{
    for (const auto& item : node.items())
    {
        const std::string& key = item.key();
        if (std::ranges::find(kAllowedKeywords, key) == kAllowedKeywords.end())
            return violation(at / key, "keyword '" + key +
                                           "' is not allowed in command schemas (allowed: type, "
                                           "properties, required, additionalProperties, enum, minimum, maximum, items, "
                                           "anyOf, $ref, $defs, description, default)");
    }

    if (const auto type = node.find("type"); type != node.end())
    {
        if (!type->is_string())
            return violation(at / "type", "'type' must be a single type name (a string), not a list");
        if (std::ranges::find(kTypeNames, type->get_ref<const std::string&>()) == kTypeNames.end())
            return violation(at / "type", "unknown type '" + type->get_ref<const std::string&>() + "'");
    }
    const bool declaresObject = node.contains("type") && node["type"] == "object";
    if (isRoot && !declaresObject)
        return violation(at, "the root of a command schema must be an object schema (\"type\": \"object\")");

    const bool isObjectSchema = declaresObject || node.contains("properties") || node.contains("required") ||
                                node.contains("additionalProperties");
    if (isObjectSchema)
    {
        const auto additional = node.find("additionalProperties");
        if (additional == node.end())
            return violation(at, "an object schema needs \"additionalProperties\": false");
        if (*additional != false)
            return violation(at / "additionalProperties", "\"additionalProperties\" must be false");
    }

    if (const auto required = node.find("required"); required != node.end())
    {
        if (!required->is_array())
            return violation(at / "required", "'required' must be an array of property names");
        for (std::size_t i = 0; i < required->size(); ++i)
        {
            const Json& name = (*required)[i];
            if (!name.is_string())
                return violation(at / "required" / i, "'required' must only contain property names (strings)");
            const auto properties = node.find("properties");
            if (properties == node.end() || !properties->is_object() || !properties->contains(name.get<std::string>()))
                return violation(at / "required" / i,
                                 "required property '" + name.get<std::string>() + "' is not declared in 'properties'");
        }
    }

    if (const auto values = node.find("enum"); values != node.end() && (!values->is_array() || values->empty()))
        return violation(at / "enum", "'enum' must be a non-empty array");
    for (const char* bound : {"minimum", "maximum"})
        if (const auto value = node.find(bound); value != node.end() && !value->is_number())
            return violation(at / bound, std::string("'") + bound + "' must be a number");
    if (const auto text = node.find("description"); text != node.end() && !text->is_string())
        return violation(at / "description", "'description' must be a string");
    return std::nullopt;
}

std::optional<CommandError> checkSchema(const Json& root, const Json& node, const Pointer& at, int depth)
{
    if (depth > kMaxDepth)
        return violation(at, "schema is nested too deeply");
    if (!node.is_object())
        return violation(at, "a schema must be a JSON object");

    if (auto found = checkKeywords(node, at, depth == 0))
        return found;
    if (const auto ref = node.find("$ref"); ref != node.end())
        if (auto found = checkRef(root, *ref, at))
            return found;

    if (const auto properties = node.find("properties"); properties != node.end())
        if (auto found = checkSchemaMap(root, *properties, at / "properties", depth))
            return found;
    if (const auto items = node.find("items"); items != node.end())
        if (auto found = checkSchema(root, *items, at / "items", depth + 1))
            return found;
    if (const auto anyOf = node.find("anyOf"); anyOf != node.end())
    {
        if (!anyOf->is_array() || anyOf->empty())
            return violation(at / "anyOf", "'anyOf' must be a non-empty array of schemas");
        for (std::size_t i = 0; i < anyOf->size(); ++i)
            if (auto found = checkSchema(root, (*anyOf)[i], at / "anyOf" / i, depth + 1))
                return found;
    }
    if (const auto defs = node.find("$defs"); defs != node.end())
        if (auto found = checkSchemaMap(root, *defs, at / "$defs", depth))
            return found;
    return std::nullopt;
}

}  // namespace

std::optional<CommandError> findSchemaSubsetViolation(const Json& schema)
{
    return checkSchema(schema, schema, Pointer(), 0);
}

}  // namespace tracklab::core
