// No personal data in project files (M1-04, DESIGN Rev 3 section 3): neither the login or full name of the system user
// nor the home folder nor the location of the project end up in the saved file. Tracktion writes the "user name" of its
// PropertyStorage into `modifiedBy`; the engine factory makes that "Tracklab".
#include "project/project_fixture.h"

#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::project;

/** What identifies the person running the tests: login name, full name, home folder. Names under 4 characters are not
    searched for (a short name would also match harmless text); the explicit modifiedBy checks cover them. */
std::vector<juce::String> personalStrings()
{
    std::vector<juce::String> strings;
    for (const auto& text : {juce::SystemStats::getLogonName(), juce::SystemStats::getFullUserName()})
        if (text.length() >= 4)
            strings.push_back(text);
    strings.push_back(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName());
    return strings;
}

void checkNoPersonalData(const juce::File& projectFile, const juce::File& projectsRoot)
{
    const auto text = projectFile.loadFileAsString();
    REQUIRE(text.isNotEmpty());
    for (const auto& personal : personalStrings())
    {
        INFO("personal string: " << personal);
        CHECK_FALSE(text.containsIgnoreCase(personal));
    }
    CHECK_FALSE(text.contains(projectsRoot.getFullPathName()));
}

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("a saved project contains no system user name, no home folder and no project location")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.addSampleContent();
        f.run("project.save");

        checkNoPersonalData(file, f.root());
    }

    TEST_CASE("a project that was never changed after project.new contains no personal data either")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");

        checkNoPersonalData(file, f.root());
    }

    TEST_CASE("modifiedBy in the saved file is the fixed value \"Tracklab\"")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.addSampleContent();
        f.run("project.save");

        const auto xml = parseXml(file);
        REQUIRE(xml != nullptr);
        CHECK(xml->getStringAttribute("modifiedBy") == "Tracklab");
    }

    TEST_CASE("a foreign modifiedBy of an opened file is replaced on the next save")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.run("project.close");
        rewriteXml(file, [](juce::XmlElement& root) { root.setAttribute("modifiedBy", "Erika Musterfrau"); });

        f.run("project.open", Json{{"path", utf8(file)}});
        f.makeModified();
        f.run("project.save");

        const auto text = file.loadFileAsString();
        CHECK_FALSE(text.contains("Erika Musterfrau"));
        const auto xml = parseXml(file);
        REQUIRE(xml != nullptr);
        CHECK(xml->getStringAttribute("modifiedBy") == "Tracklab");
    }

    TEST_CASE("save_as writes no personal data into the new project")
    {
        ProjectFixture f;
        f.newProject("Erstes");
        f.addSampleContent();

        f.run("project.save_as", Json{{"folder", utf8(f.root())}, {"name", "Zweites"}});

        checkNoPersonalData(f.projectFile("Zweites"), f.root());
        const auto xml = parseXml(f.projectFile("Zweites"));
        REQUIRE(xml != nullptr);
        CHECK(xml->getStringAttribute("modifiedBy") == "Tracklab");
    }
}
