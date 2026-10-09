// Unreadable project files (M1-04): project.open reports corrupt_project, never crashes, never touches the file, and
// keeps whatever project was open. (The offer of a backup follows in M1-05.)
#include "project/project_fixture.h"

#include "core/undo_contract.h"

#include <string>

namespace
{

using namespace tracklab_test::project;

/** The corrupt variants: name and content. The content of "truncated" is cut from a real project file. */
struct Variant
{
    std::string name;
    juce::MemoryBlock content;
};

juce::MemoryBlock blockOf(const juce::String& text)
{
    return juce::MemoryBlock(text.toRawUTF8(), text.getNumBytesAsUTF8());
}

std::vector<Variant> corruptVariants(const juce::File& validProjectFile)
{
    const auto valid = bytesOf(validProjectFile);
    REQUIRE(valid.getSize() > 200);

    std::vector<Variant> variants;
    variants.push_back({"empty file", juce::MemoryBlock()});
    variants.push_back({"plain text", blockOf("this is not a project file")});
    variants.push_back({"xml of another kind", blockOf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<PLAYLIST/>\n")});
    variants.push_back(
        {"xml with a syntax error", blockOf("<?xml version=\"1.0\"?>\n<EDIT appVersion=\"1\"><TRACK></EDIT>\n")});

    juce::MemoryBlock truncated(valid.getData(), valid.getSize() / 2);
    variants.push_back({"truncated project file", truncated});

    juce::MemoryBlock noise;
    juce::Random random(20261009);  // fixed seed: the same bytes on every run
    for (int i = 0; i < 4096; ++i)
    {
        const auto byte = static_cast<char>(random.nextInt(256));
        noise.append(&byte, 1);
    }
    variants.push_back({"binary noise", noise});
    return variants;
}

}  // namespace

TEST_SUITE("project")
{
    TEST_CASE("an unreadable project file is reported as corrupt_project and stays exactly as it was")
    {
        ProjectFixture f;
        const auto valid = f.newProject("Gut");
        f.addSampleContent();
        f.run("project.save");
        f.run("project.close");

        for (const auto& variant : corruptVariants(valid))
        {
            INFO("variant: " << variant.name);
            const auto folder = f.root().getChildFile("Kaputt");
            folder.deleteRecursively();
            REQUIRE(folder.createDirectory().wasOk());
            const auto file = folder.getChildFile("Kaputt.tracklab");
            // replaceWithData() with 0 bytes would delete the file: an empty file is created explicitly
            if (variant.content.getSize() == 0)
                REQUIRE(file.create().wasOk());
            else
                REQUIRE(file.replaceWithData(variant.content.getData(), variant.content.getSize()));
            const auto before = tracklab_test::snapshotOf(folder);

            const auto result = f.tryRun("project.open", Json{{"path", utf8(file)}});

            CHECK_FALSE(result.ok);
            CHECK(result.error.code == "corrupt_project");
            CHECK(result.error.message.find("Kaputt.tracklab") != std::string::npos);
            CHECK(sameBytes(bytesOf(file), variant.content));
            CHECK(tracklab_test::snapshotOf(folder) == before);
            CHECK(f.session->edit() == nullptr);
            CHECK(f.context.edit() == nullptr);
        }
    }

    TEST_CASE("a corrupt file does not close the project that is open")
    {
        ProjectFixture f;
        const auto good = f.newProject("Gut");
        f.addSampleContent();
        f.run("project.save");
        f.settleEdit();
        auto* editBefore = f.session->edit();
        const auto stateBefore = tracklab_test::undo::normalisedState(f.edit());

        const auto folder = f.root().getChildFile("Kaputt");
        REQUIRE(folder.createDirectory().wasOk());
        const auto bad = folder.getChildFile("Kaputt.tracklab");
        writeText(bad, "kein Projekt");

        CHECK(f.errorOf("project.open", Json{{"path", utf8(bad)}}) == "corrupt_project");

        CHECK(f.session->edit() == editBefore);
        CHECK(f.context.edit() == editBefore);
        CHECK(f.info()["path"].get<std::string>() == utf8(good));
        CHECK_FALSE(f.modified());
        CHECK(tracklab_test::undo::normalisedState(f.edit()) == stateBefore);
    }
}
