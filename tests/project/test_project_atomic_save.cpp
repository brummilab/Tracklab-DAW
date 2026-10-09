// Atomic save (M1-04, DESIGN Rev 3 section 3): temporary file next to the project file -> write -> flush -> replace.
// The test hook ProjectSession::setBeforeReplaceHook() stops a save at the one moment that matters: after the
// temporary file is complete, before it replaces the project file.
#include "project/project_fixture.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::project;

/** What the hook saw. */
struct HookObservation
{
    int calls = 0;
    juce::File temporary;
    juce::File target;
    bool temporaryExisted = false;
    bool sameFolder = false;
    juce::MemoryBlock temporaryContent;
    juce::MemoryBlock targetContent;
};

/** A project with content, saved once, then changed again (so that the next save has something to write). */
struct SavedProject
{
    explicit SavedProject(ProjectFixture& fixture) : f(fixture)
    {
        file = f.newProject("Muster");
        f.addSampleContent();
        f.run("project.save");
        f.settleEdit();
        savedBytes = bytesOf(file);
        f.makeModified();
    }

    ProjectFixture& f;
    juce::File file;
    juce::MemoryBlock savedBytes;
};

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("an aborted save leaves the old project file byte-identical")
    {
        ProjectFixture f;
        SavedProject p(f);
        f.session->setBeforeReplaceHook([](const juce::File&, const juce::File&) { return false; });

        const auto result = f.tryRun("project.save");

        CHECK_FALSE(result.ok);
        CHECK(result.error.code == "save_failed");
        CHECK(sameBytes(bytesOf(p.file), p.savedBytes));
    }

    TEST_CASE("an aborted save leaves no temporary file behind and the project stays modified")
    {
        ProjectFixture f;
        SavedProject p(f);
        f.session->setBeforeReplaceHook([](const juce::File&, const juce::File&) { return false; });

        CHECK_FALSE(f.tryRun("project.save").ok);

        CHECK(fileNamesIn(f.projectFolder("Muster")) == std::vector<std::string>{"Muster.tracklab"});
        CHECK(f.modified());
        CHECK(f.edit().hasChangedSinceSaved());
    }

    TEST_CASE("a hook that throws aborts the save the same way")
    {
        ProjectFixture f;
        SavedProject p(f);
        f.session->setBeforeReplaceHook([](const juce::File&, const juce::File&) -> bool
                                        { throw std::runtime_error("power failure"); });

        const auto result = f.tryRun("project.save");

        CHECK_FALSE(result.ok);
        CHECK(result.error.code == "save_failed");
        CHECK(sameBytes(bytesOf(p.file), p.savedBytes));
        CHECK(fileNamesIn(f.projectFolder("Muster")) == std::vector<std::string>{"Muster.tracklab"});
        CHECK(f.modified());
    }

    TEST_CASE("after an aborted save the next save works and the project closes cleanly")
    {
        ProjectFixture f;
        SavedProject p(f);
        f.session->setBeforeReplaceHook([](const juce::File&, const juce::File&) { return false; });
        CHECK_FALSE(f.tryRun("project.save").ok);

        f.session->setBeforeReplaceHook({});
        const auto result = f.tryRun("project.save");

        INFO(result.error.code << ": " << result.error.message);
        REQUIRE(result.ok);
        CHECK_FALSE(sameBytes(bytesOf(p.file), p.savedBytes));
        CHECK_FALSE(f.modified());
        CHECK(f.tryRun("project.close").ok);
    }

    TEST_CASE("an aborted save still leaves a project that opens, with the old content")
    {
        ProjectFixture f;
        SavedProject p(f);
        f.session->setBeforeReplaceHook([](const juce::File&, const juce::File&) { return false; });
        CHECK_FALSE(f.tryRun("project.save").ok);

        f.run("project.close", Json{{"discard", true}});
        f.run("project.open", Json{{"path", utf8(p.file)}});

        auto tracks = te::getAudioTracks(f.edit());
        REQUIRE(tracks.size() >= 3);
        CHECK(tracks[2]->getName() == "Gesang");
        CHECK(f.edit().state.getProperty("sampleNote").toString().isEmpty());  // the change was never written
    }

    TEST_CASE(
        "at the moment of the replace the temporary file is complete, next to the target, and the target is still old")
    {
        ProjectFixture f;
        SavedProject p(f);
        HookObservation seen;
        f.session->setBeforeReplaceHook(
            [&seen](const juce::File& temporary, const juce::File& target)
            {
                ++seen.calls;
                seen.temporary = temporary;
                seen.target = target;
                seen.temporaryExisted = temporary.existsAsFile();
                seen.sameFolder = temporary.getParentDirectory() == target.getParentDirectory();
                seen.temporaryContent = bytesOf(temporary);
                seen.targetContent = bytesOf(target);
                return true;  // let the save go on
            });

        const auto result = f.tryRun("project.save");

        INFO(result.error.code << ": " << result.error.message);
        REQUIRE(result.ok);
        CHECK(seen.calls == 1);
        CHECK(seen.target == p.file);
        CHECK(seen.temporary != p.file);
        CHECK(seen.sameFolder);
        CHECK(seen.temporaryExisted);
        CHECK(sameBytes(seen.targetContent, p.savedBytes));
        CHECK_FALSE(sameBytes(seen.temporaryContent, p.savedBytes));

        // The temporary file was a complete project file: parseable, format version, and it is what replaced the target.
        const auto text = seen.temporaryContent.toString();
        const auto xml = juce::XmlDocument::parse(text);
        REQUIRE(xml != nullptr);
        CHECK(xml->hasTagName("EDIT"));
        CHECK(xml->getIntAttribute(tracklab::project::formatVersionProperty, -1) ==
              tracklab::project::currentFormatVersion);
        CHECK(sameBytes(bytesOf(p.file), seen.temporaryContent));
        CHECK_FALSE(seen.temporary.exists());
        CHECK(fileNamesIn(f.projectFolder("Muster")) == std::vector<std::string>{"Muster.tracklab"});
    }

    TEST_CASE("the hook is called once per save, also for the first save of a project created by project.new")
    {
        ProjectFixture f;
        f.newProject("Muster");
        int calls = 0;
        f.session->setBeforeReplaceHook(
            [&calls](const juce::File&, const juce::File&)
            {
                ++calls;
                return true;
            });

        f.run("project.save");
        f.run("project.save");

        CHECK(calls == 2);
    }

    TEST_CASE("project.save_as aborted before the replace creates no project file")
    {
        ProjectFixture f;
        SavedProject p(f);
        f.session->setBeforeReplaceHook([](const juce::File&, const juce::File&) { return false; });

        const auto result = f.tryRun("project.save_as", Json{{"folder", utf8(f.root())}, {"name", "Zweites"}});

        CHECK_FALSE(result.ok);
        CHECK(result.error.code == "save_failed");
        CHECK_FALSE(f.projectFile("Zweites").exists());
        CHECK(f.info()["path"].get<std::string>() == utf8(p.file));  // still the first project
        CHECK(sameBytes(bytesOf(p.file), p.savedBytes));
    }

    //==========================================================================
    // Edit::isSaveInhibited()
    TEST_CASE("a save is refused with save_inhibited while the Edit is save-inhibited, and nothing is written")
    {
        ProjectFixture f;
        SavedProject p(f);
        int hookCalls = 0;
        f.session->setBeforeReplaceHook(
            [&hookCalls](const juce::File&, const juce::File&)
            {
                ++hookCalls;
                return true;
            });

        {
            const te::Edit::SaveInhibitor inhibitor(f.edit());
            REQUIRE(f.edit().isSaveInhibited());

            const auto result = f.tryRun("project.save");

            CHECK_FALSE(result.ok);
            CHECK(result.error.code == "save_inhibited");
            CHECK(hookCalls == 0);
            CHECK(sameBytes(bytesOf(p.file), p.savedBytes));
            CHECK(fileNamesIn(f.projectFolder("Muster")) == std::vector<std::string>{"Muster.tracklab"});
            CHECK(f.modified());
        }

        const auto after = f.tryRun("project.save");
        INFO(after.error.code << ": " << after.error.message);
        CHECK(after.ok);
        CHECK_FALSE(f.modified());
    }

    TEST_CASE("project.save_as is refused while the Edit is save-inhibited and creates no project file")
    {
        ProjectFixture f;
        SavedProject p(f);
        const te::Edit::SaveInhibitor inhibitor(f.edit());

        const auto result = f.tryRun("project.save_as", Json{{"folder", utf8(f.root())}, {"name", "Zweites"}});

        CHECK_FALSE(result.ok);
        CHECK(result.error.code == "save_inhibited");
        CHECK_FALSE(f.projectFile("Zweites").exists());
    }
}
