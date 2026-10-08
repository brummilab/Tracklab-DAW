// Engine factory (M1-01): a failed settings write leaves the existing settings.xml intact.
//
// A write that cannot create its temporary file next to the target must not touch the target. This is simulated with a
// read-only settings folder, which needs a file system that enforces folder permissions: as root (or on a file system
// that ignores them) the test is skipped. It lives in its own suite so that the skip hides nothing else.
#include "engine/engine_factory.h"

#include "test_support.h"

namespace
{

using namespace tracklab::engine;

EngineOptions fileOptions(const juce::File& dir)
{
    EngineOptions options;
    options.storage = SettingsStorage::file;
    options.settingsDirectory = dir;
    return options;
}

/** Makes a folder read-only for the life of the object and restores it afterwards (also if a check fails). */
class ReadOnlyFolder
{
public:
    explicit ReadOnlyFolder(juce::File f) : folder(std::move(f)) { folder.setReadOnly(true, false); }
    ~ReadOnlyFolder() { folder.setReadOnly(false, false); }

    ReadOnlyFolder(const ReadOnlyFolder&) = delete;
    ReadOnlyFolder& operator=(const ReadOnlyFolder&) = delete;

    /** True if creating a file in the folder really fails now. */
    bool isEnforced() const
    {
        const auto probe = folder.getChildFile("probe.tmp");
        const bool created = probe.create().wasOk();
        probe.deleteFile();
        return !created;
    }

private:
    juce::File folder;
};

}  // namespace

TEST_SUITE("engine_readonly")
{
    TEST_CASE("a failed settings write keeps the existing settings.xml unchanged")
    {
        const tracklab_test::ScopedTempDir temp;
        const auto settingsFile = temp.dir().getChildFile("settings.xml");

        {
            auto engine = createEngine(fileOptions(temp.dir()));
            REQUIRE(engine != nullptr);
            engine->getPropertyStorage().setProperty(te::SettingID::compCrossfadeMs, 42);
            engine->getPropertyStorage().flushSettingsToDisk();
        }
        REQUIRE(settingsFile.existsAsFile());
        const auto contentBefore = settingsFile.loadFileAsString();
        REQUIRE(contentBefore.isNotEmpty());

        const ReadOnlyFolder readOnly(temp.dir());
        if (!readOnly.isEnforced())
        {
            tracklab_test::skipTest("folder permissions are not enforced (root or file system without permissions)");
            return;
        }

        {
            auto engine = createEngine(fileOptions(temp.dir()));
            REQUIRE(engine != nullptr);
            engine->getPropertyStorage().setProperty(te::SettingID::compCrossfadeMs, 99);
            CHECK_NOTHROW(engine->getPropertyStorage().flushSettingsToDisk());
        }

        // Atomic means: the target was either replaced completely or not at all. Here the replacement cannot happen.
        CHECK(settingsFile.loadFileAsString() == contentBefore);
    }
}
