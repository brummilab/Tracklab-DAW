// Validation of JSON values against a command schema (M1-02), a thin layer over pboettch/json-schema-validator.
// Internal to src/core: the registry uses it for the params and the result of every command.
//
// The validator reports problems in its own words and at the object that holds the problem. Those texts go to
// Claude (tool errors), so this layer rewrites the first problem into something a model can act on: the pointer
// names the offending field, the message says what was expected and what was received.
#pragma once

#include "core/command.h"

#include <nlohmann/json-schema.hpp>

#include <optional>
#include <string>

namespace tracklab::core
{

/** Where and what: pointer is an RFC 6901 JSON Pointer into the validated instance. */
struct ValidationFailure
{
    std::string pointer;
    std::string message;
};

class CompiledSchema
{
public:
    /** Compiles `schema` (already checked against the subset). Returns the validator's complaint, or nullopt. */
    [[nodiscard]] std::optional<std::string> compile(const Json& schema);

    /** The first problem of `instance`, or nullopt if it is valid. Does not throw. */
    [[nodiscard]] std::optional<ValidationFailure> validate(const Json& instance) const;

private:
    Json schema;
    nlohmann::json_schema::json_validator validator;
};

}  // namespace tracklab::core
