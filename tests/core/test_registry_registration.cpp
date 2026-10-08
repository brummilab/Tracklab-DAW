// Command registry (M1-02): registering, listing, duplicate ids, invalid ids, tool-name mapping in both directions.
#include "core/core_test_helpers.h"

#include "core/tool_names.h"

#include <string>
#include <utility>
#include <vector>

namespace
{

using namespace tracklab::core;
using namespace tracklab_test::core_helpers;

std::string repeat(const std::string& text, int times)
{
    std::string out;
    for (int i = 0; i < times; ++i)
        out += text;
    return out;
}

}  // namespace

TEST_SUITE("core")
{
    TEST_CASE("an empty registry has no commands")
    {
        const CommandRegistry registry;
        CHECK(registry.size() == 0);
        CHECK(registry.list().empty());
        CHECK_FALSE(registry.contains("track.create"));
        CHECK(registry.find("track.create") == nullptr);
    }

    TEST_CASE("a valid command can be registered and is found with all its fields")
    {
        CommandRegistry registry;
        Command command = makeCommand("track.create", richParamsSchema());
        command.titleDe = "Spur anlegen";
        command.descriptionEn = "Creates a track.";
        command.flags.undoable = true;
        command.shortcut = "Ctrl+T";
        command.menuPath = "Spur/Neu";
        const Json paramsSchema = command.paramsSchema;
        const Json resultSchema = command.resultSchema;

        const auto outcome = registry.registerCommand(command);
        INFO(outcome.error.code << ": " << outcome.error.message);
        REQUIRE(outcome.ok);

        CHECK(registry.size() == 1);
        CHECK(registry.contains("track.create"));
        const Command* found = registry.find("track.create");
        REQUIRE(found != nullptr);
        CHECK(found->id == "track.create");
        CHECK(found->titleDe == "Spur anlegen");
        CHECK(found->descriptionEn == "Creates a track.");
        CHECK(found->paramsSchema == paramsSchema);
        CHECK(found->resultSchema == resultSchema);
        CHECK(found->flags == CommandFlags{.undoable = true});
        CHECK(found->shortcut == "Ctrl+T");
        CHECK(found->menuPath == "Spur/Neu");
        CHECK(static_cast<bool>(found->handler));
    }

    TEST_CASE("list() returns all commands sorted by id, independent of the registration order")
    {
        const std::vector<std::string> ids{"track.create", "app.version", "mixer.set_volume", "track.delete"};
        CommandRegistry forward;
        CommandRegistry backward;
        for (const auto& id : ids)
            registerOrFail(forward, makeCommand(id));
        for (auto it = ids.rbegin(); it != ids.rend(); ++it)
            registerOrFail(backward, makeCommand(*it));

        const std::vector<std::string> expected{"app.version", "mixer.set_volume", "track.create", "track.delete"};
        for (const CommandRegistry* registry : {&forward, &backward})
        {
            std::vector<std::string> listed;
            for (const Command* command : registry->list())
                listed.push_back(command->id);
            CHECK(listed == expected);
        }
    }

    TEST_CASE("valid ids are accepted")
    {
        for (const char* id : {"track.create", "assistant.propose_plan", "analyze.find_song_boundaries", "eq.band2"})
        {
            CAPTURE(id);
            CHECK(isValidCommandId(id));
            CommandRegistry registry;
            CHECK(registry.registerCommand(makeCommand(id)).ok);
        }
    }

    //==========================================================================
    TEST_CASE("registering an id twice is refused with duplicate_id and keeps the first command")
    {
        CommandRegistry registry;
        Command first = makeCommand("track.create");
        first.titleDe = "Erste";
        registerOrFail(registry, first);

        Command second = makeCommand("track.create");
        second.titleDe = "Zweite";
        const auto outcome = registry.registerCommand(second);

        CHECK_FALSE(outcome.ok);
        CHECK(outcome.error.code == error_code::duplicateId);
        CHECK(outcome.error.pointer == "/id");
        CHECK_FALSE(outcome.error.message.empty());
        CHECK(registry.size() == 1);
        REQUIRE(registry.find("track.create") != nullptr);
        CHECK(registry.find("track.create")->titleDe == "Erste");
    }

    TEST_CASE("malformed ids are refused with invalid_id and nothing is registered")
    {
        const std::vector<std::string> badIds{"",
                                              ".create",
                                              "track.",
                                              "track..create",
                                              "track create",
                                              "track.cre ate",
                                              "tr@ck.create",
                                              "track.caf\xC3\xA9",  // non-ASCII
                                              "track.create\n",
                                              "track/create"};
        for (const auto& id : badIds)
        {
            CAPTURE(id);
            CHECK_FALSE(isValidCommandId(id));
            CommandRegistry registry;
            const auto outcome = registry.registerCommand(makeCommand(id));
            CHECK_FALSE(outcome.ok);
            CHECK(outcome.error.code == error_code::invalidId);
            CHECK(outcome.error.pointer == "/id");
            CHECK_FALSE(outcome.error.message.empty());
            CHECK(registry.size() == 0);
        }
    }

    TEST_CASE("ids with three or more segments are valid and map to a tool name with all dots replaced")
    {
        for (const char* id : {"mixer.track.set_volume", "a.b.c.d"})
        {
            CAPTURE(id);
            CHECK(isValidCommandId(id));
            CommandRegistry registry;
            REQUIRE(registry.registerCommand(makeCommand(id)).ok);
            CHECK(registry.toolNameForId(id) == toolNameFromId(id));
            CHECK(registry.idForToolName(toolNameFromId(id)) == id);
        }
        CHECK(toolNameFromId("mixer.track.set_volume") == "mixer_track_set_volume");
        CHECK_FALSE(isValidCommandId("mixer.track."));
        CHECK_FALSE(isValidCommandId("mixer..track"));
    }

    TEST_CASE("id rules are strict: at least two segments, lowercase ASCII, a segment starts with a letter")
    {
        // Design Rev 3 only shows ids like "track.create"; the strict reading is the safe one for a tool name that
        // is derived from the id.
        for (const char* id : {"track", "Track.create", "track.Create", "1track.create", "track._create"})
        {
            CAPTURE(id);
            CHECK_FALSE(isValidCommandId(id));
            CommandRegistry registry;
            CHECK(registry.registerCommand(makeCommand(id)).error.code == error_code::invalidId);
        }
    }

    TEST_CASE("an id whose tool name would be longer than 128 characters is refused with invalid_tool_name")
    {
        const std::string longId = "track." + repeat("a", 123);  // 129 characters
        REQUIRE(longId.size() == 129);
        CommandRegistry registry;
        const auto outcome = registry.registerCommand(makeCommand(longId));
        CHECK_FALSE(outcome.ok);
        CHECK(outcome.error.code == error_code::invalidToolName);
        CHECK(outcome.error.pointer == "/id");
        CHECK(registry.size() == 0);

        const std::string longestId = "track." + repeat("a", 122);  // 128 characters
        REQUIRE(longestId.size() == 128);
        CHECK(registry.registerCommand(makeCommand(longestId)).ok);
    }

    TEST_CASE("a command without handler is refused with missing_handler")
    {
        CommandRegistry registry;
        Command command = makeCommand("track.create");
        command.handler = nullptr;
        const auto outcome = registry.registerCommand(command);
        CHECK_FALSE(outcome.ok);
        CHECK(outcome.error.code == error_code::missingHandler);
        CHECK(outcome.error.pointer == "/handler");
        CHECK(registry.size() == 0);
    }

    //==========================================================================
    TEST_CASE("tool names: a valid name matches ^[a-zA-Z0-9_-]{1,128}$")
    {
        CHECK(isValidToolName("a"));
        CHECK(isValidToolName("track_create"));
        CHECK(isValidToolName("Mixed-Case_9"));
        CHECK(isValidToolName(repeat("x", 128)));

        CHECK_FALSE(isValidToolName(""));
        CHECK_FALSE(isValidToolName(repeat("x", 129)));
        CHECK_FALSE(isValidToolName("track.create"));
        CHECK_FALSE(isValidToolName("track create"));
        CHECK_FALSE(isValidToolName("track\n"));
        CHECK_FALSE(isValidToolName("tr\xC3\xA4"
                                    "ck"));
    }

    TEST_CASE("tool name = id with '_' instead of '.'")
    {
        CHECK(toolNameFromId("track.create") == "track_create");
        CHECK(toolNameFromId("assistant.propose_plan") == "assistant_propose_plan");
        CHECK(toolNameFromId("app.version") == "app_version");
    }

    TEST_CASE("the registry maps id to tool name and back")
    {
        CommandRegistry registry;
        registerOrFail(registry, makeCommand("track.create"));
        registerOrFail(registry, makeCommand("assistant.propose_plan"));

        CHECK(registry.toolNameForId("track.create") == "track_create");
        CHECK(registry.idForToolName("track_create") == "track.create");

        // The reverse direction is a map, not "replace _ by .": underscores of the id survive.
        CHECK(registry.toolNameForId("assistant.propose_plan") == "assistant_propose_plan");
        CHECK(registry.idForToolName("assistant_propose_plan") == "assistant.propose_plan");
    }

    TEST_CASE("unknown ids and tool names map to nothing")
    {
        CommandRegistry registry;
        registerOrFail(registry, makeCommand("track.create"));

        CHECK_FALSE(registry.toolNameForId("track.delete").has_value());
        CHECK_FALSE(registry.idForToolName("track_delete").has_value());
        CHECK_FALSE(registry.idForToolName("track.create").has_value());  // an id is not a tool name
        CHECK_FALSE(registry.idForToolName("").has_value());
    }

    TEST_CASE("two ids with the same tool name collide: the second is refused with tool_name_collision")
    {
        // "a.b_c" and "a_b.c" both become "a_b_c".
        for (const auto& [first, second] : {std::pair<const char*, const char*>{"a.b_c", "a_b.c"},
                                            std::pair<const char*, const char*>{"a_b.c", "a.b_c"}})
        {
            CAPTURE(first);
            CAPTURE(second);
            CommandRegistry registry;
            registerOrFail(registry, makeCommand(first));

            const auto outcome = registry.registerCommand(makeCommand(second));
            CHECK_FALSE(outcome.ok);
            CHECK(outcome.error.code == error_code::toolNameCollision);
            CHECK(outcome.error.pointer == "/id");
            CHECK(outcome.error.message.find("a_b_c") != std::string::npos);

            CHECK(registry.size() == 1);
            CHECK(registry.contains(first));
            CHECK_FALSE(registry.contains(second));
            CHECK(registry.idForToolName("a_b_c") == first);
        }
    }

    TEST_CASE("a refused registration does not change the tool-name map")
    {
        CommandRegistry registry;
        registerOrFail(registry, makeCommand("track.create"));
        CHECK_FALSE(registry.registerCommand(makeCommand("track.create")).ok);  // duplicate
        Command badSchema = makeCommand("track.delete");
        badSchema.paramsSchema = Json::parse(R"({"type": "object"})");  // object without additionalProperties:false
        CHECK_FALSE(registry.registerCommand(badSchema).ok);

        CHECK(registry.size() == 1);
        CHECK(registry.idForToolName("track_create") == "track.create");
        CHECK_FALSE(registry.idForToolName("track_delete").has_value());
        // The id that was refused because of its schema is still free.
        CHECK(registry.registerCommand(makeCommand("track.delete")).ok);
    }
}
