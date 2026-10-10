// Crash recovery (M1-05, E41): opening a project whose autosave file is newer than the project file reports
// `recovery_available` (with both time stamps) in the result of project.open; project.restore_autosave (destructive)
// and project.discard_autosave decide it. An unreadable project file names the newest backup / autosave file.
// The crash is simulated by copying the project folder while the project is open (crashedProject in the fixture).
#include "project/project_fixture.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::project;
using tracklab::project::AutosaveResult;

juce::Time timeOf(const Json& stamp)
{
    REQUIRE(stamp.is_string());
    return juce::Time::fromISO8601(juce::String(stamp.get<std::string>()));
}

/** Within one second: ISO 8601 strings have a resolution of a second or a millisecond. */
bool sameInstant(const juce::Time& a, const juce::Time& b)
{
    return std::abs(a.toMilliseconds() - b.toMilliseconds()) < 1000;
}

/** An autosave of project `file` that is there after the project was closed (whatever closing does to it). */
void leaveAutosaveBehind(ProjectFixture& f, const juce::File& file)
{
    f.renameFirstTrack("Neu");
    REQUIRE(f.session->autosaveNow() == AutosaveResult::written);
    const auto autosaveBytes = bytesOf(autosaveFileOf(file));
    f.run("project.close", Json{{"discard", true}});
    REQUIRE(autosaveFileOf(file).replaceWithData(autosaveBytes.getData(), autosaveBytes.getSize()));
}

Json openCrashed(ProjectFixture& f, const CrashedProject& crashed)
{
    return f.run("project.open", Json{{"path", utf8(crashed.file)}});
}

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("opening a project with a newer autosave reports recovery_available with both time stamps")
    {
        ProjectFixture f;
        const auto crashed = crashedProject(f);

        const auto result = openCrashed(f, crashed);

        REQUIRE(result.contains("recovery_available"));
        const auto& recovery = result["recovery_available"];
        REQUIRE(recovery.is_object());
        CHECK(recovery.size() == 2);
        CHECK(sameInstant(timeOf(recovery["project_time"]), crashed.projectTime));
        CHECK(sameInstant(timeOf(recovery["autosave_time"]), crashed.autosaveTime));
        CHECK(result.size() == 5);  // the four of Info + recovery_available
        CHECK(result["path"].get<std::string>() == utf8(crashed.file));
    }

    TEST_CASE("the project is opened as the project file has it; the recovery is only offered, nothing is applied")
    {
        ProjectFixture f;
        const auto crashed = crashedProject(f);
        const auto projectBytes = bytesOf(crashed.file);
        const auto autosaveBytes = bytesOf(crashed.autosave);

        openCrashed(f, crashed);
        f.settleEdit();

        const auto tracks = te::getAudioTracks(f.edit());
        REQUIRE(tracks.size() == 3);
        CHECK(tracks[0]->getName() == "Gitarre");
        CHECK_FALSE(f.modified());
        REQUIRE(f.session->pendingRecovery().has_value());
        CHECK(sameInstant(f.session->pendingRecovery()->autosaveTime, crashed.autosaveTime));
        CHECK(sameInstant(f.session->pendingRecovery()->projectTime, crashed.projectTime));
        CHECK(sameBytes(bytesOf(crashed.file), projectBytes));
        CHECK(sameBytes(bytesOf(crashed.autosave), autosaveBytes));
        CHECK(f.info().size() == 4);  // get_info keeps its four keys
    }

    TEST_CASE("no autosave file: no recovery_available")
    {
        ProjectFixture f;
        const auto crashed = crashedProject(f);
        REQUIRE(crashed.autosave.deleteFile());

        const auto result = openCrashed(f, crashed);

        CHECK_FALSE(result.contains("recovery_available"));
        CHECK(result.size() == 4);
        CHECK_FALSE(f.session->pendingRecovery().has_value());
    }

    TEST_CASE("an autosave that is older than the project file is not offered")
    {
        ProjectFixture f;
        const auto crashed = crashedProject(f, "Muster", juce::Time(2026, 9, 9, 10, 5, 0, 0, true),
                                            juce::Time(2026, 9, 9, 10, 0, 0, 0, true));

        const auto result = openCrashed(f, crashed);

        CHECK_FALSE(result.contains("recovery_available"));
        CHECK_FALSE(f.session->pendingRecovery().has_value());
    }

    //==========================================================================
    // project.restore_autosave
    TEST_CASE("project.restore_autosave loads the autosave as project state; the project is modified, the file is not")
    {
        ProjectFixture f;
        const auto crashed = crashedProject(f);
        openCrashed(f, crashed);
        f.settleEdit();
        const auto projectBytes = bytesOf(crashed.file);

        const auto result = f.run("project.restore_autosave");

        CHECK(result.size() == 4);
        CHECK(result["path"].get<std::string>() == utf8(crashed.file));
        CHECK(result["modified"].get<bool>());
        CHECK(f.modified());
        CHECK(f.context.edit() == f.session->edit());
        const auto tracks = te::getAudioTracks(f.edit());
        REQUIRE(tracks.size() == 3);
        CHECK(tracks[0]->getName() == "Neu");
        CHECK(tracks[1]->getName() == "Bass");
        CHECK(sameBytes(bytesOf(crashed.file), projectBytes));
        CHECK(crashed.autosave.existsAsFile());  // stays until the next save
        CHECK_FALSE(f.session->pendingRecovery().has_value());
        CHECK_FALSE(f.edit().getUndoManager().canUndo());
        CHECK(f.errorOf("project.close") == "unsaved_changes");
    }

    TEST_CASE("saving after restore_autosave writes the recovered state and deletes the autosave")
    {
        ProjectFixture f;
        const auto crashed = crashedProject(f);
        openCrashed(f, crashed);
        f.settleEdit();
        f.run("project.restore_autosave");

        f.run("project.save");

        CHECK(trackNamesOfFile(crashed.file)[0] == "Neu");
        CHECK_FALSE(crashed.autosave.exists());
        CHECK_FALSE(f.modified());
    }

    TEST_CASE("project.restore_autosave without an autosave file is no_autosave, without a project no_edit")
    {
        ProjectFixture f;
        CHECK(f.errorOf("project.restore_autosave") == "no_edit");
        const auto file = f.newProject("Muster");
        const auto before = tracklab_test::snapshotOf(f.projectFolder("Muster"));

        CHECK(f.errorOf("project.restore_autosave") == "no_autosave");

        CHECK(tracklab_test::snapshotOf(f.projectFolder("Muster")) == before);
        CHECK_FALSE(f.modified());
    }

    //==========================================================================
    // project.discard_autosave
    TEST_CASE("project.discard_autosave deletes the autosave, keeps the project as opened and ends the offer")
    {
        ProjectFixture f;
        const auto crashed = crashedProject(f);
        openCrashed(f, crashed);
        f.settleEdit();
        const auto projectBytes = bytesOf(crashed.file);

        const auto result = f.run("project.discard_autosave");

        CHECK(result["discarded"].get<bool>());
        CHECK_FALSE(crashed.autosave.exists());
        CHECK(sameBytes(bytesOf(crashed.file), projectBytes));
        CHECK_FALSE(f.modified());
        CHECK(te::getAudioTracks(f.edit())[0]->getName() == "Gitarre");
        CHECK_FALSE(f.session->pendingRecovery().has_value());

        f.run("project.close");
        const auto again = f.run("project.open", Json{{"path", utf8(crashed.file)}});
        CHECK_FALSE(again.contains("recovery_available"));
    }

    TEST_CASE("the recovery is offered again when the project is closed and opened without a decision")
    {
        ProjectFixture f;
        const auto crashed = crashedProject(f);
        openCrashed(f, crashed);
        f.settleEdit();

        f.run("project.close");  // nothing changed: a clean close, but the offer was not decided

        CHECK(crashed.autosave.existsAsFile());
        const auto again = f.run("project.open", Json{{"path", utf8(crashed.file)}});
        CHECK(again.contains("recovery_available"));
    }

    TEST_CASE("the old autosave is not overwritten while the recovery is undecided")
    {
        ProjectFixture f;
        const auto crashed = crashedProject(f);
        openCrashed(f, crashed);
        f.settleEdit();
        const auto autosaveBytes = bytesOf(crashed.autosave);
        f.renameFirstTrack("Danach");

        CHECK(f.session->autosaveNow() == AutosaveResult::recoveryPending);
        CHECK(sameBytes(bytesOf(crashed.autosave), autosaveBytes));

        f.run("project.discard_autosave");
        CHECK(f.session->autosaveNow() == AutosaveResult::written);
        CHECK(trackNamesOfFile(crashed.autosave)[0] == "Danach");
    }

    TEST_CASE("after restore_autosave the autosave timer protects the recovered state again")
    {
        ProjectFixture f;
        const auto crashed = crashedProject(f);
        openCrashed(f, crashed);
        f.settleEdit();
        f.run("project.restore_autosave");
        f.renameFirstTrack("Weiter");

        CHECK(f.session->autosaveNow() == AutosaveResult::written);

        CHECK(trackNamesOfFile(crashed.autosave)[0] == "Weiter");
    }

    TEST_CASE("project.discard_autosave needs an open project")
    {
        ProjectFixture f;

        CHECK(f.errorOf("project.discard_autosave") == "no_edit");
    }

    TEST_CASE("the recovery commands take no parameters")
    {
        ProjectFixture f;
        const auto crashed = crashedProject(f);
        openCrashed(f, crashed);

        CHECK(f.errorOf("project.restore_autosave", Json{{"force", true}}) == "invalid_params");
        CHECK(f.errorOf("project.discard_autosave", Json{{"force", true}}) == "invalid_params");
        CHECK(crashed.autosave.existsAsFile());
    }

    //==========================================================================
    // An unreadable project file names the newest backup / autosave file.
    TEST_CASE("a corrupt project file: the error names the newest backup, not the older ones")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.useInjectedClock();
        f.addSampleTracks();
        for (int i = 0; i < 3; ++i)
        {
            f.renameFirstTrack("Version " + juce::String(i));
            f.saveAt(i * 60);
        }
        f.run("project.close");
        writeText(file, "kein Projekt");

        const auto result = f.tryRun("project.open", Json{{"path", utf8(file)}});

        CHECK_FALSE(result.ok);
        CHECK(result.error.code == "corrupt_project");
        CHECK(result.error.message.find("Muster.tracklab") != std::string::npos);
        CHECK(result.error.message.find(backupNameAt("Muster", 120)) != std::string::npos);
        CHECK(result.error.message.find(backupNameAt("Muster", 60)) == std::string::npos);
        CHECK(result.error.message.find(backupNameAt("Muster", 0)) == std::string::npos);
        CHECK(f.session->edit() == nullptr);
    }

    TEST_CASE("a corrupt project file with an autosave and no backup names the autosave")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.addSampleTracks();
        leaveAutosaveBehind(f, file);
        REQUIRE(backupNamesOf(file).empty());
        writeText(file, "kein Projekt");

        const auto result = f.tryRun("project.open", Json{{"path", utf8(file)}});

        CHECK(result.error.code == "corrupt_project");
        CHECK(result.error.message.find("Muster.tracklab.autosave") != std::string::npos);
    }

    TEST_CASE("a corrupt project file: the newer of autosave and newest backup is named")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.addSampleTracks();
        leaveAutosaveBehind(f, file);  // modification time: now
        const auto backups = backupsFolderOf(file);
        REQUIRE(backups.getChildFile("Muster.20200101-000000.tracklab").replaceWithText("<EDIT/>"));
        writeText(file, "kein Projekt");

        const auto autosaveNewer = f.tryRun("project.open", Json{{"path", utf8(file)}});
        CHECK(autosaveNewer.error.message.find("Muster.tracklab.autosave") != std::string::npos);

        REQUIRE(backups.getChildFile("Muster.20990101-000000.tracklab").replaceWithText("<EDIT/>"));
        const auto backupNewer = f.tryRun("project.open", Json{{"path", utf8(file)}});
        CHECK(backupNewer.error.message.find("Muster.20990101-000000.tracklab") != std::string::npos);
        CHECK(backupNewer.error.message.find("Muster.tracklab.autosave") == std::string::npos);
        CHECK(backupNewer.error.code == "corrupt_project");
    }

    TEST_CASE("a corrupt project file with neither backup nor autosave is still corrupt_project and untouched")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.run("project.close");
        writeText(file, "kein Projekt");
        const auto before = tracklab_test::snapshotOf(f.projectFolder("Muster"));

        const auto result = f.tryRun("project.open", Json{{"path", utf8(file)}});

        CHECK(result.error.code == "corrupt_project");
        CHECK(tracklab_test::snapshotOf(f.projectFolder("Muster")) == before);
    }
}
