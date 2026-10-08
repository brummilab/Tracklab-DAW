#include "core/schema_validation.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <exception>
#include <string_view>
#include <utility>
#include <vector>

namespace tracklab::core
{

namespace
{

using Pointer = Json::json_pointer;

/** What the validator reported first: the object/value it looked at, and its own wording. */
struct RawFailure
{
    Pointer pointer;
    Json instance;
    std::string message;
};

class FirstErrorHandler final : public nlohmann::json_schema::error_handler
{
public:
    void error(const Pointer& pointer, const Json& instance, const std::string& message) override
    {
        if (!failure)
            failure = RawFailure{pointer, instance, message};
    }

    std::optional<RawFailure> failure;
};

//==============================================================================
// Describing values and finding the schema of a place in the instance

const char* typeName(const Json& value)
{
    if (value.is_null())
        return "null";
    if (value.is_boolean())
        return "boolean";
    if (value.is_number_integer())  // also true for unsigned
        return "integer";
    if (value.is_number())
        return "number";
    if (value.is_string())
        return "string";
    if (value.is_array())
        return "array";
    if (value.is_object())
        return "object";
    return "binary";
}

/** A scalar as JSON text, shortened to a few bytes (never cut inside a UTF-8 character). */
std::string shortDump(const Json& value)
{
    constexpr std::size_t maxBytes = 40;
    std::string text = value.dump(-1, ' ', false, Json::error_handler_t::replace);
    if (text.size() > maxBytes)
    {
        std::size_t cut = maxBytes;
        while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0U) == 0x80U)  // never cut inside a character
            --cut;
        text = text.substr(0, cut) + "...";
    }
    return text;
}

/** The value for "got ...": the plain value for scalars, the type name for containers. */
std::string describeGot(const Json& value)
{
    return value.is_array() || value.is_object() ? std::string(typeName(value)) : shortDump(value);
}

/** "number 5", "string \"abc\"", "array": the type, plus the value for scalars. */
std::string describeValue(const Json& value)
{
    const std::string type = typeName(value);
    return value.is_array() || value.is_object() || value.is_null() ? type : type + " " + shortDump(value);
}

/** The limit as the schema author wrote it ("12", not the validator's "12.0"); the validator's text as fallback. */
std::string boundText(const Json* schema, const char* keyword, const std::string& fallback)
{
    if (schema != nullptr)
        if (const auto bound = schema->find(keyword); bound != schema->end() && bound->is_number())
            return bound->dump();
    return fallback;
}

const Json* followRef(const Json& root, const Json* node)
{
    for (int hops = 0; hops < 32 && node != nullptr; ++hops)
    {
        if (!node->is_object())
            return nullptr;
        const auto ref = node->find("$ref");
        if (ref == node->end() || !ref->is_string())
            return node;
        const auto& text = ref->get_ref<const std::string&>();
        if (!text.starts_with("#/"))
            return nullptr;
        try
        {
            node = &root.at(Pointer(text.substr(1)));
        }
        catch (const Json::exception&)
        {
            return nullptr;
        }
    }
    return nullptr;
}

std::vector<std::string> splitPointer(const std::string& pointer)
{
    std::vector<std::string> tokens;
    std::size_t start = 1;  // pointer starts with "/" unless it is empty
    while (!pointer.empty() && start <= pointer.size())
    {
        const auto slash = pointer.find('/', start);
        std::string token = pointer.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        for (const auto& [from, to] : {std::pair<std::string_view, char>{"~1", '/'}, {"~0", '~'}})
            for (auto at = token.find(from); at != std::string::npos; at = token.find(from, at + 1))
                token.replace(at, 2, 1, to);
        tokens.push_back(std::move(token));
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }
    return tokens;
}

/** The sub-schema that describes the instance place `pointer`, or nullptr if that cannot be told (e.g. below an
    anyOf). Follows properties, items and internal references. */
const Json* schemaAt(const Json& root, const Pointer& pointer)
{
    const Json* node = followRef(root, &root);
    for (const auto& token : splitPointer(pointer.to_string()))
    {
        if (node == nullptr)
            return nullptr;
        const auto properties = node->find("properties");
        const auto items = node->find("items");
        if (properties != node->end() && properties->is_object() && properties->contains(token))
            node = followRef(root, &properties->at(token));
        else if (items != node->end() && !token.empty() &&
                 std::ranges::all_of(token, [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }))
            node = followRef(root, &*items);
        else
            return nullptr;
    }
    return node;
}

std::string expectedType(const Json* schema, const char* fallback)
{
    if (schema != nullptr)
        if (const auto type = schema->find("type"); type != schema->end() && type->is_string())
            return type->get<std::string>();
    return fallback;
}

/** Text between `prefix` and `suffix` if `message` has exactly that form. */
std::optional<std::string> between(const std::string& message, std::string_view prefix, std::string_view suffix)
{
    if (message.size() < prefix.size() + suffix.size() || !message.starts_with(prefix) || !message.ends_with(suffix))
        return std::nullopt;
    return message.substr(prefix.size(), message.size() - prefix.size() - suffix.size());
}

//==============================================================================
// Rewriting the validator's first problem

ValidationFailure enrich(const Json& root, const RawFailure& raw)
{
    const std::string& message = raw.message;
    const std::string at = raw.pointer.to_string();

    // "required property 'x' not found in object" is reported at the object: point at the missing field instead.
    if (const auto name = between(message, "required property '", "' not found in object"))
    {
        const Pointer field = raw.pointer / *name;
        const Json* schema = schemaAt(root, field);
        std::string text = "missing required property '" + *name + "'";
        if (schema != nullptr && schema->contains("type"))
            text += " (expected " + expectedType(schema, "value") + ")";
        return {field.to_string(), text};
    }

    // additionalProperties:false is the only schema form we allow, so the validator's wording is always this.
    if (const auto name =
            between(message, "validation failed for additional property '", "': instance invalid as per false-schema"))
    {
        std::string text = "unknown property '" + *name + "'; ";
        std::string allowed;
        if (const Json* schema = schemaAt(root, raw.pointer); schema != nullptr && schema->contains("properties"))
            for (const auto& item : schema->at("properties").items())
                allowed += (allowed.empty() ? "" : ", ") + item.key();
        text += allowed.empty() ? "this object takes no properties" : "allowed: " + allowed;
        return {(raw.pointer / *name).to_string(), text};
    }

    const Json* schema = schemaAt(root, raw.pointer);

    if (message == "unexpected instance type")
    {
        if (schema != nullptr && schema->contains("type"))
            return {at, "expected " + expectedType(schema, "value") + ", got " + describeValue(raw.instance)};
        return {at, "unexpected type: got " + describeValue(raw.instance)};
    }

    // ASCII "<=" / ">=" on purpose: the text is also shown in places that are not guaranteed to be UTF-8 clean.
    if (const auto bound = between(message, "instance exceeds maximum of ", ""))
        return {at, "expected " + expectedType(schema, "number") + " <= " + boundText(schema, "maximum", *bound) +
                        ", got " + describeGot(raw.instance)};
    if (const auto bound = between(message, "instance is below minimum of ", ""))
        return {at, "expected " + expectedType(schema, "number") + " >= " + boundText(schema, "minimum", *bound) +
                        ", got " + describeGot(raw.instance)};

    if (message.starts_with("no subschema has succeeded") && schema != nullptr && schema->contains("anyOf"))
    {
        std::string alternatives;
        for (const auto& alternative : schema->at("anyOf"))
            alternatives += (alternatives.empty() ? "" : " | ") + expectedType(followRef(root, &alternative), "other");
        return {at, "expected " + alternatives + ", got " + describeValue(raw.instance)};
    }

    if (message == "instance not found in required enum" && schema != nullptr && schema->contains("enum"))
        return {at, "expected one of " + schema->at("enum").dump() + ", got " + describeValue(raw.instance)};

    return {at, message};
}

}  // namespace

std::optional<std::string> CompiledSchema::compile(const Json& source)
{
    try
    {
        schema = source;
        validator.set_root_schema(schema);
        return std::nullopt;
    }
    catch (const std::exception& error)
    {
        return std::string(error.what());
    }
}

std::optional<ValidationFailure> CompiledSchema::validate(const Json& instance) const
{
    try
    {
        FirstErrorHandler handler;
        validator.validate(instance, handler);
        if (!handler.failure)
            return std::nullopt;
        return enrich(schema, *handler.failure);
    }
    catch (const std::exception& error)
    {
        return ValidationFailure{"", std::string("the validator failed: ") + error.what()};
    }
}

}  // namespace tracklab::core
