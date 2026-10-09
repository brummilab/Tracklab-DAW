// Save As and media (M1-04, lead decision 5): save_as copies no media, so a source outside the new project folder has to
// be found from the new project file: through a path that is relative to the new file, or an absolute one.
#include "project/project_fixture.h"

#include <string>

namespace
{

using namespace tracklab_test::project;

te::WaveAudioClip* firstWaveClip(te::Edit& edit)
{
    for (auto* track : te::getAudioTracks(edit))
        for (auto* clip : track->getClips())
            if (auto* wave = dynamic_cast<te::WaveAudioClip*>(clip))
                return wave;
    return nullptr;
}

const juce::XmlElement* findAudioClip(const juce::XmlElement& element)
{
    if (element.hasTagName("AUDIOCLIP"))
        return &element;
    for (const auto* child : element.getChildIterator())
        if (const auto* found = findAudioClip(*child))
            return found;
    return nullptr;
}

std::string sourceAttribute(const juce::File& projectFile)
{
    const auto xml = parseXml(projectFile);
    REQUIRE(xml != nullptr);
    const auto* clip = findAudioClip(*xml);
    REQUIRE(clip != nullptr);
    return clip->getStringAttribute("source").toStdString();
}

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("after save_as the clip of a file in the old project's Audio/ is still found")
    {
        ProjectFixture f;
        f.newProject("Erstes");
        const auto audio = f.projectFolder("Erstes").getChildFile("Audio").getChildFile("take1.wav");
        writeSineWav(audio);
        f.addClip(audio);
        f.run("project.save");

        f.run("project.save_as", Json{{"folder", utf8(f.root())}, {"name", "Zweites"}});
        f.settleEdit();

        auto* clip = firstWaveClip(f.edit());
        REQUIRE(clip != nullptr);
        CHECK(clip->getSourceFileReference().getFile() == audio);
        CHECK(clip->getAudioFile().isValid());

        // Also from the file on disk: nothing was copied, and the path in it leads back to the original.
        f.run("project.close");
        f.run("project.open", Json{{"path", utf8(f.projectFile("Zweites"))}});
        auto* reopened = firstWaveClip(f.edit());
        REQUIRE(reopened != nullptr);
        CHECK(reopened->getSourceFileReference().getFile() == audio);
        CHECK(reopened->getSourceFileReference().getFile().existsAsFile());
        CHECK_FALSE(f.projectFolder("Zweites").getChildFile("Audio").getChildFile("take1.wav").exists());
    }

    TEST_CASE("the path written by save_as is relative to the new project file, and the old file is untouched")
    {
        ProjectFixture f;
        const auto first = f.newProject("Erstes");
        const auto audio = f.projectFolder("Erstes").getChildFile("Audio").getChildFile("take1.wav");
        writeSineWav(audio);
        f.addClip(audio);
        f.run("project.save");
        const auto firstBytes = bytesOf(first);

        f.run("project.save_as", Json{{"folder", utf8(f.root())}, {"name", "Zweites"}});

        const auto source = sourceAttribute(f.projectFile("Zweites"));
        CHECK_FALSE(juce::File::isAbsolutePath(juce::String(source)));
        CHECK(f.projectFile("Zweites").getParentDirectory().getChildFile(juce::String(source)).getFullPathName() ==
              audio.getFullPathName());
        CHECK_FALSE(f.projectFile("Zweites").loadFileAsString().contains(f.root().getFullPathName()));
        CHECK(sameBytes(bytesOf(first), firstBytes));
    }

    TEST_CASE("a source outside every project folder is found after save_as, too")
    {
        ProjectFixture f;
        const tracklab_test::ScopedTempDir library;
        const auto audio = library.dir().getChildFile("Sample Bibliothek").getChildFile("loop.wav");
        writeSineWav(audio);
        f.newProject("Erstes");
        f.addClip(audio);
        f.run("project.save");

        f.run("project.save_as", Json{{"folder", utf8(f.root())}, {"name", "Zweites"}});
        f.run("project.close");
        f.run("project.open", Json{{"path", utf8(f.projectFile("Zweites"))}});

        auto* clip = firstWaveClip(f.edit());
        REQUIRE(clip != nullptr);
        CHECK(clip->getSourceFileReference().getFile() == audio);
        CHECK(clip->getAudioFile().isValid());
    }

    TEST_CASE("save_as adds nothing to the undo history because of the re-written paths")
    {
        ProjectFixture f;
        f.newProject("Erstes");
        const auto audio = f.projectFolder("Erstes").getChildFile("Audio").getChildFile("take1.wav");
        writeSineWav(audio);
        f.addClip(audio);
        const auto before = f.edit().getUndoManager().getUndoDescriptions();

        f.run("project.save_as", Json{{"folder", utf8(f.root())}, {"name", "Zweites"}});
        f.settleEdit();

        CHECK(f.edit().getUndoManager().getUndoDescriptions() == before);
        CHECK_FALSE(f.modified());
    }

    TEST_CASE("an aborted save_as leaves the paths, the file and the modified flag as they were")
    {
        ProjectFixture f;
        const auto first = f.newProject("Erstes");
        const auto audio = f.projectFolder("Erstes").getChildFile("Audio").getChildFile("take1.wav");
        writeSineWav(audio);
        f.addClip(audio);
        f.run("project.save");
        f.settleEdit();
        const auto sourceBefore = firstWaveClip(f.edit())->state.getProperty(te::IDs::source).toString();
        f.session->setBeforeReplaceHook([](const juce::File&, const juce::File&) { return false; });

        CHECK(f.errorOf("project.save_as", Json{{"folder", utf8(f.root())}, {"name", "Zweites"}}) == "save_failed");

        CHECK(firstWaveClip(f.edit())->state.getProperty(te::IDs::source).toString() == sourceBefore);
        CHECK(firstWaveClip(f.edit())->getSourceFileReference().getFile() == audio);
        CHECK(f.edit().editFileRetriever() == first);
        CHECK_FALSE(f.modified());
        CHECK_FALSE(f.projectFolder("Zweites").exists());

        f.session->setBeforeReplaceHook({});
        f.run("project.save");  // the project still saves to its own file
        CHECK(first.existsAsFile());
    }
}
