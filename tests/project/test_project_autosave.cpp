// Autosave (M1-05, E41): `<project>.tracklab.autosave` next to the project file, written by a message-thread timer only
// when the project changed, atomically like a save, never while the Edit is save-inhibited, removed by a clean save or
// close. The timer's work is reachable as ProjectSession::autosaveNow(), so that almost no test has to wait.
#include "project/project_fixture.h"

#include <chrono>
#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::project;
using tracklab::project::AutosaveResult;

/** A project that has content, is saved and settled (not modified). */
juce::File savedProject(ProjectFixture& f, const std::string& name = "Muster")
{
    const auto file = f.newProject(name);
    f.addSampleTracks();
    f.run("project.save");
    f.settleEdit();
    REQUIRE_FALSE(f.modified());
    return file;
}

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("the defaults are an autosave every 2 minutes and 10 backups")
    {
        ProjectFixture f;

        CHECK(f.session->autosaveInterval() == std::chrono::minutes(2));
        CHECK(f.session->maxBackups() == 10);
    }

    TEST_CASE("autosaveNow without an open project does nothing")
    {
        ProjectFixture f;

        CHECK(f.session->autosaveNow() == AutosaveResult::noProject);
    }

    TEST_CASE("a project that was not changed since the last save is not autosaved")
    {
        ProjectFixture f;
        const auto file = savedProject(f);
        WriteCounter writes(f);

        const auto result = f.session->autosaveNow();

        CHECK(result == AutosaveResult::notModified);
        CHECK(writes.autosaveWrites == 0);
        CHECK_FALSE(autosaveFileOf(file).exists());
    }

    TEST_CASE("a new project is not autosaved before something changed")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");

        CHECK(f.session->autosaveNow() == AutosaveResult::notModified);
        CHECK_FALSE(autosaveFileOf(file).exists());
    }

    TEST_CASE("a changed project is written to <project>.tracklab.autosave and the project file stays as it was")
    {
        ProjectFixture f;
        const auto file = savedProject(f);
        const auto savedBytes = bytesOf(file);
        f.renameFirstTrack("Neu");
        const auto undoBefore = f.edit().getUndoManager().getUndoDescriptions();

        const auto result = f.session->autosaveNow();

        CHECK(result == AutosaveResult::written);
        const auto autosave = file.getSiblingFile("Muster.tracklab.autosave");
        REQUIRE(autosave.existsAsFile());
        const auto xml = parseXml(autosave);
        REQUIRE(xml != nullptr);
        CHECK(xml->hasTagName("EDIT"));
        CHECK(xml->getIntAttribute(tracklab::project::formatVersionProperty) ==
              tracklab::project::currentFormatVersion);
        CHECK(trackNamesOfFile(autosave) == std::vector<std::string>{"Neu", "Bass", "Gesang"});
        CHECK(sameBytes(bytesOf(file), savedBytes));
        CHECK(trackNamesOfFile(file) == std::vector<std::string>{"Gitarre", "Bass", "Gesang"});
        CHECK(f.modified());  // an autosave is not a save
        CHECK(f.edit().getUndoManager().getUndoDescriptions() == undoBefore);
    }

    TEST_CASE("an autosave is not repeated until the project changes again")
    {
        ProjectFixture f;
        const auto file = savedProject(f);
        WriteCounter writes(f);
        f.renameFirstTrack("Neu");
        REQUIRE(f.session->autosaveNow() == AutosaveResult::written);

        CHECK(f.session->autosaveNow() == AutosaveResult::notModified);
        CHECK(writes.autosaveWrites == 1);

        f.renameFirstTrack("Noch neuer");
        CHECK(f.session->autosaveNow() == AutosaveResult::written);
        CHECK(writes.autosaveWrites == 2);
        CHECK(trackNamesOfFile(autosaveFileOf(file))[0] == "Noch neuer");
    }

    TEST_CASE("an autosave goes through the atomic write: temporary file next to the autosave, complete, then replaced")
    {
        ProjectFixture f;
        const auto file = savedProject(f);
        WriteCounter writes(f);
        f.renameFirstTrack("Neu");
        juce::MemoryBlock temporaryContent;
        bool temporaryExisted = false;
        f.session->setBeforeReplaceHook(
            [&](const juce::File& temporary, const juce::File& target)
            {
                writes.lastTemporary = temporary;
                writes.lastTarget = target;
                temporaryExisted = temporary.existsAsFile();
                temporaryContent = bytesOf(temporary);
                return true;
            });

        REQUIRE(f.session->autosaveNow() == AutosaveResult::written);

        CHECK(writes.lastTarget == autosaveFileOf(file));
        CHECK(writes.lastTemporary.getParentDirectory() == writes.lastTarget.getParentDirectory());
        CHECK(writes.lastTemporary != writes.lastTarget);
        CHECK(temporaryExisted);
        CHECK(temporaryContent.getSize() > 200);
        CHECK_FALSE(writes.lastTemporary.exists());
        CHECK(sameBytes(bytesOf(autosaveFileOf(file)), temporaryContent));
        CHECK(fileNamesIn(f.projectFolder("Muster")) ==
              std::vector<std::string>{"Muster.tracklab", "Muster.tracklab.autosave"});
    }

    TEST_CASE("an aborted autosave leaves the previous autosave byte-identical, no temporary file, project modified")
    {
        ProjectFixture f;
        const auto file = savedProject(f);
        WriteCounter writes(f);
        f.renameFirstTrack("Erste");
        REQUIRE(f.session->autosaveNow() == AutosaveResult::written);
        const auto autosaveBytes = bytesOf(autosaveFileOf(file));
        const auto projectBytes = bytesOf(file);
        f.renameFirstTrack("Zweite");
        writes.refuseAutosave = true;

        const auto result = f.session->autosaveNow();

        CHECK(result == AutosaveResult::failed);
        CHECK(sameBytes(bytesOf(autosaveFileOf(file)), autosaveBytes));
        CHECK(sameBytes(bytesOf(file), projectBytes));
        CHECK(fileNamesIn(f.projectFolder("Muster")) ==
              std::vector<std::string>{"Muster.tracklab", "Muster.tracklab.autosave"});
        CHECK(f.modified());

        writes.refuseAutosave = false;  // the failure is not remembered as "autosaved"
        CHECK(f.session->autosaveNow() == AutosaveResult::written);
        CHECK(trackNamesOfFile(autosaveFileOf(file))[0] == "Zweite");
    }

    TEST_CASE("an aborted first autosave creates no file at all")
    {
        ProjectFixture f;
        savedProject(f);
        WriteCounter writes(f);
        writes.refuseAutosave = true;
        f.renameFirstTrack("Neu");

        CHECK(f.session->autosaveNow() == AutosaveResult::failed);

        CHECK(fileNamesIn(f.projectFolder("Muster")) == std::vector<std::string>{"Muster.tracklab"});
        CHECK(writes.autosaveWrites == 1);  // it did try, through the atomic write

        writes.refuseAutosave = false;
        CHECK(f.session->autosaveNow() == AutosaveResult::written);
    }

    TEST_CASE("no autosave while the Edit is save-inhibited; the next attempt after it writes")
    {
        ProjectFixture f;
        const auto file = savedProject(f);
        WriteCounter writes(f);
        f.renameFirstTrack("Neu");

        {
            const te::Edit::SaveInhibitor inhibitor(f.edit());
            REQUIRE(f.edit().isSaveInhibited());

            CHECK(f.session->autosaveNow() == AutosaveResult::saveInhibited);
            CHECK(writes.autosaveWrites == 0);
            CHECK_FALSE(autosaveFileOf(file).exists());
        }

        CHECK(f.session->autosaveNow() == AutosaveResult::written);
        CHECK(autosaveFileOf(file).existsAsFile());
    }

    TEST_CASE("the autosave timer writes a changed project after the interval and leaves a saved one alone")
    {
        ProjectFixture f;
        const auto file = savedProject(f);
        WriteCounter writes(f);
        f.renameFirstTrack("Neu");

        // Default interval (2 minutes): nothing happens within a moment.
        pumpMessageLoop(300);
        CHECK(writes.autosaveWrites == 0);
        CHECK_FALSE(autosaveFileOf(file).exists());

        f.session->setAutosaveInterval(std::chrono::milliseconds(100));
        CHECK(f.session->autosaveInterval() == std::chrono::milliseconds(100));
        pumpMessageLoop(500);
        CHECK(writes.autosaveWrites >= 1);
        CHECK(autosaveFileOf(file).existsAsFile());
        CHECK(trackNamesOfFile(autosaveFileOf(file))[0] == "Neu");

        // A save removes the autosave; the timer keeps running but there is nothing to write.
        f.run("project.save");
        f.settleEdit();
        const int writesAfterSave = writes.autosaveWrites;
        pumpMessageLoop(400);
        CHECK(writes.autosaveWrites == writesAfterSave);
        CHECK_FALSE(autosaveFileOf(file).exists());

        // An interval <= 0 switches the timer off.
        f.session->setAutosaveInterval(std::chrono::milliseconds(0));
        f.renameFirstTrack("Aus");
        pumpMessageLoop(300);
        CHECK(writes.autosaveWrites == writesAfterSave);
    }

    TEST_CASE("a clean project.save deletes the autosave")
    {
        ProjectFixture f;
        const auto file = savedProject(f);
        f.renameFirstTrack("Neu");
        REQUIRE(f.session->autosaveNow() == AutosaveResult::written);
        REQUIRE(autosaveFileOf(file).existsAsFile());

        f.run("project.save");

        CHECK_FALSE(autosaveFileOf(file).exists());
        CHECK(trackNamesOfFile(file)[0] == "Neu");
    }

    TEST_CASE("a failed save keeps the autosave")
    {
        ProjectFixture f;
        const auto file = savedProject(f);
        f.renameFirstTrack("Neu");
        REQUIRE(f.session->autosaveNow() == AutosaveResult::written);
        f.session->setBeforeReplaceHook([](const juce::File&, const juce::File&) { return false; });

        CHECK(f.errorOf("project.save") == "save_failed");

        CHECK(autosaveFileOf(file).existsAsFile());
    }

    TEST_CASE("closing a project without changes deletes a leftover autosave")
    {
        ProjectFixture f;
        const auto file = savedProject(f);
        CHECK_FALSE(autosaveFileOf(file).exists());
        REQUIRE(autosaveFileOf(file).replaceWithText("<EDIT/>"));  // what an earlier run left behind

        f.run("project.close");

        CHECK_FALSE(autosaveFileOf(file).exists());
        CHECK(file.existsAsFile());
    }

    TEST_CASE("project.save_as continues in the new project: its autosave is written next to the new file only")
    {
        ProjectFixture f;
        const auto first = savedProject(f);
        const auto second = f.projectFile("Zweites");
        f.run("project.save_as", Json{{"folder", utf8(f.root())}, {"name", "Zweites"}});
        f.settleEdit();
        f.renameFirstTrack("Neu");

        CHECK(f.session->autosaveNow() == AutosaveResult::written);

        CHECK(autosaveFileOf(second).existsAsFile());
        CHECK_FALSE(autosaveFileOf(first).exists());
    }
}
