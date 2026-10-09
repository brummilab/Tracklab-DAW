// Unsaved changes (M1-04): project.close / project.open / project.new never drop them silently. Only project.close with
// "discard": true does, and that command is flagged destructive (the confirmation comes from the GUI / the Claude rules).
#include "project/project_fixture.h"

#include "core/undo_contract.h"

#include <string>

namespace
{

using namespace tracklab_test::project;

/** An open, saved project that has an unsaved change. */
struct DirtyProject
{
    explicit DirtyProject(ProjectFixture& fixture) : f(fixture)
    {
        file = f.newProject("Muster");
        f.addSampleContent();
        f.run("project.save");
        f.settleEdit();
        savedBytes = bytesOf(file);
        f.makeModified();
        editBefore = f.session->edit();
        stateBefore = tracklab_test::undo::normalisedState(f.edit());
    }

    /** The project is still open, still modified, unchanged, and the file on disk is the one that was saved. */
    void checkUntouched() const
    {
        CHECK(f.session->edit() == editBefore);
        CHECK(f.context.edit() == editBefore);
        CHECK(f.modified());
        CHECK(tracklab_test::undo::normalisedState(f.edit()) == stateBefore);
        CHECK(sameBytes(bytesOf(file), savedBytes));
    }

    ProjectFixture& f;
    juce::File file;
    juce::MemoryBlock savedBytes;
    te::Edit* editBefore = nullptr;
    std::string stateBefore;
};

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("project.get_info reports modified after a change and not modified after the save")
    {
        ProjectFixture f;
        f.newProject("Muster");
        f.settleEdit();
        CHECK_FALSE(f.modified());

        f.makeModified();
        CHECK(f.modified());

        f.run("project.save");
        f.settleEdit();
        CHECK_FALSE(f.modified());
    }

    TEST_CASE("project.close with unsaved changes fails with unsaved_changes and keeps the project")
    {
        ProjectFixture f;
        DirtyProject p(f);

        SUBCASE("without the parameter")
        {
            CHECK(f.errorOf("project.close") == "unsaved_changes");
        }
        SUBCASE("with discard false")
        {
            CHECK(f.errorOf("project.close", Json{{"discard", false}}) == "unsaved_changes");
        }

        p.checkUntouched();
    }

    TEST_CASE("project.open with unsaved changes fails with unsaved_changes and keeps the project")
    {
        ProjectFixture f;
        const auto otherFile = f.newProject("Anderes");  // another project on disk to open
        f.run("project.close");
        DirtyProject p(f);

        CHECK(f.errorOf("project.open", Json{{"path", utf8(otherFile)}}) == "unsaved_changes");

        p.checkUntouched();
    }

    TEST_CASE("project.new with unsaved changes fails with unsaved_changes, creates nothing and keeps the project")
    {
        ProjectFixture f;
        DirtyProject p(f);
        const auto before = tracklab_test::snapshotOf(f.root());

        CHECK(f.errorOf("project.new", Json{{"folder", utf8(f.root())}, {"name", "Neues"}}) == "unsaved_changes");

        CHECK(tracklab_test::snapshotOf(f.root()) == before);
        p.checkUntouched();
    }

    TEST_CASE("project.close with discard drops the changes without writing them")
    {
        ProjectFixture f;
        DirtyProject p(f);

        const auto result = f.tryRun("project.close", Json{{"discard", true}});

        INFO(result.error.code << ": " << result.error.message);
        REQUIRE(result.ok);
        CHECK(result.result["closed"].get<bool>());
        CHECK(f.session->edit() == nullptr);
        CHECK(f.context.edit() == nullptr);
        CHECK(f.errorOf("project.get_info") == "no_edit");
        CHECK(sameBytes(bytesOf(p.file), p.savedBytes));
        CHECK(fileNamesIn(f.projectFolder("Muster")) == std::vector<std::string>{"Muster.tracklab"});
    }

    TEST_CASE("after a discard the project opens with the last saved state")
    {
        ProjectFixture f;
        DirtyProject p(f);
        f.run("project.close", Json{{"discard", true}});

        f.run("project.open", Json{{"path", utf8(p.file)}});
        f.settleEdit();

        CHECK_FALSE(f.modified());
        CHECK(f.edit().state.getProperty("sampleNote").toString().isEmpty());
        auto tracks = te::getAudioTracks(f.edit());
        REQUIRE(tracks.size() >= 3);
        CHECK(tracks[0]->getName() == "Gitarre");
    }

    TEST_CASE("project.close of a saved project needs no discard")
    {
        ProjectFixture f;
        f.newProject("Muster");
        f.addSampleContent();
        f.run("project.save");
        f.settleEdit();

        const auto result = f.tryRun("project.close");

        INFO(result.error.code << ": " << result.error.message);
        CHECK(result.ok);
        CHECK(f.session->edit() == nullptr);
    }

    TEST_CASE("project.close of a project that was never changed needs no discard")
    {
        ProjectFixture f;
        f.newProject("Muster");
        f.settleEdit();
        pumpMessageLoop(300);

        CHECK(f.tryRun("project.close").ok);
    }

    TEST_CASE("saving makes closing possible without discard")
    {
        ProjectFixture f;
        DirtyProject p(f);
        REQUIRE(f.errorOf("project.close") == "unsaved_changes");

        f.run("project.save");
        f.settleEdit();

        CHECK(f.tryRun("project.close").ok);
    }

    TEST_CASE("discard must be a boolean")
    {
        ProjectFixture f;
        f.newProject("Muster");

        CHECK(f.errorOf("project.close", Json{{"discard", "yes"}}) == "invalid_params");
        CHECK(f.errorOf("project.close", Json{{"discard", 1}}) == "invalid_params");
        CHECK(f.errorOf("project.close", Json{{"force", true}}) == "invalid_params");
        CHECK(f.session->edit() != nullptr);
    }

    TEST_CASE(
        "project.open and project.new have no discard parameter: dropping changes goes through project.close only")
    {
        ProjectFixture f;
        DirtyProject p(f);

        CHECK(f.errorOf("project.open", Json{{"path", utf8(p.file)}, {"discard", true}}) == "invalid_params");
        CHECK(f.errorOf("project.new", Json{{"folder", utf8(f.root())}, {"name", "Neues"}, {"discard", true}}) ==
              "invalid_params");

        p.checkUntouched();
    }

    TEST_CASE("destroying the session with unsaved changes writes nothing")
    {
        ProjectFixture f;
        DirtyProject p(f);

        f.session.reset();

        CHECK(f.context.edit() == nullptr);
        CHECK(sameBytes(bytesOf(p.file), p.savedBytes));
        CHECK(fileNamesIn(f.projectFolder("Muster")) == std::vector<std::string>{"Muster.tracklab"});
    }
}
