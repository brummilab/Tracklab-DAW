// First real command (M1-02): app.version (readOnly), registered by registerAppCommands().
#include "core/core_test_helpers.h"

#include "core/app_commands.h"

#include <string>

namespace
{

using namespace tracklab::core;
using namespace tracklab_test::core_helpers;

}  // namespace

TEST_SUITE("core")
{
    TEST_CASE("app.version is registered, readOnly and nothing else")
    {
        CommandRegistry registry;
        const auto outcome = registerAppCommands(registry, "1.2.3");
        INFO(outcome.error.code << ": " << outcome.error.message << " @ " << outcome.error.pointer);
        REQUIRE(outcome.ok);

        const Command* command = registry.find("app.version");
        REQUIRE(command != nullptr);
        CHECK(command->flags == CommandFlags{.readOnly = true});
        CHECK_FALSE(command->titleDe.empty());
        CHECK_FALSE(command->descriptionEn.empty());
        CHECK(registry.toolNameForId("app.version") == "app_version");
        CHECK(registry.idForToolName("app_version") == "app.version");
    }

    TEST_CASE("app.version returns the version it was registered with")
    {
        CommandRegistry registry;
        REQUIRE(registerAppCommands(registry, "1.2.3").ok);

        const auto result = registry.execute("app.version", Json::object());
        INFO(result.error.code << ": " << result.error.message);
        REQUIRE(result.ok);
        CHECK(result.result == Json{{"version", "1.2.3"}});
        CHECK(result.toJson() == Json::parse(R"({"ok": true, "result": {"version": "1.2.3"}})"));
    }

    TEST_CASE("app.version takes no parameters")
    {
        CommandRegistry registry;
        REQUIRE(registerAppCommands(registry, TRACKLAB_EXPECTED_VERSION).ok);

        const auto result = registry.execute("app.version", Json{{"verbose", true}});
        CHECK_FALSE(result.ok);
        CHECK(result.error.code == error_code::invalidParams);
        CHECK(result.error.message.find("verbose") != std::string::npos);
    }

    TEST_CASE("app.version has a params schema without properties and a result schema with a required version string")
    {
        CommandRegistry registry;
        REQUIRE(registerAppCommands(registry, "1.2.3").ok);
        const Command* command = registry.find("app.version");
        REQUIRE(command != nullptr);

        const Json& params = command->paramsSchema;
        CHECK(params.value("type", "") == "object");
        CHECK(params.value("additionalProperties", true) == false);
        CHECK((!params.contains("properties") || params["properties"].empty()));

        const Json& result = command->resultSchema;
        CHECK(result.value("type", "") == "object");
        CHECK(result.value("additionalProperties", true) == false);
        REQUIRE(result.contains("properties"));
        CHECK(result["properties"].size() == 1);
        CHECK(result["properties"]["version"].value("type", "") == "string");
        CHECK(result.value("required", Json::array()) == Json::array({"version"}));
    }

    TEST_CASE("registering the app commands twice into one registry is refused with duplicate_id")
    {
        CommandRegistry registry;
        REQUIRE(registerAppCommands(registry, "1.2.3").ok);
        const auto again = registerAppCommands(registry, "1.2.3");
        CHECK_FALSE(again.ok);
        CHECK(again.error.code == error_code::duplicateId);
        CHECK(registry.size() == 1);
    }
}
