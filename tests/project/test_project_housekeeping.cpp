// Housekeeping of the project folder (M1-05, hints of review M1-04): temporary files of an interrupted save are removed
// when a project is opened, and a project name that is too long for the derived file names (backup, autosave, temporary
// file) is invalid_project_name instead of a save_failed later.
#include "project/project_fixture.h"

#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::project;
using tracklab::project::AutosaveResult;

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("opening a project removes the temporary files of an interrupted save")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.run("project.close");
        const auto folder = f.projectFolder("Muster");
        REQUIRE(folder.getChildFile("Muster_temp1f2e3d.tracklab").replaceWithText("<EDIT>half"));
        REQUIRE(folder.getChildFile("Muster_temp0a9b.tracklab").replaceWithText(""));
        REQUIRE(folder.getChildFile("Notizen.txt").replaceWithText("bleibt"));
        const auto projectBytes = bytesOf(file);

        f.run("project.open", Json{{"path", utf8(file)}});

        CHECK(fileNamesIn(folder) == std::vector<std::string>{"Muster.tracklab", "Notizen.txt"});
        CHECK(sameBytes(bytesOf(file), projectBytes));
    }

    TEST_CASE("a project without leftovers is opened without any change in its folder")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.run("project.close");
        const auto before = tracklab_test::snapshotOf(f.projectFolder("Muster"));

        f.run("project.open", Json{{"path", utf8(file)}});

        CHECK(tracklab_test::snapshotOf(f.projectFolder("Muster")) == before);
    }

    TEST_CASE("a project name longer than the derived file names allow is invalid_project_name; nothing is created")
    {
        ProjectFixture f;
        const std::string longest255(255, 'a');
        const std::string long220(220, 'a');
        std::string umlauts260;  // 130 characters, 260 bytes in UTF-8
        for (int i = 0; i < 130; ++i)
            umlauts260 += "\xC3\xA4";

        for (const auto& name : {longest255, long220, umlauts260})
        {
            INFO("name of " << name.size() << " bytes");
            CHECK(f.errorOf("project.new", Json{{"folder", utf8(f.root())}, {"name", name}}) == "invalid_project_name");
        }

        CHECK(f.root().findChildFiles(juce::File::findFilesAndDirectories, false).isEmpty());
    }

    TEST_CASE("project.save_as with a too long name is invalid_project_name, not save_failed")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        const auto before = tracklab_test::snapshotOf(f.root());

        const auto result =
            f.tryRun("project.save_as", Json{{"folder", utf8(f.root())}, {"name", std::string(220, 'b')}});

        CHECK(result.error.code == "invalid_project_name");
        CHECK(tracklab_test::snapshotOf(f.root()) == before);
        CHECK(f.info()["path"].get<std::string>() == utf8(file));
    }

    TEST_CASE("a project with a 100 byte name works completely: save, backup, autosave, recovery file, reopen")
    {
        ProjectFixture f;
        f.useInjectedClock();  // two saves within a second would share one backup name
        const std::string name(100, 'n');
        const auto file = f.newProject(name);
        f.addSampleContent();
        f.saveAt(0);
        f.renameFirstTrack("Neu");

        CHECK(f.session->autosaveNow() == AutosaveResult::written);
        CHECK(autosaveFileOf(file).existsAsFile());
        f.saveAt(60);

        CHECK(backupNamesOf(file).size() == 2);
        f.run("project.close");
        CHECK(f.tryRun("project.open", Json{{"path", utf8(file)}}).ok);

        // the same with 50 umlauts: 100 bytes
        f.run("project.close");
        std::string umlauts;
        for (int i = 0; i < 50; ++i)
            umlauts += "\xC3\xA4";
        const auto second = f.newProject(umlauts);
        f.addSampleContent();
        f.saveAt(120);
        f.renameFirstTrack("Neu");
        CHECK(f.session->autosaveNow() == AutosaveResult::written);
        f.saveAt(180);
        CHECK(backupNamesOf(second).size() == 2);
    }
}
