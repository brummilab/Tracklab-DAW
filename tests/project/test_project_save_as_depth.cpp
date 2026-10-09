// Save As into a folder of another depth (M1-04, review round 1, finding 2): the paths are written while the Edit's file is
// already the new one, so the clip finds its media at once, without re-opening the project.
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

/** A saved project with a clip of Audio/take1.wav, and an empty folder three levels down. */
struct ProjectWithClip
{
    explicit ProjectWithClip(ProjectFixture& fixture)
    {
        fixture.newProject("Erstes");
        audio = fixture.projectFolder("Erstes").getChildFile("Audio").getChildFile("take1.wav");
        writeSineWav(audio);
        fixture.addClip(audio);
        fixture.run("project.save");
        deeper = fixture.root().getChildFile("a").getChildFile("b").getChildFile("c");
        REQUIRE(deeper.createDirectory().wasOk());
    }

    juce::File audio;
    juce::File deeper;
};

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("after save_as into a folder of another depth the clip is online without re-opening")
    {
        ProjectFixture f;
        const ProjectWithClip p(f);

        f.run("project.save_as", Json{{"folder", utf8(p.deeper)}, {"name", "Zweites"}});

        auto* clip = firstWaveClip(f.edit());
        REQUIRE(clip != nullptr);
        CHECK(clip->getSourceFileReference().getFile() == p.audio);
        CHECK(clip->getAudioFile().isValid());
        CHECK(clip->getCurrentSourceFile() == p.audio);
    }

    TEST_CASE("an aborted save_as into a folder of another depth leaves the clip online")
    {
        ProjectFixture f;
        const ProjectWithClip p(f);
        f.session->setBeforeReplaceHook([](const juce::File&, const juce::File&) { return false; });

        CHECK(f.errorOf("project.save_as", Json{{"folder", utf8(p.deeper)}, {"name", "Zweites"}}) == "save_failed");

        auto* clip = firstWaveClip(f.edit());
        REQUIRE(clip != nullptr);
        CHECK(clip->getAudioFile().isValid());
        CHECK(clip->getCurrentSourceFile() == p.audio);
    }
}
