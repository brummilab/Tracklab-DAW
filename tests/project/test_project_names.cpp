// Project names (M1-04, review round 1): the name is a folder and a file name and the project has to open on Windows and
// Linux, so the rules are those of Windows. Everything else is allowed. Also: the empty template and the undo depth.
#include "project/project_fixture.h"

#include "core/undo_levels.h"

#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::project;

const std::vector<std::string>& forbiddenNames()
{
    static const std::vector<std::string> names = {
        // forbidden characters, one at a time
        "a<b", "a>b", "a:b", "a\"b", "a/b", "a\\b", "a|b", "a?b", "a*b",
        // control characters
        std::string("a\tb"), std::string("a\nb"), std::string("a\001b"), std::string("a\037b"),
        // blanks at the ends, a dot at the end, the folder references, nothing
        " Muster", "Muster ", "Muster.", ".", "..", "",
        // reserved device names of Windows: any case, with an extension, all numbers
        "CON", "con", "Prn", "AUX", "nul", "NUL.txt", "aux.tar.gz", "COM1", "com9", "LPT1", "lpt9", "Com5.x"};
    return names;
}

const std::vector<std::string>& allowedNames()
{
    static const std::vector<std::string> names = {"#Probe", "Band@Ort", "Lied, Fassung 2", "Rock;Roll", "Fish & Chips",
                                                   // look like reserved names, are none
                                                   "CONSOLE", "COM10", "COM0", "LPT", "Nullpunkt", "a.b",
                                                   // an umlaut (UTF-8)
                                                   std::string("\xC3\x9C") + "bung"};
    return names;
}

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("names that Windows or a file system forbids are invalid_project_name, and nothing is created")
    {
        ProjectFixture f;

        for (const auto& name : forbiddenNames())
        {
            INFO("name: `" << name << "`");
            const auto result = f.tryRun("project.new", Json{{"folder", utf8(f.root())}, {"name", name}});
            CHECK_FALSE(result.ok);
            CHECK(result.error.code == "invalid_project_name");
            CHECK(result.error.message.find("not a valid project name") != std::string::npos);
        }

        CHECK(f.root().getNumberOfChildFiles(juce::File::findFilesAndDirectories) == 0);
        CHECK(f.session->edit() == nullptr);
    }

    TEST_CASE("the same names are refused by project.save_as")
    {
        ProjectFixture f;
        const auto first = f.newProject("Erstes");

        for (const auto& name : forbiddenNames())
        {
            INFO("name: `" << name << "`");
            CHECK(f.errorOf("project.save_as", Json{{"folder", utf8(f.root())}, {"name", name}}) ==
                  "invalid_project_name");
        }

        CHECK(f.info()["path"].get<std::string>() == utf8(first));
        CHECK(f.root().getNumberOfChildFiles(juce::File::findFilesAndDirectories) == 1);
    }

    TEST_CASE("the message says why the name is refused")
    {
        ProjectFixture f;

        const auto reserved = f.tryRun("project.new", Json{{"folder", utf8(f.root())}, {"name", "NUL.txt"}});
        const auto character = f.tryRun("project.new", Json{{"folder", utf8(f.root())}, {"name", "a?b"}});
        const auto blank = f.tryRun("project.new", Json{{"folder", utf8(f.root())}, {"name", "Muster "}});

        CHECK(reserved.error.message.find("reserved") != std::string::npos);
        CHECK(character.error.message.find("'?'") != std::string::npos);
        CHECK(blank.error.message.find("blank") != std::string::npos);
    }

    TEST_CASE("names with # @ , ; & and names that only look like reserved ones are fine")
    {
        for (const auto& name : allowedNames())
        {
            INFO("name: `" << name << "`");
            ProjectFixture f;

            const auto result = f.tryRun("project.new", Json{{"folder", utf8(f.root())}, {"name", name}});

            INFO(result.error.code << ": " << result.error.message);
            CHECK(result.ok);
            CHECK(f.projectFile(name).existsAsFile());
        }
    }

    //==========================================================================
    TEST_CASE("an empty project has the master at 0 dB and no track")
    {
        ProjectFixture f;
        f.newProject("Muster");

        auto& e = f.edit();

        CHECK(te::volumeFaderPositionToDB(e.getMasterSliderPosParameter()->getCurrentValue()) == doctest::Approx(0.0));
        CHECK(te::getAudioTracks(e).isEmpty());
    }

    TEST_CASE("the undo depth lives in core and is 200")
    {
        CHECK(tracklab::core::defaultUndoLevels == 200);
    }
}
