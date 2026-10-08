// The JSON Schema subset allowed for command schemas (M1-02, DESIGN Rev 3 section 3).
//
// Allowed keywords: type, properties, required, additionalProperties (only the value false), enum, minimum,
// maximum, items, anyOf, $ref (only internal, "#/..."), $defs, description, default.
// Every schema that describes an object (type "object", or any of properties/required/additionalProperties) must
// have "additionalProperties": false. The root of a command schema is an object schema.
// Further rules (lead decision 6): "type" is a single type name (no list); an internal "$ref" must resolve inside the
// same schema and must not be a pure reference cycle; every name in "required" must be declared in "properties".
// Beside "$ref" only "description" and "default" are allowed (the validator ignores other keywords there).
// Property names (keys of "properties" and "$defs"), the values of "enum" and "default" are data, not keywords.
#pragma once

#include "core/command.h"

#include <optional>

namespace tracklab::core
{

/** The first violation of the subset in `schema` (depth first, in document order), or nullopt.
    error.code is error_code::invalidSchema; error.pointer is relative to `schema`
    (e.g. "/properties/x/pattern", "" for the root); error.message names the problem. */
std::optional<CommandError> findSchemaSubsetViolation(const Json& schema);

}  // namespace tracklab::core
