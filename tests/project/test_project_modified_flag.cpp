// The "modified" flag after a save (M1-04, lead decision 2): Tracktion marks an Edit "changed" up to 500 ms after the
// last plugin change (Edit::PluginChangeTimer, private and not cancellable), also if the project was saved in between.
// ProjectSession looks at the flag once more after every save and open and clears what is only that leftover. These tests
// save immediately after a change that touches plugins, without waiting for Tracktion's timer first (the other tests wait,
// see ProjectFixture::letPluginTimersRunOut), and then wait longer than the timer.
#include "project/project_fixture.h"

#include <string>

namespace
{

using namespace tracklab_test::project;

/** Tracks with names and a tempo, but no waiting: the volume plugins of the new tracks are "changed" just now. */
void addContentWithoutWaiting(ProjectFixture& f)
{
    auto& e = f.edit();
    e.ensureNumberOfAudioTracks(3);
    auto tracks = te::getAudioTracks(e);
    REQUIRE(tracks.size() >= 3);
    tracks[0]->setName("Gitarre");
    tracks[1]->setName("Bass");
    tracks[2]->setName("Gesang");
    e.tempoSequence.getTempo(0)->setBpm(97.0);
    settle(e);
}

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("a project saved right after plugin changes stays unmodified after Tracktion's plugin timer has run")
    {
        ProjectFixture f;
        f.newProject("Muster");
        addContentWithoutWaiting(f);
        REQUIRE(f.modified());

        const auto result = f.run("project.save");
        CHECK_FALSE(result["modified"].get<bool>());
        CHECK_FALSE(f.modified());

        pumpMessageLoop(1000);  // Tracktion's timer (500 ms) and the second look of the session (650 ms) are over
        settle(f.edit());

        CHECK_FALSE(f.modified());
        CHECK_FALSE(f.edit().hasChangedSinceSaved());
        CHECK(f.tryRun("project.close").ok);  // no unsaved_changes
    }

    TEST_CASE("save_as right after plugin changes stays unmodified as well")
    {
        ProjectFixture f;
        f.newProject("Erstes");
        addContentWithoutWaiting(f);

        f.run("project.save_as", Json{{"folder", utf8(f.root())}, {"name", "Zweites"}});
        pumpMessageLoop(1000);
        settle(f.edit());

        CHECK_FALSE(f.modified());
    }

    TEST_CASE("a real change made after the save is not lost to the second look")
    {
        ProjectFixture f;
        f.newProject("Muster");
        addContentWithoutWaiting(f);
        f.run("project.save");

        f.makeModified();  // an undoable change right after the save, inside the window of the second look
        pumpMessageLoop(1000);
        settle(f.edit());

        CHECK(f.modified());
        CHECK(f.errorOf("project.close") == "unsaved_changes");
    }

    TEST_CASE("a new and an opened project stay unmodified after Tracktion's timers")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        pumpMessageLoop(1000);
        settle(f.edit());
        CHECK_FALSE(f.modified());

        f.run("project.close");
        f.run("project.open", Json{{"path", utf8(file)}});
        pumpMessageLoop(1000);
        settle(f.edit());

        CHECK_FALSE(f.modified());
        CHECK(f.tryRun("project.close").ok);
    }

    TEST_CASE("the session can be destroyed while the second look is still pending")
    {
        ProjectFixture f;
        f.newProject("Muster");
        addContentWithoutWaiting(f);
        f.run("project.save");

        f.session.reset();  // the timer must not touch the destroyed Edit
        pumpMessageLoop(900);

        CHECK(f.context.edit() == nullptr);
    }

    TEST_CASE("closing the project while the second look is pending is safe")
    {
        ProjectFixture f;
        f.newProject("Muster");
        addContentWithoutWaiting(f);
        f.run("project.save");
        f.run("project.close");

        pumpMessageLoop(900);

        CHECK(f.session->edit() == nullptr);
        f.newProject("Neues");  // and a new project afterwards is not disturbed by the old timer
        pumpMessageLoop(900);
        CHECK_FALSE(f.modified());
    }
}
