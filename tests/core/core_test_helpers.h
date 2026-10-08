// Helpers of the core tests (M1-02): schema and command builders. Nothing here touches the file system or a device.
#pragma once

#include "core/command.h"
#include "core/command_registry.h"

#include "test_support.h"

#include <string>
#include <utility>

namespace tracklab_test::core_helpers
{

using tracklab::core::Command;
using tracklab::core::CommandFlags;
using tracklab::core::CommandRegistry;
using tracklab::core::Json;

/** {"type":"object","properties":<properties>,"required":<required>,"additionalProperties":false} */
inline Json objectSchema(Json properties = Json::object(), Json required = Json::array())
{
    Json schema = Json::object();
    schema["type"] = "object";
    schema["properties"] = std::move(properties);
    schema["additionalProperties"] = false;
    if (!required.empty())
        schema["required"] = std::move(required);
    return schema;
}

/** A schema with every kind of constraint the tests below need:
    name (string, required), gain_db (number -60..12), mode (enum), count (integer >= 0), tags (array of strings),
    opts (nested object with level 0..10). */
inline Json richParamsSchema()
{
    return Json::parse(R"({
        "type": "object",
        "properties": {
            "name": {"type": "string", "description": "Track name"},
            "gain_db": {"type": "number", "minimum": -60, "maximum": 12},
            "mode": {"type": "string", "enum": ["mono", "stereo"]},
            "count": {"type": "integer", "minimum": 0},
            "tags": {"type": "array", "items": {"type": "string"}},
            "opts": {
                "type": "object",
                "properties": {"level": {"type": "integer", "minimum": 0, "maximum": 10}},
                "additionalProperties": false
            }
        },
        "required": ["name"],
        "additionalProperties": false
    })");
}

/** Result schema {"value": integer} with "value" required. */
inline Json valueResultSchema()
{
    return Json::parse(R"({
        "type": "object",
        "properties": {"value": {"type": "integer"}},
        "required": ["value"],
        "additionalProperties": false
    })");
}

/** A valid command: id, empty params, result {"value": 1}. Fields can be adjusted by the caller afterwards. */
inline Command makeCommand(std::string id, Json paramsSchema = objectSchema(), Json resultSchema = valueResultSchema())
{
    Command command;
    command.id = std::move(id);
    command.titleDe = "Testbefehl";
    command.descriptionEn = "A command for tests.";
    command.paramsSchema = std::move(paramsSchema);
    command.resultSchema = std::move(resultSchema);
    command.handler = [](const Json&) { return Json{{"value", 1}}; };
    return command;
}

/** Registers `command` and fails the test (REQUIRE) if that is refused. */
inline void registerOrFail(CommandRegistry& registry, Command command)
{
    const auto id = command.id;
    const auto outcome = registry.registerCommand(std::move(command));
    INFO("registering " << id << ": " << outcome.error.code << " / " << outcome.error.message);
    REQUIRE(outcome.ok);
}

}  // namespace tracklab_test::core_helpers
