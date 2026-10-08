// Engine factory (O-04): where Tracktion's cache folder lives.
// App mode (CacheMode::persistent) keeps a permanent cache that survives the engine; CLI and tests (default) use a
// private folder that is deleted with the engine. Folders of crashed engines are cleaned up at start. All base folders
// are injected, so the user's real cache and the system temp folder are never touched.
#include "engine/engine_factory.h"

#include "scoped_env.h"
#include "test_support.h"

#include <cstdlib>

namespace
{

using namespace tracklab::engine;

constexpr auto kPrivatePrefix = "tracklab-engine-cache";

struct Folders
{
    tracklab_test::ScopedTempDir root;
    juce::File settings = root.dir().getChildFile("settings");
    juce::File temp = root.dir().getChildFile("temp");
    juce::File cache = root.dir().getChildFile("user-cache").getChildFile("Tracklab");  // parent is missing

    Folders() { temp.createDirectory(); }

    EngineOptions options(CacheMode mode) const
    {
        EngineOptions o;
        o.storage = SettingsStorage::file;
        o.settingsDirectory = settings;
        o.cache = mode;
        o.cacheDirectory = cache;
        o.tempDirectory = temp;
        return o;
    }
};

/** Entries of `dir` whose name starts with the private cache prefix. */
juce::Array<juce::File> privateCachesIn(const juce::File& dir)
{
    return dir.findChildFiles(juce::File::findFilesAndDirectories, false, juce::String(kPrivatePrefix) + "*");
}

/** A folder like the one a crashed engine leaves behind: old (also inside), with content, used by nobody. */
juce::File makeOrphan(const juce::File& temp, const juce::String& name)
{
    const auto folder = temp.getChildFile(name);
    REQUIRE(folder.createDirectory().wasOk());
    const auto file = folder.getChildFile("thumbnail.tmp");
    REQUIRE(file.replaceWithText("Muster"));
    const auto old = juce::Time::getCurrentTime() - juce::RelativeTime::days(1);
    file.setLastModificationTime(old);
    folder.setLastModificationTime(old);
    return folder;
}

}  // namespace

TEST_SUITE("engine")
{
    TEST_CASE("the default cache mode of the options is a private cache")
    {
        CHECK(EngineOptions{}.cache == CacheMode::privateTemporary);
    }

    //==========================================================================
    TEST_CASE("persistent cache: getAppCacheFolder is the given folder and is created with its parents")
    {
        const Folders f;
        REQUIRE_FALSE(f.cache.exists());

        auto engine = createEngine(f.options(CacheMode::persistent));
        REQUIRE(engine != nullptr);
        CHECK(engine->getPropertyStorage().getAppCacheFolder().getFullPathName() == f.cache.getFullPathName());
        CHECK(f.cache.isDirectory());
    }

    TEST_CASE("persistent cache: the folder and its content survive the engine and are there in the next engine")
    {
        const Folders f;
        {
            auto engine = createEngine(f.options(CacheMode::persistent));
            REQUIRE(engine != nullptr);
            REQUIRE(
                engine->getPropertyStorage().getAppCacheFolder().getChildFile("thumbnail.tmp").replaceWithText("x"));
        }
        CHECK(f.cache.getChildFile("thumbnail.tmp").existsAsFile());

        auto engine = createEngine(f.options(CacheMode::persistent));
        REQUIRE(engine != nullptr);
        CHECK(engine->getPropertyStorage().getAppCacheFolder().getFullPathName() == f.cache.getFullPathName());
        CHECK(f.cache.getChildFile("thumbnail.tmp").existsAsFile());
    }

    TEST_CASE("persistent cache: no private cache folder is created in the temp folder")
    {
        const Folders f;
        {
            auto engine = createEngine(f.options(CacheMode::persistent));
            REQUIRE(engine != nullptr);
            CHECK(privateCachesIn(f.temp).isEmpty());
        }
        CHECK(privateCachesIn(f.temp).isEmpty());
    }

    TEST_CASE("persistent cache: the settings folder holds settings.xml only")
    {
        const Folders f;
        {
            auto engine = createEngine(f.options(CacheMode::persistent));
            REQUIRE(engine != nullptr);
            engine->getPropertyStorage().getAppCacheFolder().getChildFile("thumbnail.tmp").replaceWithText("x");
            engine->getPropertyStorage().setProperty(te::SettingID::compCrossfadeMs, 42);
            engine->getPropertyStorage().flushSettingsToDisk();
        }
        const auto files = f.settings.findChildFiles(juce::File::findFilesAndDirectories, true);
        CHECK(files.size() == 1);
    }

    //==========================================================================
    TEST_CASE("private cache (default): a folder in the temp folder, deleted together with the engine")
    {
        const Folders f;
        juce::File cacheFolder;
        {
            auto engine = createEngine(f.options(CacheMode::privateTemporary));
            REQUIRE(engine != nullptr);
            cacheFolder = engine->getPropertyStorage().getAppCacheFolder();
            CHECK(cacheFolder.isDirectory());
            CHECK(cacheFolder.getParentDirectory().getFullPathName() == f.temp.getFullPathName());
            CHECK(cacheFolder.getFileName().startsWith(kPrivatePrefix));
            CHECK(cacheFolder.getFullPathName() != f.cache.getFullPathName());
            cacheFolder.getChildFile("thumbnail.tmp").replaceWithText("x");
        }
        CHECK_FALSE(cacheFolder.exists());
        CHECK(privateCachesIn(f.temp).isEmpty());
        CHECK_FALSE(f.cache.exists());  // the persistent folder is not created in this mode
    }

    TEST_CASE("private cache: a CLI/test engine (DeviceMode::none) never uses the user cache")
    {
        const Folders f;
        auto options = f.options(CacheMode::privateTemporary);
        options.devices = DeviceMode::none;
        auto engine = createEngine(options);
        REQUIRE(engine != nullptr);
        CHECK(engine->getPropertyStorage().getAppCacheFolder().getFullPathName() != f.cache.getFullPathName());
        CHECK_FALSE(f.cache.exists());
    }

    //==========================================================================
    TEST_CASE("orphaned private caches (crashed engines) are deleted when an engine starts, private mode")
    {
        const Folders f;
        const auto orphanA = makeOrphan(f.temp, "tracklab-engine-cacheAAAA1111");
        const auto orphanB = makeOrphan(f.temp, "tracklab-engine-cacheBBBB2222");

        auto engine = createEngine(f.options(CacheMode::privateTemporary));
        REQUIRE(engine != nullptr);
        CHECK_FALSE(orphanA.exists());
        CHECK_FALSE(orphanB.exists());
    }

    TEST_CASE("orphaned private caches are deleted when an engine starts, persistent mode")
    {
        const Folders f;
        const auto orphan = makeOrphan(f.temp, "tracklab-engine-cacheAAAA1111");

        auto engine = createEngine(f.options(CacheMode::persistent));
        REQUIRE(engine != nullptr);
        CHECK_FALSE(orphan.exists());
    }

    TEST_CASE("the cache of a running engine is not deleted when another engine starts, even if it looks old")
    {
        const Folders f;
        auto running = createEngine(f.options(CacheMode::privateTemporary));
        REQUIRE(running != nullptr);
        const auto runningCache = running->getPropertyStorage().getAppCacheFolder();
        const auto marker = runningCache.getChildFile("thumbnail.tmp");
        REQUIRE(marker.replaceWithText("Muster"));
        // A long-running app: nothing has touched the folder for a day. Age alone must not make it an orphan.
        const auto old = juce::Time::getCurrentTime() - juce::RelativeTime::days(1);
        marker.setLastModificationTime(old);
        runningCache.setLastModificationTime(old);

        {
            auto second = createEngine(f.options(CacheMode::privateTemporary));
            REQUIRE(second != nullptr);
            CHECK(runningCache.isDirectory());
            CHECK(marker.existsAsFile());
            CHECK(second->getPropertyStorage().getAppCacheFolder().getFullPathName() != runningCache.getFullPathName());
        }

        // The second engine removes its own folder only.
        CHECK(runningCache.isDirectory());
        CHECK(marker.existsAsFile());
        CHECK(privateCachesIn(f.temp).size() == 1);

        running.reset();
        CHECK(privateCachesIn(f.temp).isEmpty());
    }

    TEST_CASE("the cleanup of orphaned caches leaves everything else in the temp folder alone")
    {
        const Folders f;
        const auto unrelated = makeOrphan(f.temp, "unrelated-folder");
        const auto otherEngineScratch = makeOrphan(f.temp, "tracklab-engine-scratch");
        const auto note = f.temp.getChildFile("tracklab-note.txt");
        REQUIRE(note.replaceWithText("Muster"));

        auto engine = createEngine(f.options(CacheMode::privateTemporary));
        REQUIRE(engine != nullptr);
        CHECK(unrelated.getChildFile("thumbnail.tmp").existsAsFile());
        CHECK(otherEngineScratch.getChildFile("thumbnail.tmp").existsAsFile());
        CHECK(note.existsAsFile());
    }

    //==========================================================================
#if JUCE_LINUX
    TEST_CASE("default cache folder (Linux): XDG_CACHE_HOME/Tracklab")
    {
        const tracklab_test::ScopedTempDir temp;
        const auto base = temp.dir().getChildFile("xdg");
        const tracklab_test::ScopedEnv xdg("XDG_CACHE_HOME", base.getFullPathName().toRawUTF8());

        CHECK(defaultCacheDirectory().getFullPathName() == base.getChildFile("Tracklab").getFullPathName());
        CHECK_FALSE(base.exists());  // not created by asking
    }

    TEST_CASE("default cache folder (Linux): ~/.cache/Tracklab if XDG_CACHE_HOME is unset or empty")
    {
        const tracklab_test::ScopedTempDir temp;
        const auto home = temp.dir().getChildFile("home");
        const tracklab_test::ScopedEnv homeEnv("HOME", home.getFullPathName().toRawUTF8());
        const auto expected = home.getChildFile(".cache").getChildFile("Tracklab").getFullPathName();

        {
            const tracklab_test::ScopedEnv xdg("XDG_CACHE_HOME", nullptr);
            CHECK(defaultCacheDirectory().getFullPathName() == expected);
        }
        {
            const tracklab_test::ScopedEnv xdg("XDG_CACHE_HOME", "");
            CHECK(defaultCacheDirectory().getFullPathName() == expected);
        }
        CHECK_FALSE(home.exists());
    }

    TEST_CASE("persistent cache without an explicit folder uses the default cache folder")
    {
        const Folders f;
        const auto base = f.root.dir().getChildFile("xdg");
        const tracklab_test::ScopedEnv xdg("XDG_CACHE_HOME", base.getFullPathName().toRawUTF8());

        auto options = f.options(CacheMode::persistent);
        options.cacheDirectory = juce::File();  // empty = defaultCacheDirectory()
        auto engine = createEngine(options);
        REQUIRE(engine != nullptr);
        CHECK(engine->getPropertyStorage().getAppCacheFolder().getFullPathName() ==
              base.getChildFile("Tracklab").getFullPathName());
    }
#endif

#if JUCE_WINDOWS
    TEST_CASE("default cache folder (Windows): %LOCALAPPDATA%\\Tracklab\\cache")
    {
        const char* local = std::getenv("LOCALAPPDATA");
        REQUIRE(local != nullptr);
        CHECK(defaultCacheDirectory().getFullPathName() ==
              juce::File(juce::String(local)).getChildFile("Tracklab").getChildFile("cache").getFullPathName());
    }
#endif
}
