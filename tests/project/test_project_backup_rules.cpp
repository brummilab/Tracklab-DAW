// M1-05, review round 1: rules of the backups that the first tests left open. A restore never rotates its own backup
// away, two backups in the same second get a counter suffix instead of overwriting each other, `maxBackups <= 0`
// switches the backups off, and interrupted writes in Backups/ are cleaned up when the project is opened.
#include "project/project_fixture.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::project;

/** A new project with the injected clock and the three sample tracks. */
juce::File projectWithClock(ProjectFixture& f)
{
    const auto file = f.newProject("Muster");
    f.useInjectedClock();
    f.addSampleTracks();
    return file;
}

std::string counted(int seconds, int counter)
{
    const auto plain = backupNameAt("Muster", seconds);
    return plain.substr(0, plain.size() - std::string(".tracklab").size()) + "-" + std::to_string(counter) +
           ".tracklab";
}

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
    TEST_CASE("restoring the oldest of 10 backups keeps it: the rotation after the safety backup removes another one")
    {
        ProjectFixture f;
        const auto file = projectWithClock(f);
        for (int i = 0; i <= 10; ++i)  // 11 saves: the backups of saves 1..10 are left, save 0's is rotated away
        {
            f.renameFirstTrack("Version " + juce::String(i));
            f.saveAt(i * 60);
        }
        REQUIRE(backupNamesOf(file).size() == 10);
        const auto oldest = backupNameAt("Muster", 60);
        REQUIRE(listedNames(f.run("project.list_backups")).back() == oldest);
        const auto wanted = trackNamesOfFile(backupsFolderOf(file).getChildFile(juce::String(oldest)));
        REQUIRE(wanted[0] == "Version 0");
        f.renameFirstTrack("Ungespeichert");
        f.clockNow = clockTime(660);

        f.run("project.restore_backup", Json{{"name", oldest}});

        const auto names = backupNamesOf(file);
        CHECK(names.size() == 10);
        CHECK(backupsFolderOf(file).getChildFile(juce::String(oldest)).existsAsFile());  // the chosen one survives
        CHECK(backupsFolderOf(file).getChildFile(juce::String(backupNameAt("Muster", 660))).existsAsFile());  // safety
        CHECK_FALSE(
            backupsFolderOf(file).getChildFile(juce::String(backupNameAt("Muster", 120))).exists());  // next oldest
        CHECK(te::getAudioTracks(f.edit())[0]->getName().toStdString() == wanted[0]);
        CHECK(f.modified());
        CHECK(trackNamesOfFile(backupsFolderOf(file).getChildFile(juce::String(backupNameAt("Muster", 660))))[0] ==
              "Ungespeichert");
    }

    TEST_CASE("two backups in the same second get a counter suffix and nothing is overwritten")
    {
        ProjectFixture f;
        const auto file = projectWithClock(f);
        f.renameFirstTrack("Eins");
        f.saveAt(0);
        const auto first = bytesOf(backupsFolderOf(file).getChildFile(juce::String(backupNameAt("Muster"))));
        f.renameFirstTrack("Zwei");
        f.saveAt(0);
        f.renameFirstTrack("Drei");
        f.saveAt(0);

        // (the names are sorted as text: "-" sorts before ".")
        CHECK(backupNamesOf(file) == std::vector<std::string>{counted(0, 2), counted(0, 3), backupNameAt("Muster")});
        CHECK(sameBytes(bytesOf(backupsFolderOf(file).getChildFile(juce::String(backupNameAt("Muster")))), first));
        CHECK(trackNamesOfFile(backupsFolderOf(file).getChildFile(juce::String(counted(0, 2))))[0] == "Eins");
        CHECK(trackNamesOfFile(backupsFolderOf(file).getChildFile(juce::String(counted(0, 3))))[0] == "Zwei");
        CHECK(listedNames(f.run("project.list_backups")) ==
              std::vector<std::string>{counted(0, 3), counted(0, 2), backupNameAt("Muster")});
    }

    TEST_CASE("the rotation counts and removes backups with a counter suffix, the oldest first")
    {
        ProjectFixture f;
        const auto file = projectWithClock(f);
        f.session->setMaxBackups(2);
        for (int i = 0; i < 4; ++i)
        {
            f.renameFirstTrack("Version " + juce::String(i));
            f.saveAt(0);  // all in one second
        }

        CHECK(backupNamesOf(file) == std::vector<std::string>{counted(0, 3), counted(0, 4)});

        f.renameFirstTrack("Danach");
        f.saveAt(30);  // a later second is newer than every counter of the earlier one
        CHECK(backupNamesOf(file) == std::vector<std::string>{counted(0, 4), backupNameAt("Muster", 30)});
    }

    TEST_CASE("restoring in the same second as the backup keeps the chosen backup byte-identical")
    {
        ProjectFixture f;
        const auto file = projectWithClock(f);
        f.renameFirstTrack("Eins");
        f.saveAt(0);
        f.renameFirstTrack("Zwei");
        f.saveAt(60);
        const auto chosen = backupNameAt("Muster", 60);
        const auto chosenFile = backupsFolderOf(file).getChildFile(juce::String(chosen));
        const auto chosenBytes = bytesOf(chosenFile);
        f.renameFirstTrack("Ungespeichert");  // the clock still says second 60

        f.run("project.restore_backup", Json{{"name", chosen}});

        CHECK(sameBytes(bytesOf(chosenFile), chosenBytes));
        CHECK(backupsFolderOf(file).getChildFile(juce::String(counted(60, 2))).existsAsFile());  // the safety backup
        CHECK(trackNamesOfFile(backupsFolderOf(file).getChildFile(juce::String(counted(60, 2))))[0] == "Ungespeichert");
    }

    TEST_CASE("maxBackups <= 0 switches the backups off: none is made, none is deleted, saving still works")
    {
        for (const int count : {0, -1})
        {
            INFO("maxBackups " << count);
            ProjectFixture f;
            const auto file = projectWithClock(f);
            const auto old = backupsFolderOf(file).getChildFile("Muster.20200101-000000.tracklab");
            REQUIRE(old.replaceWithText("<EDIT/>"));
            f.session->setMaxBackups(count);
            CHECK(f.session->maxBackups() == count);

            f.renameFirstTrack("Eins");
            f.saveAt(0);
            f.renameFirstTrack("Zwei");
            f.saveAt(60);

            CHECK(backupNamesOf(file) == std::vector<std::string>{"Muster.20200101-000000.tracklab"});
            CHECK(trackNamesOfFile(file)[0] == "Zwei");
            CHECK_FALSE(f.modified());
        }
    }

    TEST_CASE("opening a project removes the temporary files of an interrupted write in Backups, and only those")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.run("project.close");
        const auto backups = backupsFolderOf(file);
        const std::vector<std::string> leftovers = {"Muster.20200101-000000_temp1f2e3d4c.tracklab",
                                                    "Muster.20200101-000000-2_temp0a9b.tracklab"};
        const std::vector<std::string> keep = {
            "Muster.20200101-000000.tracklab",           "Muster.20200101-000000-2.tracklab",
            "Muster.20200101-000000_tempxyz.tracklab",   "Muster_temp1f2e.tracklab",
            "Anderes.20200101-000000_temp1f2e.tracklab", "Notiz_temp1f2e.txt"};
        for (const auto& name : leftovers)
            REQUIRE(backups.getChildFile(juce::String(name)).replaceWithText("<EDIT>half"));
        for (const auto& name : keep)
            REQUIRE(backups.getChildFile(juce::String(name)).replaceWithText("bleibt"));

        f.run("project.open", Json{{"path", utf8(file)}});

        auto expected = keep;
        std::sort(expected.begin(), expected.end());
        CHECK(backupNamesOf(file) == expected);
    }
}
