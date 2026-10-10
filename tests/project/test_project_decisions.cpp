// M1-05, decisions of the lead after the first test round: the name limit at its boundary, what closing and saving do
// to the autosave, the error codes of the backup / autosave commands, and the temporary files of an interrupted autosave.
#include "project/path_length_helper.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::project;
using tracklab::project::AutosaveResult;

std::string umlauts(int count)
{
    std::string text;
    for (int i = 0; i < count; ++i)
        text += "\xC3\xA4";
    return text;
}

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("a project name of 121 bytes is invalid_project_name, whatever the path (see test_project_path_length)")
    {
        ProjectFixture f;

        CHECK(f.errorOf("project.new", Json{{"folder", utf8(f.root())}, {"name", std::string(121, 'b')}}) ==
              "invalid_project_name");
        CHECK(f.errorOf("project.new", Json{{"folder", utf8(f.root())}, {"name", umlauts(60) + "b"}}) ==
              "invalid_project_name");
        CHECK(f.projectFolder(std::string(121, 'b')).exists() == false);
    }

    TEST_CASE("the longest name that fits the path still has working backup, autosave and temporary file names")
    {
        ProjectFixture f;
        f.useInjectedClock();
        const auto file = f.newProject(std::string(std::min<size_t>(120, longestNameFitting(f.root())), 'z'));
        f.addSampleTracks();
        f.saveAt(0);
        f.renameFirstTrack("Neu");

        CHECK(f.session->autosaveNow() == AutosaveResult::written);
        f.saveAt(60);

        CHECK(backupNamesOf(file).size() == 2);
        CHECK_FALSE(autosaveFileOf(file).exists());
    }

    TEST_CASE("project.close with discard deletes the autosave")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.addSampleTracks();
        f.run("project.save");
        f.settleEdit();
        f.renameFirstTrack("Neu");
        REQUIRE(f.session->autosaveNow() == AutosaveResult::written);

        CHECK(f.errorOf("project.close") == "unsaved_changes");
        CHECK(autosaveFileOf(file).existsAsFile());  // a refused close deletes nothing

        f.run("project.close", Json{{"discard", true}});

        CHECK_FALSE(autosaveFileOf(file).exists());
        CHECK(trackNamesOfFile(file)[0] == "Gitarre");
    }

    TEST_CASE("project.close keeps the autosave of an undecided recovery, also with discard")
    {
        ProjectFixture f;
        const auto crashed = crashedProject(f);
        f.run("project.open", Json{{"path", utf8(crashed.file)}});
        f.settleEdit();
        const auto autosaveBytes = bytesOf(crashed.autosave);

        f.run("project.close", Json{{"discard", true}});

        CHECK(sameBytes(bytesOf(crashed.autosave), autosaveBytes));
        CHECK(f.run("project.open", Json{{"path", utf8(crashed.file)}}).contains("recovery_available"));
    }

    TEST_CASE("saving while a recovery is offered deletes the autosave and ends the offer")
    {
        ProjectFixture f;
        const auto crashed = crashedProject(f);
        f.run("project.open", Json{{"path", utf8(crashed.file)}});
        f.settleEdit();
        REQUIRE(f.session->pendingRecovery().has_value());

        f.run("project.save");

        CHECK_FALSE(crashed.autosave.exists());
        CHECK_FALSE(f.session->pendingRecovery().has_value());
        CHECK(trackNamesOfFile(crashed.file)[0] == "Gitarre");  // the project as opened, not the autosave
        f.run("project.close");
        CHECK_FALSE(f.run("project.open", Json{{"path", utf8(crashed.file)}}).contains("recovery_available"));
    }

    TEST_CASE("project.restore_backup takes only plain names of this project's backups")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.useInjectedClock();
        f.addSampleTracks();
        f.saveAt(0);
        f.settleEdit();
        REQUIRE(f.root().getChildFile("Geheim.tracklab").replaceWithText("<EDIT/>"));
        const auto before = tracklab_test::snapshotOf(f.projectFolder("Muster"));

        for (const auto* name : {"../Muster.tracklab", "Backups/x.tracklab", "a\\b.tracklab", "..", "", "/etc/passwd"})
        {
            INFO("name \"" << name << "\"");
            CHECK(f.errorOf("project.restore_backup", Json{{"name", name}}) == "invalid_params");
        }
        CHECK(f.errorOf("project.restore_backup", Json{{"name", utf8(f.root().getChildFile("Geheim.tracklab"))}}) ==
              "invalid_params");
        CHECK(f.errorOf("project.restore_backup", Json{{"name", "Muster.20000101-000000.tracklab"}}) ==
              "backup_not_found");
        CHECK(f.errorOf("project.restore_backup", Json{{"name", "Anderes.20000101-000000.tracklab"}}) ==
              "backup_not_found");

        CHECK(tracklab_test::snapshotOf(f.projectFolder("Muster")) == before);
        CHECK(file.existsAsFile());
    }

    TEST_CASE("project.restore_backup of an unreadable backup changes nothing, not even a safety backup")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.useInjectedClock();
        f.addSampleTracks();
        REQUIRE(backupsFolderOf(file).getChildFile("Muster.20200101-000000.tracklab").replaceWithText("kein Projekt"));
        f.renameFirstTrack("Ungespeichert");
        const auto before = backupNamesOf(file);

        CHECK(f.errorOf("project.restore_backup", Json{{"name", "Muster.20200101-000000.tracklab"}}) ==
              "corrupt_project");

        CHECK(backupNamesOf(file) == before);
        CHECK(te::getAudioTracks(f.edit())[0]->getName() == "Ungespeichert");
    }

    TEST_CASE("project.discard_autosave without an autosave file is no_autosave")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        const auto before = tracklab_test::snapshotOf(f.projectFolder("Muster"));

        CHECK(f.errorOf("project.discard_autosave") == "no_autosave");

        CHECK(tracklab_test::snapshotOf(f.projectFolder("Muster")) == before);
        CHECK(file.existsAsFile());
    }

    TEST_CASE("opening a project removes the temporary files of an interrupted autosave, and only those")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.run("project.close");
        const auto folder = f.projectFolder("Muster");
        REQUIRE(folder.getChildFile("Muster.tracklab_temp1f2e3d4c.autosave").replaceWithText("<EDIT>half"));
        REQUIRE(folder.getChildFile("Muster.tracklab_tempxyz.autosave").replaceWithText("keep"));
        REQUIRE(folder.getChildFile("Other.tracklab_temp1f2e.autosave").replaceWithText("keep"));

        f.run("project.open", Json{{"path", utf8(file)}});

        CHECK(fileNamesIn(folder) == std::vector<std::string>{"Muster.tracklab", "Muster.tracklab_tempxyz.autosave",
                                                              "Other.tracklab_temp1f2e.autosave"});
    }
}
