// Relative media paths (M1-04): a clip of a file in Audio/ is stored relative to the project file, so that the whole project
// folder can be moved or copied and the media are still found (alwaysUseRelativePaths, DESIGN Rev 3 section 3).
// The audio is a generated sine; the clip is made through the Tracktion API, as the import will do.
#include "project/project_fixture.h"

#include <functional>
#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::project;

/** The `source` attributes of all audio clips of the project file. */
std::vector<std::string> clipSources(const juce::File& projectFile)
{
    std::vector<std::string> sources;
    const auto xml = parseXml(projectFile);
    REQUIRE(xml != nullptr);
    std::function<void(const juce::XmlElement&)> visit = [&](const juce::XmlElement& element)
    {
        if (element.hasTagName("AUDIOCLIP"))
            sources.push_back(element.getStringAttribute("source").toStdString());
        for (const auto* child : element.getChildIterator())
            visit(*child);
    };
    visit(*xml);
    return sources;
}

std::string withSlashes(std::string path)
{
    for (auto& c : path)
        if (c == '\\')
            c = '/';
    return path;
}

te::WaveAudioClip* firstWaveClip(te::Edit& edit)
{
    for (auto* track : te::getAudioTracks(edit))
        for (auto* clip : track->getClips())
            if (auto* wave = dynamic_cast<te::WaveAudioClip*>(clip))
                return wave;
    return nullptr;
}

/** A project with one clip of Audio/take1.wav, saved and closed. Returns the project file. */
juce::File projectWithClip(ProjectFixture& f, const std::string& name = "Muster",
                           const juce::String& audioPath = "Audio/take1.wav")
{
    const auto file = f.newProject(name);
    const auto audio = f.projectFolder(name).getChildFile(audioPath);
    writeSineWav(audio);
    f.addClip(audio);
    f.run("project.save");
    f.settleEdit();
    f.run("project.close");
    return file;
}

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("a clip of a file in Audio/ is stored with a path relative to the project file")
    {
        ProjectFixture f;
        const auto file = projectWithClip(f);

        const auto sources = clipSources(file);

        REQUIRE(sources.size() == 1);
        CHECK(withSlashes(sources[0]) == "Audio/take1.wav");
        CHECK_FALSE(juce::File::isAbsolutePath(juce::String(sources[0])));
    }

    TEST_CASE("the saved project file contains no absolute path of the project folder")
    {
        ProjectFixture f;
        const auto file = projectWithClip(f);

        const auto text = file.loadFileAsString();

        CHECK_FALSE(text.contains(f.root().getFullPathName()));
        CHECK_FALSE(text.contains(f.projectFolder("Muster").getFullPathName()));
    }

    TEST_CASE("a clip of a file in a sub folder of Audio/ is relative, too")
    {
        ProjectFixture f;
        const auto file = projectWithClip(f, "Muster", "Audio/Tag 1/take2.wav");

        const auto sources = clipSources(file);

        REQUIRE(sources.size() == 1);
        CHECK(withSlashes(sources[0]) == "Audio/Tag 1/take2.wav");
    }

    TEST_CASE("moving the project folder keeps the clip's media: the source resolves in the new place")
    {
        ProjectFixture f;
        const auto file = projectWithClip(f);
        const tracklab_test::ScopedTempDir elsewhere;
        const auto newFolder = elsewhere.dir().getChildFile("Umgezogen");
        REQUIRE(f.projectFolder("Muster").moveFileTo(newFolder));
        REQUIRE_FALSE(file.exists());  // the old place is gone: a stale absolute path cannot be what is found
        const auto movedFile = newFolder.getChildFile("Muster.tracklab");
        REQUIRE(movedFile.existsAsFile());

        f.run("project.open", Json{{"path", utf8(movedFile)}});
        f.settleEdit();

        auto* clip = firstWaveClip(f.edit());
        REQUIRE(clip != nullptr);
        const auto source = clip->getSourceFileReference().getFile();
        CHECK(source == newFolder.getChildFile("Audio").getChildFile("take1.wav"));
        CHECK(source.existsAsFile());
        CHECK(clip->getAudioFile().isValid());
    }

    TEST_CASE("a copy of the project folder resolves its media inside the copy, not in the original")
    {
        ProjectFixture f;
        const auto file = projectWithClip(f);
        const tracklab_test::ScopedTempDir elsewhere;
        const auto copy = elsewhere.dir().getChildFile("Kopie");
        REQUIRE(f.projectFolder("Muster").copyDirectoryTo(copy));
        REQUIRE(f.projectFolder("Muster")
                    .getChildFile("Audio")
                    .getChildFile("take1.wav")
                    .deleteFile());  // original media gone

        f.run("project.open", Json{{"path", utf8(copy.getChildFile("Muster.tracklab"))}});

        auto* clip = firstWaveClip(f.edit());
        REQUIRE(clip != nullptr);
        CHECK(clip->getSourceFileReference().getFile() == copy.getChildFile("Audio").getChildFile("take1.wav"));
        CHECK(clip->getSourceFileReference().getFile().existsAsFile());
    }

    TEST_CASE("a project saved after the move still has only relative paths")
    {
        ProjectFixture f;
        projectWithClip(f);
        const tracklab_test::ScopedTempDir elsewhere;
        const auto newFolder = elsewhere.dir().getChildFile("Umgezogen");
        REQUIRE(f.projectFolder("Muster").moveFileTo(newFolder));
        const auto movedFile = newFolder.getChildFile("Muster.tracklab");
        f.run("project.open", Json{{"path", utf8(movedFile)}});
        f.settleEdit();
        f.makeModified();

        f.run("project.save");

        const auto sources = clipSources(movedFile);
        REQUIRE(sources.size() == 1);
        CHECK(withSlashes(sources[0]) == "Audio/take1.wav");
        CHECK_FALSE(movedFile.loadFileAsString().contains(elsewhere.dir().getFullPathName()));
    }

    TEST_CASE("the clip's source resolves in the folder where the project was created")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        const auto audio = f.projectFolder("Muster").getChildFile("Audio").getChildFile("take1.wav");
        writeSineWav(audio);

        auto clip = f.addClip(audio);

        REQUIRE(clip != nullptr);
        CHECK(clip->getSourceFileReference().getFile() == audio);
        CHECK(clip->getAudioFile().isValid());
        CHECK(file.existsAsFile());
    }
}
