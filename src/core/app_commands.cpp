#include "core/app_commands.h"

#include <utility>

namespace tracklab::core
{

RegisterResult registerAppCommands(CommandRegistry& registry, std::string versionText)
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
    version.handler = [text = std::move(versionText)](const Json&) { return Json{{"version", text}}; };
    return registry.registerCommand(std::move(version));
}

}  // namespace tracklab::core
