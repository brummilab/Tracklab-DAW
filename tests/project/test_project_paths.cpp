// Paths that come from outside (M1-04, lead decision 3): the parent folder has to exist (it is never created for the
// caller), and only absolute paths are taken (nothing may depend on the working directory of the process).
#include "project/project_fixture.h"

#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::project;

/** A relative folder that must not appear under the working directory, whatever the call did. */
const char* const relativeFolder = "tracklab-relative-folder-test";

juce::File relativeFolderInCwd()
{
    return juce::File::getCurrentWorkingDirectory().getChildFile(relativeFolder);
}

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("project.new in a folder that does not exist fails with folder_not_found and creates nothing")
    {
        ProjectFixture f;
        const auto missing = f.root().getChildFile("Gibt-es-nicht");

        const auto result = f.tryRun("project.new", Json{{"folder", utf8(missing)}, {"name", "Muster"}});

        CHECK_FALSE(result.ok);
        CHECK(result.error.code == "folder_not_found");
        CHECK_FALSE(missing.exists());  // the missing folder is not created on the way
        CHECK(f.root().getNumberOfChildFiles(juce::File::findFilesAndDirectories) == 0);
        CHECK(f.session->edit() == nullptr);
    }

    TEST_CASE("project.new with a plain file as the folder fails with folder_not_found")
    {
        ProjectFixture f;
        const auto notAFolder = f.root().getChildFile("datei.txt");
        writeText(notAFolder, "kein Ordner");

        CHECK(f.errorOf("project.new", Json{{"folder", utf8(notAFolder)}, {"name", "Muster"}}) == "folder_not_found");
        CHECK(notAFolder.existsAsFile());
    }

    TEST_CASE("project.save_as in a folder that does not exist fails with folder_not_found and changes nothing")
    {
        ProjectFixture f;
        const auto first = f.newProject("Erstes");
        f.run("project.save");
        const auto before = bytesOf(first);
        const auto missing = f.root().getChildFile("Gibt-es-nicht");

        CHECK(f.errorOf("project.save_as", Json{{"folder", utf8(missing)}, {"name", "Zweites"}}) == "folder_not_found");

        CHECK_FALSE(missing.exists());
        CHECK(f.info()["path"].get<std::string>() == utf8(first));
        CHECK(sameBytes(bytesOf(first), before));
    }

    TEST_CASE("a relative folder is invalid_params for project.new and project.save_as, and nothing is created")
    {
        ProjectFixture f;
        REQUIRE_FALSE(relativeFolderInCwd().exists());

        CHECK(f.errorOf("project.new", Json{{"folder", relativeFolder}, {"name", "Muster"}}) == "invalid_params");
        CHECK(f.errorOf("project.new", Json{{"folder", ""}, {"name", "Muster"}}) == "invalid_params");
        CHECK(f.errorOf("project.new", Json{{"folder", "."}, {"name", "Muster"}}) == "invalid_params");

        f.newProject("Erstes");
        CHECK(f.errorOf("project.save_as", Json{{"folder", relativeFolder}, {"name", "Zweites"}}) == "invalid_params");

        const bool created = relativeFolderInCwd().exists();
        relativeFolderInCwd().deleteRecursively();
        CHECK_FALSE(created);
        CHECK(f.info()["name"].get<std::string>() == "Erstes");
    }

    TEST_CASE("a relative path is invalid_params for project.open, with a pointer to the parameter")
    {
        ProjectFixture f;

        const auto result = f.tryRun("project.open", Json{{"path", "Muster/Muster.tracklab"}});

        CHECK_FALSE(result.ok);
        CHECK(result.error.code == "invalid_params");
        CHECK(result.error.pointer == "/path");
        CHECK(f.errorOf("project.open", Json{{"path", ""}}) == "invalid_params");
    }

    TEST_CASE("the check for an absolute path comes before the one for the folder")
    {
        ProjectFixture f;

        const auto result = f.tryRun("project.new", Json{{"folder", "Gibt/es/nicht"}, {"name", "Muster"}});

        CHECK(result.error.code == "invalid_params");
        CHECK(result.error.pointer == "/folder");
    }
}
