// Project round trip (M1-04): new -> change -> save -> close -> open gives the same project; what open and save do and
// do not touch on disk; the state a freshly opened project is in.
#include "project/project_fixture.h"

#include "core/undo_contract.h"

#include <string>

namespace
{

using namespace tracklab_test::project;
namespace project = tracklab::project;

std::string stateOf(te::Edit& edit)
{
    return tracklab_test::undo::normalisedState(edit);
}

/** `count` undo transactions with 100 changes each, so that the undo limit is exactly the number of levels (see
    tests/core/test_undo_levels.cpp). */
void fillUndoHistory(te::Edit& edit, int count)
{
    auto& um = edit.getUndoManager();
    for (int step = 0; step < count; ++step)
    {
        um.beginNewTransaction("Schritt " + juce::String(step));
        for (int k = 0; k < 100; ++k)
            edit.state.setProperty(juce::Identifier("p" + juce::String(k)), step + 1, &um);
    }
}

int countUndoSteps(te::Edit& edit)
{
    int steps = 0;
    while (edit.getUndoManager().canUndo())
    {
        REQUIRE(edit.getUndoManager().undo());
        ++steps;
        REQUIRE(steps <= 100000);
    }
    return steps;
}

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("save, close and open give the same project state")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.addSampleContent();
        f.run("project.save");
        f.settleEdit();
        const auto saved = stateOf(f.edit());
        REQUIRE_FALSE(saved.empty());

        f.run("project.close");
        CHECK(f.session->edit() == nullptr);
        f.run("project.open", Json{{"path", utf8(file)}});
        f.settleEdit();

        CHECK(stateOf(f.edit()) == saved);
        auto tracks = te::getAudioTracks(f.edit());
        REQUIRE(tracks.size() >= 3);
        CHECK(tracks[0]->getName() == "Gitarre");
        CHECK(tracks[1]->getName() == "Bass");
        CHECK(tracks[2]->getName() == "Gesang");
        CHECK(f.edit().tempoSequence.getTempo(0)->getBpm() == doctest::Approx(97.0));
    }

    TEST_CASE("a second save/open cycle keeps the state, too")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.addSampleContent();
        f.run("project.save");
        f.run("project.close");
        f.run("project.open", Json{{"path", utf8(file)}});
        f.settleEdit();
        const auto firstOpen = stateOf(f.edit());

        f.run("project.save");
        f.run("project.close");
        f.run("project.open", Json{{"path", utf8(file)}});
        f.settleEdit();

        CHECK(stateOf(f.edit()) == firstOpen);
    }

    TEST_CASE("properties and nodes that Tracktion does not know survive open and save")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.run("project.close");
        rewriteXml(file,
                   [](juce::XmlElement& root)
                   {
                       root.setAttribute("customNote", "Beispiel");
                       auto* extra = root.createNewChildElement("EXTRANODE");
                       extra->setAttribute("flavour", "fremd");
                   });

        f.run("project.open", Json{{"path", utf8(file)}});
        f.run("project.save");

        const auto xml = parseXml(file);
        REQUIRE(xml != nullptr);
        CHECK(xml->getStringAttribute("customNote") == "Beispiel");
        const auto* extra = xml->getChildByName("EXTRANODE");
        REQUIRE(extra != nullptr);
        CHECK(extra->getStringAttribute("flavour") == "fremd");
    }

    //==========================================================================
    // project.open
    TEST_CASE("project.open reports path, name, format version and not modified")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.run("project.close");

        const auto result = f.run("project.open", Json{{"path", utf8(file)}});
        f.settleEdit();

        CHECK(result["path"].get<std::string>() == utf8(file));
        CHECK(result["name"].get<std::string>() == "Muster");
        CHECK(result["format_version"].get<int>() == 1);
        CHECK_FALSE(result["modified"].get<bool>());
        CHECK_FALSE(f.modified());
        CHECK(f.info() == Json{{"path", utf8(file)}, {"name", "Muster"}, {"format_version", 1}, {"modified", false}});
    }

    TEST_CASE("project.open connects the Edit to the project file and the edit context")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.run("project.close");

        f.run("project.open", Json{{"path", utf8(file)}});

        REQUIRE(f.session->edit() != nullptr);
        CHECK(f.context.edit() == f.session->edit());
        REQUIRE(f.edit().editFileRetriever);
        CHECK(f.edit().editFileRetriever() == file);
        CHECK(f.edit().alwaysUseRelativePaths.get());
    }

    TEST_CASE("project.open writes nothing to disk")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.addSampleContent();
        f.run("project.save");
        f.run("project.close");
        const auto before = tracklab_test::snapshotOf(f.projectFolder("Muster"));
        const auto bytes = bytesOf(file);

        f.run("project.open", Json{{"path", utf8(file)}});
        f.settleEdit();
        pumpMessageLoop(300);  // any timer of the engine would have fired by now

        CHECK(tracklab_test::snapshotOf(f.projectFolder("Muster")) == before);
        CHECK(sameBytes(bytesOf(file), bytes));
    }

    TEST_CASE("a freshly opened project is not modified and has an empty undo history")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.addSampleContent();
        f.run("project.save");
        f.run("project.close");

        f.run("project.open", Json{{"path", utf8(file)}});
        f.settleEdit();
        pumpMessageLoop(300);

        CHECK_FALSE(f.modified());
        const auto undo = f.run("edit.get_undo_state");
        CHECK_FALSE(undo["can_undo"].get<bool>());
        CHECK_FALSE(undo["can_redo"].get<bool>());
    }

    TEST_CASE("project.open replaces the open project when nothing is unsaved")
    {
        ProjectFixture f;
        const auto first = f.newProject("Erstes");
        f.run("project.close");
        f.newProject("Zweites");
        f.settleEdit();

        f.run("project.open", Json{{"path", utf8(first)}});

        CHECK(f.info()["name"].get<std::string>() == "Erstes");
        CHECK(f.context.edit() == f.session->edit());
        CHECK(f.projectFile("Zweites").existsAsFile());
    }

    TEST_CASE("project.open takes a path and nothing else")
    {
        ProjectFixture f;

        CHECK(f.errorOf("project.open", Json::object()) == "invalid_params");
        CHECK(f.errorOf("project.open", Json{{"path", 5}}) == "invalid_params");
        CHECK(f.errorOf("project.open", Json{{"path", "/x/y.tracklab"}, {"discard", true}}) == "invalid_params");
    }

    TEST_CASE("project.open of a file that does not exist fails with project_not_found and keeps the open project")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.settleEdit();
        auto* editBefore = f.session->edit();

        const auto missing = f.root().getChildFile("Gibt-es-nicht").getChildFile("Gibt-es-nicht.tracklab");
        CHECK(f.errorOf("project.open", Json{{"path", utf8(missing)}}) == "project_not_found");

        CHECK(f.session->edit() == editBefore);
        CHECK(f.info()["path"].get<std::string>() == utf8(file));
        CHECK_FALSE(missing.getParentDirectory().exists());  // opening never creates anything
    }

    TEST_CASE("projects opened by project.open and created by project.new keep 200 undo steps")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");

        SUBCASE("new")
        {
            fillUndoHistory(f.edit(), 250);
            settle(f.edit());
            CHECK(countUndoSteps(f.edit()) == 200);
        }
        SUBCASE("open")
        {
            f.run("project.close");
            f.run("project.open", Json{{"path", utf8(file)}});
            f.settleEdit();
            fillUndoHistory(f.edit(), 250);
            settle(f.edit());
            CHECK(countUndoSteps(f.edit()) == 200);
        }
    }

    //==========================================================================
    // project.save
    TEST_CASE("project.save writes the project file with the format version and clears modified")
    {
        ProjectFixture f;
        const auto file = f.newProject("Muster");
        f.addSampleContent();
        REQUIRE(f.modified());
        const auto before = bytesOf(file);

        const auto result = f.run("project.save");
        f.settleEdit();

        CHECK_FALSE(sameBytes(bytesOf(file), before));
        CHECK(result["path"].get<std::string>() == utf8(file));
        CHECK_FALSE(result["modified"].get<bool>());
        CHECK_FALSE(f.modified());
        CHECK_FALSE(f.edit().hasChangedSinceSaved());
        const auto xml = parseXml(file);
        REQUIRE(xml != nullptr);
        CHECK(xml->hasTagName("EDIT"));
        CHECK(xml->getIntAttribute(project::formatVersionProperty, -1) == 1);
    }

    TEST_CASE("a change after a save makes the project modified again")
    {
        ProjectFixture f;
        f.newProject("Muster");
        f.run("project.save");
        f.settleEdit();
        REQUIRE_FALSE(f.modified());

        f.makeModified();

        CHECK(f.modified());
    }

    TEST_CASE("project.save leaves no temporary file next to the project file")
    {
        ProjectFixture f;
        f.newProject("Muster");
        f.addSampleContent();

        f.run("project.save");
        f.run("project.save");

        CHECK(fileNamesIn(f.projectFolder("Muster")) == std::vector<std::string>{"Muster.tracklab"});
        for (const char* sub : project::subFolderNames)
            CHECK(f.projectFolder("Muster").getChildFile(sub).isDirectory());
    }

    TEST_CASE("project.save and project.get_info without an open project fail with no_edit")
    {
        ProjectFixture f;

        CHECK(f.errorOf("project.save") == "no_edit");
        CHECK(f.errorOf("project.get_info") == "no_edit");
        CHECK(f.errorOf("project.save_as", Json{{"folder", utf8(f.root())}, {"name", "Muster"}}) == "no_edit");
        CHECK(f.root().getNumberOfChildFiles(juce::File::findFilesAndDirectories) == 0);
    }

    TEST_CASE("project.close without an open project is ok and does nothing")
    {
        ProjectFixture f;

        CHECK(f.tryRun("project.close").ok);
        CHECK(f.tryRun("project.close", Json{{"discard", true}}).ok);
        CHECK(f.errorOf("project.get_info") == "no_edit");
    }

    //==========================================================================
    // project.save_as
    TEST_CASE("project.save_as writes a new project folder and continues in it; the old file stays as it was")
    {
        ProjectFixture f;
        const auto first = f.newProject("Erstes");
        f.run("project.save");
        const auto firstBytes = bytesOf(first);
        f.addSampleContent();

        const auto result = f.run("project.save_as", Json{{"folder", utf8(f.root())}, {"name", "Zweites"}});
        f.settleEdit();

        const auto second = f.projectFile("Zweites");
        CHECK(second.existsAsFile());
        for (const char* sub : project::subFolderNames)
            CHECK(f.projectFolder("Zweites").getChildFile(sub).isDirectory());
        CHECK(result["path"].get<std::string>() == utf8(second));
        CHECK(result["name"].get<std::string>() == "Zweites");
        CHECK_FALSE(result["modified"].get<bool>());
        CHECK(f.info()["path"].get<std::string>() == utf8(second));
        CHECK(sameBytes(bytesOf(first), firstBytes));
        CHECK(fileNamesIn(f.projectFolder("Zweites")) == std::vector<std::string>{"Zweites.tracklab"});

        // From now on save writes the new file, not the old one.
        f.makeModified();
        f.run("project.save");
        CHECK(sameBytes(bytesOf(first), firstBytes));
        const auto reopened = parseXml(second);
        REQUIRE(reopened != nullptr);
        CHECK(reopened->getStringAttribute("sampleNote").isNotEmpty());

        // The content of the first project was in the copy.
        f.run("project.close");
        f.run("project.open", Json{{"path", utf8(second)}});
        auto tracks = te::getAudioTracks(f.edit());
        REQUIRE(tracks.size() >= 3);
        CHECK(tracks[0]->getName() == "Gitarre");
    }

    TEST_CASE("project.save_as refuses an existing project and invalid names, and changes nothing")
    {
        ProjectFixture f;
        const auto first = f.newProject("Erstes");
        f.run("project.save");
        const auto other = f.root().getChildFile("Zweites");
        other.createDirectory();
        const auto otherFile = other.getChildFile("Zweites.tracklab");
        writeText(otherFile, "irgendwas");
        const auto otherBytes = bytesOf(otherFile);

        CHECK(f.errorOf("project.save_as", Json{{"folder", utf8(f.root())}, {"name", "Zweites"}}) == "project_exists");
        CHECK(f.errorOf("project.save_as", Json{{"folder", utf8(f.root())}, {"name", "../x"}}) ==
              "invalid_project_name");
        CHECK(f.errorOf("project.save_as", Json{{"folder", utf8(f.root())}, {"name", ""}}) == "invalid_project_name");

        CHECK(sameBytes(bytesOf(otherFile), otherBytes));
        CHECK(f.info()["path"].get<std::string>() == utf8(first));
    }
}
