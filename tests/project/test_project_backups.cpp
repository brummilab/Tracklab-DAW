// Rotating backups (M1-05, E41): every save puts a version of the project into `Backups/` as
// `<project>.<YYYYMMDD-HHMMSS>.tracklab` (time of the injected clock, local time), at most `maxBackups` (default 10),
// the oldest by the time in the name go first; project.list_backups (readOnly) and project.restore_backup
// (destructive, takes a backup of the current state first).
#include "project/project_fixture.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::project;

/** A saved project with content and the injected clock, settled. */
juce::File projectWithClock(ProjectFixture& f, const std::string& name = "Muster")
{
    const auto file = f.newProject(name);
    f.useInjectedClock();
    f.addSampleContent();
    return file;
}

/** The `name` fields of the backups in the result of project.list_backups, in the order of the result. */
std::vector<std::string> listedNames(const Json& listing)
{
    std::vector<std::string> names;
    for (const auto& entry : listing.at("backups"))
        names.push_back(entry.at("name").get<std::string>());
    return names;
}

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("every save puts one backup named <project>.<YYYYMMDD-HHMMSS>.tracklab into Backups")
    {
        ProjectFixture f;
        const auto file = projectWithClock(f);
        REQUIRE(backupNamesOf(file).empty());

        f.saveAt(0);
        CHECK(backupNamesOf(file) == std::vector<std::string>{"Muster.20261009-123456.tracklab"});

        f.renameFirstTrack("Zwei");
        f.saveAt(61);
        CHECK(backupNamesOf(file) ==
              std::vector<std::string>{"Muster.20261009-123456.tracklab", "Muster.20261009-123557.tracklab"});
    }

    TEST_CASE("the backups are complete project files; the newest one is the file as it was before the last save")
    {
        ProjectFixture f;
        const auto file = projectWithClock(f);
        f.saveAt(0);
        f.renameFirstTrack("Zwei");
        f.saveAt(60);
        const auto beforeThirdSave = bytesOf(file);
        f.renameFirstTrack("Drei");

        f.saveAt(120);

        const auto newest = backupsFolderOf(file).getChildFile(backupNameAt("Muster", 120));
        REQUIRE(newest.existsAsFile());
        CHECK(sameBytes(bytesOf(newest), beforeThirdSave));
        CHECK(trackNamesOfFile(newest)[0] == "Zwei");
        CHECK(trackNamesOfFile(file)[0] == "Drei");
        for (const auto& name : backupNamesOf(file))
            CHECK(parseXml(backupsFolderOf(file).getChildFile(juce::String(name))) != nullptr);
    }

    TEST_CASE("after 12 saves exactly the 10 newest backups are left, the oldest were deleted first")
    {
        ProjectFixture f;
        const auto file = projectWithClock(f);

        for (int i = 0; i < 12; ++i)
        {
            f.renameFirstTrack("Version " + juce::String(i));
            f.saveAt(i * 60);
            CHECK(backupNamesOf(file).size() == static_cast<size_t>(std::min(i + 1, 10)));
        }

        std::vector<std::string> expected;
        for (int i = 2; i < 12; ++i)
            expected.push_back(backupNameAt("Muster", i * 60));
        CHECK(backupNamesOf(file) == expected);
    }

    TEST_CASE("the oldest is chosen by the time in the name; other files in Backups are neither counted nor deleted")
    {
        ProjectFixture f;
        const auto file = projectWithClock(f);
        const auto backups = backupsFolderOf(file);
        for (int i = 0; i < 10; ++i)
            REQUIRE(backups.getChildFile(juce::String::formatted("Muster.20200101-00000%d.tracklab", i))
                        .replaceWithText("<EDIT/>"));
        REQUIRE(backups.getChildFile("Anderes.20190101-000000.tracklab").replaceWithText("<EDIT/>"));
        REQUIRE(backups.getChildFile("Notiz.txt").replaceWithText("Muster"));

        f.saveAt(0);

        CHECK_FALSE(backups.getChildFile("Muster.20200101-000000.tracklab").exists());
        CHECK(backups.getChildFile("Muster.20200101-000001.tracklab").existsAsFile());
        CHECK(backups.getChildFile("Muster.20200101-000009.tracklab").existsAsFile());
        CHECK(backups.getChildFile(juce::String(backupNameAt("Muster"))).existsAsFile());
        CHECK(backups.getChildFile("Anderes.20190101-000000.tracklab").existsAsFile());
        CHECK(backups.getChildFile("Notiz.txt").existsAsFile());
        CHECK(backupNamesOf(file).size() == 12);  // 10 of the project + the two foreign files
    }

    TEST_CASE("the number of backups is adjustable")
    {
        ProjectFixture f;
        const auto file = projectWithClock(f);
        f.session->setMaxBackups(3);
        CHECK(f.session->maxBackups() == 3);

        for (int i = 0; i < 5; ++i)
        {
            f.renameFirstTrack("Version " + juce::String(i));
            f.saveAt(i * 60);
        }

        CHECK(backupNamesOf(file) == std::vector<std::string>{backupNameAt("Muster", 120), backupNameAt("Muster", 180),
                                                              backupNameAt("Muster", 240)});
    }

    TEST_CASE("a failed save adds no backup and removes none")
    {
        ProjectFixture f;
        const auto file = projectWithClock(f);
        f.session->setMaxBackups(2);
        f.renameFirstTrack("Eins");
        f.saveAt(0);
        f.renameFirstTrack("Zwei");
        f.saveAt(60);
        const auto before = backupNamesOf(file);
        REQUIRE(before.size() == 2);
        f.session->setBeforeReplaceHook([](const juce::File&, const juce::File&) { return false; });
        f.renameFirstTrack("Drei");

        CHECK(f.errorOf("project.save") == "save_failed");

        CHECK(backupNamesOf(file) == before);
    }

    TEST_CASE("a missing Backups folder is created by the next save")
    {
        ProjectFixture f;
        const auto file = projectWithClock(f);
        REQUIRE(backupsFolderOf(file).deleteRecursively());

        f.saveAt(0);

        CHECK(backupNamesOf(file) == std::vector<std::string>{backupNameAt("Muster")});
    }

    TEST_CASE("two saves within one second neither fail nor leave more than the allowed backups")
    {
        ProjectFixture f;
        const auto file = projectWithClock(f);

        f.saveAt(0);
        f.renameFirstTrack("Zwei");
        const auto second = f.tryRun("project.save");

        CHECK(second.ok);
        const auto names = backupNamesOf(file);
        CHECK_FALSE(names.empty());
        CHECK(names.size() <= 2);
        CHECK(fileNamesIn(f.projectFolder("Muster")) == std::vector<std::string>{"Muster.tracklab"});
    }

    TEST_CASE("without an injected clock the backup is named after the current time")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.addSampleContent();
        const auto before = juce::Time::getCurrentTime();

        f.run("project.save");

        const auto names = backupNamesOf(file);
        REQUIRE(names.size() == 1);
        const auto stamp =
            juce::String(names[0]).fromFirstOccurrenceOf(".", false, false).upToFirstOccurrenceOf(".", false, false);
        const auto expectedDay = before.formatted("%Y%m%d");
        CHECK(stamp.startsWith(expectedDay));
        CHECK(stamp.length() == 15);
    }

    //==========================================================================
    // project.list_backups
    TEST_CASE("project.list_backups lists the backups newest first with name, time and size")
    {
        ProjectFixture f;
        const auto file = projectWithClock(f);
        for (int i = 0; i < 3; ++i)
        {
            f.renameFirstTrack("Version " + juce::String(i));
            f.saveAt(i * 60);
        }

        const auto listing = f.run("project.list_backups");

        CHECK(listedNames(listing) == std::vector<std::string>{backupNameAt("Muster", 120), backupNameAt("Muster", 60),
                                                               backupNameAt("Muster")});
        for (const auto& entry : listing["backups"])
        {
            const auto name = entry["name"].get<std::string>();
            INFO("backup " << name);
            CHECK(entry["size_bytes"].get<juce::int64>() ==
                  backupsFolderOf(file).getChildFile(juce::String(name)).getSize());
            REQUIRE(entry["time"].is_string());
        }
        const auto newestTime = juce::Time::fromISO8601(juce::String(listing["backups"][0]["time"].get<std::string>()));
        CHECK(std::abs(newestTime.toMilliseconds() - clockTime(120).toMilliseconds()) < 1500);
    }

    TEST_CASE(
        "project.list_backups of a project without backups is an empty list; other projects' files are not listed")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        CHECK(f.run("project.list_backups").at("backups").empty());

        REQUIRE(backupsFolderOf(file).getChildFile("Anderes.20190101-000000.tracklab").replaceWithText("<EDIT/>"));
        REQUIRE(backupsFolderOf(file).getChildFile("Notiz.txt").replaceWithText("x"));

        CHECK(f.run("project.list_backups").at("backups").empty());
    }

    TEST_CASE("project.list_backups needs an open project and changes nothing")
    {
        ProjectFixture f;
        CHECK(f.errorOf("project.list_backups") == "no_edit");
        projectWithClock(f);
        f.saveAt(0);
        f.settleEdit();
        const auto before = tracklab_test::snapshotOf(f.projectFolder("Muster"));

        f.run("project.list_backups");

        CHECK(tracklab_test::snapshotOf(f.projectFolder("Muster")) == before);
        CHECK_FALSE(f.modified());
    }

    //==========================================================================
    // project.restore_backup
    TEST_CASE("project.restore_backup loads the backup as the project state and marks the project modified")
    {
        ProjectFixture f;
        const auto file = projectWithClock(f);
        f.renameFirstTrack("Eins");
        f.saveAt(0);
        f.renameFirstTrack("Zwei");
        f.saveAt(60);
        const auto backupName = listedNames(f.run("project.list_backups")).back();  // the oldest
        const auto backupFile = backupsFolderOf(file).getChildFile(juce::String(backupName));
        const auto wanted = trackNamesOfFile(backupFile);
        f.renameFirstTrack("Ungespeichert");
        f.clockNow = clockTime(120);
        const auto projectBytes = bytesOf(file);

        const auto result = f.run("project.restore_backup", Json{{"name", backupName}});

        CHECK(result["path"].get<std::string>() == utf8(file));
        CHECK(result["modified"].get<bool>());
        CHECK(result.size() == 4);
        CHECK(f.modified());
        CHECK(f.context.edit() == f.session->edit());
        te::Edit& edit = f.edit();
        const auto tracks = te::getAudioTracks(edit);
        REQUIRE(tracks.size() == static_cast<int>(wanted.size()));
        for (int i = 0; i < tracks.size(); ++i)
            CHECK(tracks[i]->getName().toStdString() == wanted[static_cast<size_t>(i)]);
        CHECK(wanted[0] != "Ungespeichert");
        CHECK(sameBytes(bytesOf(file), projectBytes));  // the project file on disk is only replaced by a save
        CHECK(backupFile.existsAsFile());               // the backup is not consumed
    }

    TEST_CASE("project.restore_backup first backs up the current state, the unsaved changes included")
    {
        ProjectFixture f;
        const auto file = projectWithClock(f);
        f.renameFirstTrack("Eins");
        f.saveAt(0);
        const auto older = listedNames(f.run("project.list_backups")).front();
        f.renameFirstTrack("Ungespeichert");
        f.clockNow = clockTime(300);

        f.run("project.restore_backup", Json{{"name", older}});

        const auto names = backupNamesOf(file);
        REQUIRE(names.size() == 2);
        const auto safety = backupsFolderOf(file).getChildFile(juce::String(backupNameAt("Muster", 300)));
        REQUIRE(safety.existsAsFile());
        CHECK(trackNamesOfFile(safety)[0] == "Ungespeichert");
        CHECK(listedNames(f.run("project.list_backups")).front() == backupNameAt("Muster", 300));
    }

    TEST_CASE("the project can be saved after restoring a backup: the file has the restored state")
    {
        ProjectFixture f;
        const auto file = projectWithClock(f);
        f.renameFirstTrack("Eins");
        f.saveAt(0);
        f.renameFirstTrack("Zwei");
        f.saveAt(60);
        const auto oldest = listedNames(f.run("project.list_backups")).back();
        const auto wanted = trackNamesOfFile(backupsFolderOf(file).getChildFile(juce::String(oldest)));
        f.run("project.restore_backup", Json{{"name", oldest}});
        f.clockNow = clockTime(120);

        f.run("project.save");

        CHECK(trackNamesOfFile(file) == wanted);
        CHECK_FALSE(f.modified());
    }

    TEST_CASE("project.restore_backup works while the project has unsaved changes (it is destructive, not guarded)")
    {
        ProjectFixture f;
        projectWithClock(f);
        f.saveAt(0);
        const auto name = listedNames(f.run("project.list_backups")).front();
        f.renameFirstTrack("Ungespeichert");
        REQUIRE(f.modified());
        f.clockNow = clockTime(30);

        CHECK(f.tryRun("project.restore_backup", Json{{"name", name}}).ok);
    }

    TEST_CASE("project.restore_backup refuses an unknown name, a path and a missing project; nothing changes")
    {
        ProjectFixture f;
        CHECK(f.errorOf("project.restore_backup", Json{{"name", "Muster.20261009-123456.tracklab"}}) == "no_edit");
        const auto file = projectWithClock(f);
        f.saveAt(0);
        f.settleEdit();
        const auto secret = f.root().getChildFile("Geheim.tracklab");
        REQUIRE(file.copyFileTo(secret));
        const auto before = tracklab_test::snapshotOf(f.projectFolder("Muster"));
        const auto* edit = f.session->edit();

        CHECK(f.errorOf("project.restore_backup", Json{{"name", "Muster.20000101-000000.tracklab"}}) ==
              "backup_not_found");
        for (const auto* escape : {"../Muster.tracklab", "../../Geheim.tracklab", "Backups/../Muster.tracklab"})
        {
            INFO("name " << escape);
            const auto code = f.errorOf("project.restore_backup", Json{{"name", escape}});
            CHECK((code == "backup_not_found" || code == "invalid_params"));
        }
        const auto absolute = f.errorOf("project.restore_backup", Json{{"name", utf8(secret)}});
        CHECK((absolute == "backup_not_found" || absolute == "invalid_params"));

        CHECK(tracklab_test::snapshotOf(f.projectFolder("Muster")) == before);
        CHECK(f.session->edit() == edit);
        CHECK_FALSE(f.modified());
    }

    TEST_CASE("the backup commands take their parameters strictly")
    {
        ProjectFixture f;
        projectWithClock(f);

        CHECK(f.errorOf("project.list_backups", Json{{"all", true}}) == "invalid_params");
        CHECK(f.errorOf("project.restore_backup") == "invalid_params");
        CHECK(f.errorOf("project.restore_backup", Json{{"name", 7}}) == "invalid_params");
        CHECK(f.errorOf("project.restore_backup", Json{{"name", "x"}, {"force", true}}) == "invalid_params");
    }
}
