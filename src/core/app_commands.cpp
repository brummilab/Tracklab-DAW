#include "core/app_commands.h"

#include <utility>

namespace tracklab::core
{

RegisterResult registerAppCommands(CommandRegistry& registry, std::string appVersion)
{
    Command version;
    version.id = "app.version";
    version.titleDe = "Version anzeigen";
    version.descriptionEn = "Returns the version of Tracklab.";
    version.paramsSchema = Json::parse(R"({"type": "object", "properties": {}, "additionalProperties": false})");
    version.resultSchema = Json::parse(R"({
        "type": "object",
        "properties": {"version": {"type": "string"}},
        "required": ["version"],
        "additionalProperties": false
    })");
    version.flags.readOnly = true;
    version.handler = [appVersion = std::move(appVersion)](const Json&) { return Json{{"version", appVersion}}; };
    return registry.registerCommand(std::move(version));
}

}  // namespace tracklab::core
