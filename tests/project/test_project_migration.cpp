// Migration when opening (M1-04): an old file (v0, no tracklabFormatVersion) is migrated in memory, a file from a newer
// Tracklab is refused with a clear error. The framework itself is tested in test_project_format.cpp.
#include "project/project_fixture.h"

#include <string>

namespace
{

using namespace tracklab_test::project;
namespace project = tracklab::project;

/** A saved project with three named tracks, closed; the version attribute is then removed (v0) or set. */
juce::File savedProject(ProjectFixture& f, const std::string& name = "Muster")
{
    const auto file = f.newProject(name);
    f.addSampleContent();
    f.run("project.save");
    f.run("project.close");
    return file;
}

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("a v0 file (no tracklabFormatVersion) opens as version 1 with its content")
    {
        ProjectFixture f;
        const auto file = savedProject(f);
        rewriteXml(file, [](juce::XmlElement& root) { root.removeAttribute(project::formatVersionProperty); });
        REQUIRE_FALSE(parseXml(file)->hasAttribute(project::formatVersionProperty));

        const auto result = f.tryRun("project.open", Json{{"path", utf8(file)}});
        f.settleEdit();

        INFO(result.error.code << ": " << result.error.message);
        REQUIRE(result.ok);
        CHECK(result.result["format_version"].get<int>() == 1);
        CHECK(f.info()["format_version"].get<int>() == 1);
        auto tracks = te::getAudioTracks(f.edit());
        REQUIRE(tracks.size() >= 3);
        CHECK(tracks[0]->getName() == "Gitarre");
        CHECK(tracks[2]->getName() == "Gesang");
        CHECK(f.edit().tempoSequence.getTempo(0)->getBpm() == doctest::Approx(97.0));
    }

    TEST_CASE("opening a v0 file does not rewrite it; the next save writes version 1")
    {
        ProjectFixture f;
        const auto file = savedProject(f);
        rewriteXml(file, [](juce::XmlElement& root) { root.removeAttribute(project::formatVersionProperty); });
        const auto before = bytesOf(file);
        const auto snapshot = tracklab_test::snapshotOf(f.projectFolder("Muster"));

        f.run("project.open", Json{{"path", utf8(file)}});
        f.settleEdit();
        pumpMessageLoop(300);

        CHECK(sameBytes(bytesOf(file), before));
        CHECK(tracklab_test::snapshotOf(f.projectFolder("Muster")) == snapshot);

        f.run("project.save");
        const auto xml = parseXml(file);
        REQUIRE(xml != nullptr);
        CHECK(xml->getIntAttribute(project::formatVersionProperty, -1) == 1);
        auto tracks = te::getAudioTracks(f.edit());
        REQUIRE(tracks.size() >= 3);
        CHECK(tracks[1]->getName() == "Bass");
    }

    TEST_CASE("a file from a newer Tracklab is refused with project_too_new and nothing is changed")
    {
        ProjectFixture f;
        const auto file = savedProject(f);
        rewriteXml(file, [](juce::XmlElement& root) { root.setAttribute(project::formatVersionProperty, 99); });
        const auto before = bytesOf(file);

        const auto result = f.tryRun("project.open", Json{{"path", utf8(file)}});

        CHECK_FALSE(result.ok);
        CHECK(result.error.code == "project_too_new");
        CHECK(result.error.message.find("99") != std::string::npos);
        CHECK(sameBytes(bytesOf(file), before));
        CHECK(f.session->edit() == nullptr);
    }

    TEST_CASE("the version right above the current one is already too new")
    {
        ProjectFixture f;
        const auto file = savedProject(f);
        rewriteXml(file, [](juce::XmlElement& root)
                   { root.setAttribute(project::formatVersionProperty, project::currentFormatVersion + 1); });

        CHECK(f.errorOf("project.open", Json{{"path", utf8(file)}}) == "project_too_new");
    }

    TEST_CASE("a refused newer file does not close the open project")
    {
        ProjectFixture f;
        const auto newer = savedProject(f, "Neuer");
        rewriteXml(newer, [](juce::XmlElement& root) { root.setAttribute(project::formatVersionProperty, 99); });
        const auto current = f.newProject("Aktuell");
        f.settleEdit();
        auto* editBefore = f.session->edit();

        CHECK(f.errorOf("project.open", Json{{"path", utf8(newer)}}) == "project_too_new");

        CHECK(f.session->edit() == editBefore);
        CHECK(f.info()["path"].get<std::string>() == utf8(current));
        CHECK_FALSE(f.modified());
    }

    TEST_CASE("a current v1 file opens without any migration")
    {
        ProjectFixture f;
        const auto file = savedProject(f);
        REQUIRE(parseXml(file)->getIntAttribute(project::formatVersionProperty, -1) == 1);

        const auto result = f.tryRun("project.open", Json{{"path", utf8(file)}});

        CHECK(result.ok);
        CHECK(result.result["format_version"].get<int>() == 1);
    }
}
