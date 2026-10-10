// M1-05, review round 1: Windows opens paths of at most 259 characters, so project.new / project.save_as refuse, on every
// platform alike, a project whose longest file (a backup's temporary file with the highest counter) would be longer:
// path_too_long, nothing created. The tests compute the allowed name length from the folder they run in (the system's
// temporary folder, long on Windows) and test exactly at the boundary. The rule of 120 bytes per name is separate.
#include "project/path_length_helper.h"

#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::project;
using tracklab::project::AutosaveResult;

Json newParams(const juce::File& parent, const std::string& name)
{
    return Json{{"folder", utf8(parent)}, {"name", name}};
}

std::string supplementary(size_t count)  // U+1F3B5, 4 bytes in UTF-8, 2 UTF-16 units
{
    std::string text;
    for (size_t i = 0; i < count; ++i)
        text += "\xF0\x9F\x8E\xB5";
    return text;
}

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("project.new: the longest name that fits works completely, one character more is path_too_long")
    {
        ProjectFixture f;
        const auto parent = deepParent(f);
        const auto longest = longestNameFitting(parent);
        REQUIRE(longest >= 1);
        REQUIRE(longest <= 120);
        const std::string fitting(longest, 'a');
        const auto before = tracklab_test::snapshotOf(parent);

        const auto tooLong = f.tryRun("project.new", newParams(parent, std::string(longest + 1, 'a')));

        CHECK(tooLong.error.code == "path_too_long");
        CHECK(tooLong.error.message.find("259") != std::string::npos);
        CHECK(tracklab_test::snapshotOf(parent) == before);  // nothing was created
        CHECK(f.session->edit() == nullptr);

        const auto file = fileFromUtf8(f.run("project.new", newParams(parent, fitting))["path"].get<std::string>());
        f.settleEdit();
        f.useInjectedClock();
        f.addSampleTracks();
        f.saveAt(0);
        f.renameFirstTrack("Neu");
        CHECK(f.session->autosaveNow() == AutosaveResult::written);
        f.saveAt(0);  // same second: the backup gets a counter suffix
        f.saveAt(60);
        CHECK(backupNamesOf(file).size() == 3);
        f.run("project.close");
        CHECK(f.tryRun("project.open", Json{{"path", utf8(file)}}).ok);
    }

    TEST_CASE("project.new counts UTF-16 units: a character outside the BMP is two")
    {
        ProjectFixture f;
        const auto parent = deepParent(f);
        const auto longest = longestNameFitting(parent);
        // 5 characters of 2 units each, the rest ASCII: exactly `longest` units fit, one more does not. (Counted as
        // characters the name would be shorter than it is for Windows; counted as bytes it is 3 per supplementary
        // character longer, still far below the 120 byte rule.)
        REQUIRE(longest >= 11);
        const auto ascii = longest - 10;
        const std::string fitting = supplementary(5) + std::string(ascii, 'a');

        CHECK(f.errorOf("project.new", newParams(parent, fitting + "a")) == "path_too_long");
        CHECK(f.tryRun("project.new", newParams(parent, fitting)).ok);
    }

    TEST_CASE("project.save_as: the same boundary; a refused save_as changes nothing")
    {
        ProjectFixture f;
        const auto first = f.newProject("Muster");
        f.addSampleTracks();
        f.run("project.save");
        f.settleEdit();
        const auto parent = deepParent(f);
        const auto longest = longestNameFitting(parent);
        REQUIRE(longest >= 1);
        const auto rootBefore = tracklab_test::snapshotOf(f.root());

        const auto refused = f.tryRun("project.save_as", newParams(parent, std::string(longest + 1, 'b')));

        CHECK(refused.error.code == "path_too_long");
        CHECK(tracklab_test::snapshotOf(f.root()) == rootBefore);
        CHECK(f.info()["path"].get<std::string>() == utf8(first));

        const auto result = f.run("project.save_as", newParams(parent, std::string(longest, 'b')));
        CHECK(result["name"].get<std::string>() == std::string(longest, 'b'));
    }

    TEST_CASE("the rule of 120 bytes per name is independent of the path: 121 bytes are invalid_project_name anywhere")
    {
        ProjectFixture f;
        const auto parent = deepParent(f);
        std::string umlauts;  // 60 characters, 120 bytes
        for (int i = 0; i < 60; ++i)
            umlauts += "\xC3\xA4";

        for (const auto& name : {std::string(121, 'a'), std::string(255, 'a'), umlauts + "b"})
        {
            INFO("name of " << name.size() << " bytes");
            CHECK(f.errorOf("project.new", newParams(f.root(), name)) == "invalid_project_name");
            CHECK(f.errorOf("project.new", newParams(parent, name)) == "invalid_project_name");
        }
        // 120 bytes are not refused for their length; the folder decides whether they fit (path_too_long) or not.
        for (const auto& name : {std::string(120, 'a'), umlauts})
        {
            INFO("name of " << name.size() << " bytes");
            const auto code = f.errorOf("project.new", newParams(parent, name));
            CHECK((code.empty() || code == "path_too_long"));
            if (f.session->edit() != nullptr)
                f.run("project.close");
        }
    }
}
