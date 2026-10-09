// project.new (M1-04): folder structure, the written file, names, refusals.
#include "project/project_fixture.h"

#include <string>

namespace
{

using namespace tracklab_test::project;
namespace project = tracklab::project;

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("project.new creates <folder>/<name>/<name>.tracklab with Audio, Renders, Backups and Peaks")
    {
        ProjectFixture f;

        const auto result = f.run("project.new", Json{{"folder", utf8(f.root())}, {"name", "Muster"}});

        const auto folder = f.root().getChildFile("Muster");
        CHECK(folder.isDirectory());
        CHECK(folder.getChildFile("Muster.tracklab").existsAsFile());
        for (const char* sub : project::subFolderNames)
            CHECK_MESSAGE(folder.getChildFile(sub).isDirectory(), "missing sub folder " << sub);
        CHECK(result["path"].get<std::string>() == utf8(folder.getChildFile("Muster.tracklab")));
        CHECK(result["name"].get<std::string>() == "Muster");
    }

    TEST_CASE("project.new puts nothing else next to the project file")
    {
        ProjectFixture f;
        f.newProject("Muster");

        CHECK(fileNamesIn(f.projectFolder("Muster")) == std::vector<std::string>{"Muster.tracklab"});
        CHECK(fileNamesIn(f.root()).empty());  // nothing beside the project folder either
    }

    TEST_CASE("project.new writes a valid project file right away: EDIT root, format version 1")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");

        const auto xml = parseXml(file);
        REQUIRE(xml != nullptr);
        CHECK(xml->hasTagName("EDIT"));
        CHECK(xml->getIntAttribute(project::formatVersionProperty, -1) == project::currentFormatVersion);
    }

    TEST_CASE("project.new stores relative media paths: alwaysUseRelativePaths is set in the file")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");

        const auto xml = parseXml(file);
        REQUIRE(xml != nullptr);
        CHECK(xml->getBoolAttribute("alwaysUseRelativePaths", false));
        CHECK(f.edit().alwaysUseRelativePaths.get());
    }

    TEST_CASE("project.new opens the project: get_info, the edit context and an empty undo history")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");

        const auto info = f.info();
        CHECK(info["path"].get<std::string>() == utf8(file));
        CHECK(info["name"].get<std::string>() == "Muster");
        CHECK(info["format_version"].get<int>() == 1);
        CHECK_FALSE(info["modified"].get<bool>());
        CHECK(f.context.edit() == f.session->edit());
        CHECK(f.context.edit() != nullptr);

        const auto undo = f.run("edit.get_undo_state");
        CHECK_FALSE(undo["can_undo"].get<bool>());
        CHECK_FALSE(undo["can_redo"].get<bool>());
    }

    TEST_CASE("project.new accepts the template \"empty\" and no other")
    {
        ProjectFixture f;

        SUBCASE("empty")
        {
            CHECK(f.tryRun("project.new", Json{{"folder", utf8(f.root())}, {"name", "Muster"}, {"template", "empty"}})
                      .ok);
        }
        SUBCASE("anything else is an invalid parameter")
        {
            CHECK(
                f.errorOf("project.new", Json{{"folder", utf8(f.root())}, {"name", "Muster"}, {"template", "rock"}}) ==
                "invalid_params");
            CHECK_FALSE(f.projectFolder("Muster").exists());
        }
    }

    TEST_CASE("project.new needs folder and name and takes nothing else")
    {
        ProjectFixture f;

        CHECK(f.errorOf("project.new", Json{{"name", "Muster"}}) == "invalid_params");
        CHECK(f.errorOf("project.new", Json{{"folder", utf8(f.root())}}) == "invalid_params");
        CHECK(f.errorOf("project.new", Json{{"folder", utf8(f.root())}, {"name", "Muster"}, {"discard", true}}) ==
              "invalid_params");
    }

    TEST_CASE("project.new takes names with spaces and umlauts (UTF-8 paths)")
    {
        ProjectFixture f;
        const std::string name = "\xC3\x9C"
                                 "bungsraum Aufnahme";  // "Uebungsraum Aufnahme" with a capital U umlaut

        const auto file = f.newProject(name);

        CHECK(file.existsAsFile());
        CHECK(file.getFileName() == juce::String::fromUTF8(name.c_str()) + ".tracklab");
        CHECK(f.info()["name"].get<std::string>() == name);
        f.run("project.save");
        CHECK(file.existsAsFile());
    }

    TEST_CASE("project.new refuses names that are not a plain name and creates nothing")
    {
        ProjectFixture f;
        // "../<evil>" would create a folder next to the root folder if the name were taken as a path.
        const auto evilName =
            juce::String("tracklab-evil-") + juce::String(juce::Random::getSystemRandom().nextInt64());
        const auto evil = f.root().getSiblingFile(evilName);
        const auto cleanUp = [&evil] { evil.deleteRecursively(); };

        for (const std::string bad : {std::string(), std::string("."), std::string(".."), std::string("a/b"),
                                      std::string("a\\b"), std::string("../") + evilName.toStdString()})
        {
            INFO("name: `" << bad << "`");
            const auto result = f.tryRun("project.new", Json{{"folder", utf8(f.root())}, {"name", bad}});
            CHECK_FALSE(result.ok);
            CHECK(result.error.code == "invalid_project_name");
        }

        const bool escaped = evil.exists();
        cleanUp();
        CHECK_FALSE(escaped);
        CHECK(f.root().getNumberOfChildFiles(juce::File::findFilesAndDirectories) == 0);
        CHECK(f.session->edit() == nullptr);  // no project was opened by the failed calls
    }

    TEST_CASE("project.new never overwrites an existing project: project_exists, file untouched")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.addSampleContent();
        f.run("project.save");
        f.run("project.close");
        const auto before = bytesOf(file);

        CHECK(f.errorOf("project.new", Json{{"folder", utf8(f.root())}, {"name", "Muster"}}) == "project_exists");

        CHECK(sameBytes(bytesOf(file), before));
        CHECK_FALSE(f.session->edit());  // the failed call opened nothing
    }

    TEST_CASE("a failed project.new leaves the open project open and unchanged")
    {
        ProjectFixture f;
        f.newProject("Muster");
        f.addSampleContent();
        f.run("project.save");
        f.settleEdit();
        auto* editBefore = f.session->edit();
        const auto before = bytesOf(f.projectFile("Muster"));

        CHECK(f.errorOf("project.new", Json{{"folder", utf8(f.root())}, {"name", "Muster"}}) == "project_exists");
        CHECK(f.errorOf("project.new", Json{{"folder", utf8(f.root())}, {"name", "../x"}}) == "invalid_project_name");

        CHECK(f.session->edit() == editBefore);
        CHECK(f.info()["name"].get<std::string>() == "Muster");
        CHECK(sameBytes(bytesOf(f.projectFile("Muster")), before));
    }

    TEST_CASE("project.new replaces the open project when nothing is unsaved")
    {
        ProjectFixture f;
        f.newProject("Erstes");
        f.settleEdit();

        const auto second = f.newProject("Zweites");

        CHECK(f.info()["path"].get<std::string>() == utf8(second));
        CHECK(f.context.edit() == f.session->edit());
        CHECK(f.projectFile("Erstes").existsAsFile());  // the first one stays on disk
    }
}
