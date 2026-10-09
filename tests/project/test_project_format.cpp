// Project format (M1-04): format version and the migration framework (steps vN -> vN+1 on the state tree).
// Pure ValueTree tests: no engine, no file.
#include "project/project_format.h"

#include "test_support.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using namespace tracklab::project;

juce::ValueTree makeEditState(int version, bool withVersion = true)
{
    juce::ValueTree edit("EDIT");
    edit.setProperty("appVersion", "0.1.0", nullptr);
    edit.setProperty("keepMe", "Beispiel", nullptr);
    if (withVersion)
        edit.setProperty(formatVersionProperty, version, nullptr);
    juce::ValueTree track("AUDIOTRACK");
    track.setProperty("name", "Spur A", nullptr);
    edit.appendChild(track, nullptr);
    return edit;
}

/** Steps 0->1, 1->2, 2->3 that record the order in which they ran and mark the state. */
std::vector<MigrationStep> recordingSteps(std::vector<int>& ran)
{
    std::vector<MigrationStep> steps;
    for (int from = 0; from < 3; ++from)
        steps.push_back(MigrationStep{from, [&ran, from](juce::ValueTree& edit)
                                      {
                                          ran.push_back(from);
                                          edit.setProperty("step" + juce::String(from), true, nullptr);
                                      }});
    return steps;
}

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("the format version of this build is 1 and the property is tracklabFormatVersion")
    {
        CHECK(currentFormatVersion == 1);
        CHECK(std::string(formatVersionProperty) == "tracklabFormatVersion");
        CHECK(std::string(fileExtension) == ".tracklab");
    }

    TEST_CASE("the project folders are Audio, Renders, Backups and Peaks")
    {
        std::vector<std::string> names;
        for (const char* name : subFolderNames)
            names.emplace_back(name);
        CHECK(names == std::vector<std::string>{"Audio", "Renders", "Backups", "Peaks"});
    }

    TEST_CASE("formatVersionOf reads the integer property and is 0 without it")
    {
        CHECK(formatVersionOf(makeEditState(0, false)) == 0);
        CHECK(formatVersionOf(makeEditState(1)) == 1);
        CHECK(formatVersionOf(makeEditState(7)) == 7);
    }

    //==========================================================================
    // The built-in steps (v0 -> v1)
    TEST_CASE("the built-in migration brings a v0 state (no version property) to v1 and keeps its content")
    {
        auto edit = makeEditState(0, false);

        const auto outcome = migrateState(edit, builtInMigrationSteps(), currentFormatVersion);

        INFO(outcome.code << ": " << outcome.message);
        REQUIRE(outcome.ok);
        CHECK(outcome.fromVersion == 0);
        CHECK(outcome.toVersion == 1);
        CHECK(formatVersionOf(edit) == 1);
        CHECK(edit.getProperty("keepMe").toString() == "Beispiel");
        CHECK(edit.getProperty("appVersion").toString() == "0.1.0");
        REQUIRE(edit.getNumChildren() == 1);
        CHECK(edit.getChild(0).getProperty("name").toString() == "Spur A");
    }

    TEST_CASE("the built-in migration leaves a state that is already at the current version unchanged")
    {
        auto edit = makeEditState(currentFormatVersion);
        const auto before = edit.createCopy();

        const auto outcome = migrateState(edit, builtInMigrationSteps(), currentFormatVersion);

        INFO(outcome.code << ": " << outcome.message);
        REQUIRE(outcome.ok);
        CHECK(outcome.fromVersion == 1);
        CHECK(outcome.toVersion == 1);
        CHECK(edit.isEquivalentTo(before));
    }

    TEST_CASE("a state with a newer version than this build knows is refused with project_too_new")
    {
        auto edit = makeEditState(currentFormatVersion + 98);  // 99
        const auto before = edit.createCopy();

        const auto outcome = migrateState(edit, builtInMigrationSteps(), currentFormatVersion);

        CHECK_FALSE(outcome.ok);
        CHECK(outcome.code == error_code::projectTooNew);
        CHECK(outcome.message.find("99") != std::string::npos);
        CHECK(edit.isEquivalentTo(before));
    }

    //==========================================================================
    // The framework with test steps
    TEST_CASE("migrateState runs every step from the state's version up to the target, each once, in order")
    {
        std::vector<int> ran;
        const auto steps = recordingSteps(ran);

        SUBCASE("from v0 to v3")
        {
            auto edit = makeEditState(0, false);
            const auto outcome = migrateState(edit, steps, 3);
            REQUIRE(outcome.ok);
            CHECK(ran == std::vector<int>{0, 1, 2});
            CHECK(formatVersionOf(edit) == 3);
            CHECK(outcome.fromVersion == 0);
            CHECK(outcome.toVersion == 3);
        }
        SUBCASE("from v1 to v3: only the steps 1->2 and 2->3")
        {
            auto edit = makeEditState(1);
            const auto outcome = migrateState(edit, steps, 3);
            REQUIRE(outcome.ok);
            CHECK(ran == std::vector<int>{1, 2});
            CHECK(formatVersionOf(edit) == 3);
            CHECK(outcome.fromVersion == 1);
        }
        SUBCASE("to a target below the last step: later steps do not run")
        {
            auto edit = makeEditState(0, false);
            const auto outcome = migrateState(edit, steps, 2);
            REQUIRE(outcome.ok);
            CHECK(ran == std::vector<int>{0, 1});
            CHECK(formatVersionOf(edit) == 2);
            CHECK_FALSE(edit.hasProperty("step2"));
        }
    }

    TEST_CASE("migrateState does nothing when the state is at the target version")
    {
        std::vector<int> ran;
        const auto steps = recordingSteps(ran);
        auto edit = makeEditState(3);
        const auto before = edit.createCopy();

        const auto outcome = migrateState(edit, steps, 3);

        REQUIRE(outcome.ok);
        CHECK(ran.empty());
        CHECK(edit.isEquivalentTo(before));
    }

    TEST_CASE("migrateState refuses a state newer than the target without running a step")
    {
        std::vector<int> ran;
        const auto steps = recordingSteps(ran);
        auto edit = makeEditState(5);
        const auto before = edit.createCopy();

        const auto outcome = migrateState(edit, steps, 3);

        CHECK_FALSE(outcome.ok);
        CHECK(outcome.code == error_code::projectTooNew);
        CHECK(outcome.message.find('5') != std::string::npos);
        CHECK(ran.empty());
        CHECK(edit.isEquivalentTo(before));
    }

    TEST_CASE("a missing step in the chain fails with migration_failed and leaves the state as it was")
    {
        std::vector<int> ran;
        auto steps = recordingSteps(ran);
        steps.erase(steps.begin() + 1);  // no 1 -> 2
        auto edit = makeEditState(0, false);
        const auto before = edit.createCopy();

        const auto outcome = migrateState(edit, steps, 3);

        CHECK_FALSE(outcome.ok);
        CHECK(outcome.code == error_code::migrationFailed);
        CHECK(edit.isEquivalentTo(before));
        CHECK(formatVersionOf(edit) == 0);
    }

    TEST_CASE("a step that throws fails with migration_failed and nothing of the earlier steps stays visible")
    {
        std::vector<int> ran;
        auto steps = recordingSteps(ran);
        steps[1].apply = [](juce::ValueTree& edit)
        {
            edit.setProperty("half", true, nullptr);
            throw std::runtime_error("step failed on purpose");
        };
        auto edit = makeEditState(0, false);
        const auto before = edit.createCopy();

        const auto outcome = migrateState(edit, steps, 3);

        CHECK_FALSE(outcome.ok);
        CHECK(outcome.code == error_code::migrationFailed);
        CHECK_FALSE(outcome.message.empty());
        CHECK(edit.isEquivalentTo(before));
        CHECK_FALSE(edit.hasProperty("step0"));
        CHECK_FALSE(edit.hasProperty("half"));
        CHECK(formatVersionOf(edit) == 0);
    }

    TEST_CASE("a step can restructure the tree: rename a property and move a child")
    {
        std::vector<MigrationStep> steps;
        steps.push_back(MigrationStep{0, [](juce::ValueTree& edit)
                                      {
                                          edit.setProperty("renamed", edit.getProperty("keepMe"), nullptr);
                                          edit.removeProperty("keepMe", nullptr);
                                          edit.addChild(juce::ValueTree("NEWNODE"), 0, nullptr);
                                      }});
        auto edit = makeEditState(0, false);

        const auto outcome = migrateState(edit, steps, 1);

        REQUIRE(outcome.ok);
        CHECK(edit.getProperty("renamed").toString() == "Beispiel");
        CHECK_FALSE(edit.hasProperty("keepMe"));
        CHECK(edit.getChild(0).hasType("NEWNODE"));
        CHECK(formatVersionOf(edit) == 1);
    }
}
