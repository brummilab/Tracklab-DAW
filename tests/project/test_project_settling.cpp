// Changes right after create / open / save (M1-04, review round 1, findings 1 and 3). Tracktion's
// EditChangeResetterTimer resets the "changed" flag unconditionally 200 ms after an Edit was created, so a real change
// made before that must not be forgotten; and a move of a track against a track of another type is a change, however
// Tracktion sorts the tracks afterwards. The tests call project.new / project.open directly (not
// ProjectFixture::newProject, which lets the engine settle for 250 ms first) and change the project at once.
#include "project/project_fixture.h"

#include <string>

namespace
{

using namespace tracklab_test::project;

/** An undoable change of the project, made without any waiting. */
void changeAtOnce(te::Edit& e)
{
    e.state.setProperty("sampleNote", 1, &e.getUndoManager());
    settle(e);
}

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("a change right after project.new stays modified after Tracktion's resetter timer, and close refuses")
    {
        ProjectFixture f;
        f.run("project.new", Json{{"folder", utf8(f.root())}, {"name", "Muster"}});

        changeAtOnce(f.edit());
        CHECK(f.modified());
        pumpMessageLoop(1000);
        settle(f.edit());

        CHECK(f.modified());
        CHECK(f.edit().hasChangedSinceSaved());
        CHECK(f.errorOf("project.close") == "unsaved_changes");
        CHECK(f.session->edit() != nullptr);
    }

    TEST_CASE("a change right after project.open stays modified after Tracktion's resetter timer, and close refuses")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.run("project.close");
        f.run("project.open", Json{{"path", utf8(file)}});

        changeAtOnce(f.edit());
        CHECK(f.modified());
        pumpMessageLoop(1000);
        settle(f.edit());

        CHECK(f.modified());
        CHECK(f.edit().hasChangedSinceSaved());
        CHECK(f.errorOf("project.close") == "unsaved_changes");
    }

    TEST_CASE("a change made before the engine has attached its undo listener is seen by project.close")
    {
        ProjectFixture f;
        f.run("project.new", Json{{"folder", utf8(f.root())}, {"name", "Muster"}});

        f.edit().state.setProperty("sampleNote", 1, &f.edit().getUndoManager());  // no settle at all

        CHECK(f.errorOf("project.close") == "unsaved_changes");
    }

    TEST_CASE("a change right after project.new that is then saved leaves a clean project")
    {
        ProjectFixture f;
        f.run("project.new", Json{{"folder", utf8(f.root())}, {"name", "Muster"}});
        changeAtOnce(f.edit());

        f.run("project.save");
        pumpMessageLoop(1000);
        settle(f.edit());

        CHECK_FALSE(f.modified());
        CHECK(f.tryRun("project.close").ok);
    }

    TEST_CASE("moving an audio track and a folder track against each other right after a save is a change")
    {
        ProjectFixture f;
        f.newProject("Muster");
        auto& e = f.edit();
        e.ensureNumberOfAudioTracks(1);
        auto audio = te::getAudioTracks(e)[0];
        auto folder = e.insertNewFolderTrack(te::TrackInsertPoint(*audio, false), nullptr, false);
        REQUIRE(folder != nullptr);
        settle(e);
        ProjectFixture::letPluginTimersRunOut(e);
        f.run("project.save");

        const auto orderOf = [&e]
        {
            std::string ids;
            for (auto* track : te::getAllTracks(e))
                ids += track->itemID.toString().toStdString() + ",";
            return ids;
        };
        const auto before = orderOf();
        e.moveTrack(folder, te::TrackInsertPoint(nullptr, nullptr));  // the folder track in front of the audio track
        settle(e);
        REQUIRE(orderOf() != before);
        pumpMessageLoop(1000);
        settle(e);

        CHECK(f.modified());
        CHECK(f.errorOf("project.close") == "unsaved_changes");
    }
}
