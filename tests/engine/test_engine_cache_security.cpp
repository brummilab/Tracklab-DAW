// Engine factory (O-04, review round 1): the private caches must not be a way for another user to make this user's
// cleanup delete things. The base folder of the private caches belongs to the user alone (mode 0700); the cleanup
// skips symbolic links, foreign folders and locks that do not belong to the user.
#include "engine/cache_folder.h"
#include "engine/engine_factory.h"

#include "scoped_env.h"
#include "test_support.h"

#if JUCE_LINUX
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{

using namespace tracklab::engine;

#if JUCE_LINUX
constexpr unsigned kOtherUser = 65534;  // "nobody": any user that is not the one running the tests

unsigned modeOf(const juce::File& file)
{
    struct stat st
    {
    };
    REQUIRE(::stat(file.getFullPathName().toRawUTF8(), &st) == 0);
    return static_cast<unsigned>(st.st_mode) & 0777U;
}

bool ownedByUs(const juce::File& file)
{
    struct stat st
    {
    };
    REQUIRE(::stat(file.getFullPathName().toRawUTF8(), &st) == 0);
    return st.st_uid == ::geteuid();
}

/** Giving a file to another user needs root; the tests that do it are skipped (exit 77) otherwise. */
bool canChangeOwner()
{
    return ::geteuid() == 0;
}

void giveToOtherUser(const juce::File& file)
{
    REQUIRE(::chown(file.getFullPathName().toRawUTF8(), kOtherUser, kOtherUser) == 0);
}

/** A folder like the one a crashed engine leaves behind, with content and a day old. */
juce::File makeOrphan(const juce::File& base, const juce::String& name)
{
    const auto folder = base.getChildFile(name);
    REQUIRE(folder.createDirectory().wasOk());
    REQUIRE(folder.getChildFile("thumbnail.tmp").replaceWithText("Muster"));
    folder.setLastModificationTime(juce::Time::getCurrentTime() - juce::RelativeTime::days(1));
    return folder;
}

/** A user environment inside a temporary folder: HOME, the XDG folders. Nothing of the real user is read or made. */
struct FakeUser
{
    tracklab_test::ScopedTempDir root;
    juce::File home = root.dir().getChildFile("home");
    juce::File runtime = root.dir().getChildFile("runtime");
    juce::File cacheHome = root.dir().getChildFile("xdg-cache");
    tracklab_test::ScopedEnv homeEnv{"HOME", home.getFullPathName().toRawUTF8()};
    tracklab_test::ScopedEnv runtimeEnv{"XDG_RUNTIME_DIR", runtime.getFullPathName().toRawUTF8()};
    tracklab_test::ScopedEnv cacheEnv{"XDG_CACHE_HOME", cacheHome.getFullPathName().toRawUTF8()};

    FakeUser()
    {
        home.createDirectory();
        runtime.createDirectory();
    }
};
#endif

}  // namespace

TEST_SUITE("engine")
{
    TEST_CASE("the base folder given in the options is used as it is and may be cleaned")
    {
        const tracklab_test::ScopedTempDir temp;
        const auto base = detail::resolvePrivateBase(temp.dir());
        CHECK(base.folder == temp.dir());
        CHECK(base.cleanable);
    }

#if JUCE_LINUX
    TEST_CASE("default base of the private caches: $XDG_RUNTIME_DIR/tracklab, mode 0700, owned by the user")
    {
        const FakeUser user;
        const auto base = detail::resolvePrivateBase({});
        CHECK(base.folder.getFullPathName() == user.runtime.getChildFile("tracklab").getFullPathName());
        CHECK(base.cleanable);
        CHECK(base.folder.isDirectory());
        CHECK(modeOf(base.folder) == 0700U);
        CHECK(ownedByUs(base.folder));
    }

    TEST_CASE("default base without $XDG_RUNTIME_DIR: <cache folder>/private, mode 0700, owned by the user")
    {
        const FakeUser user;
        const tracklab_test::ScopedEnv noRuntime("XDG_RUNTIME_DIR", nullptr);
        const auto base = detail::resolvePrivateBase({});
        CHECK(base.folder.getFullPathName() ==
              user.cacheHome.getChildFile("Tracklab").getChildFile("private").getFullPathName());
        CHECK(base.cleanable);
        CHECK(modeOf(base.folder) == 0700U);
        CHECK(ownedByUs(base.folder));
    }

    TEST_CASE("default base: a relative $XDG_RUNTIME_DIR is ignored")
    {
        const FakeUser user;
        const tracklab_test::ScopedEnv relative("XDG_RUNTIME_DIR", "relative/runtime");
        const auto base = detail::resolvePrivateBase({});
        CHECK(base.folder.getFullPathName() ==
              user.cacheHome.getChildFile("Tracklab").getChildFile("private").getFullPathName());
        CHECK_FALSE(juce::File::getCurrentWorkingDirectory().getChildFile("relative").exists());
    }

    TEST_CASE("default base: a $XDG_RUNTIME_DIR of another user is not used")
    {
        if (!canChangeOwner())
        {
            tracklab_test::skipTest("needs root to give a folder to another user");
            return;
        }
        const FakeUser user;
        giveToOtherUser(user.runtime);
        const auto base = detail::resolvePrivateBase({});
        CHECK(base.folder.getFullPathName() ==
              user.cacheHome.getChildFile("Tracklab").getChildFile("private").getFullPathName());
        CHECK_FALSE(user.runtime.getChildFile("tracklab").exists());
    }

    TEST_CASE("default base: a folder that belongs to another user ends in the shared temp folder, no cleanup")
    {
        if (!canChangeOwner())
        {
            tracklab_test::skipTest("needs root to give a folder to another user");
            return;
        }
        const FakeUser user;
        const auto planted = user.cacheHome.getChildFile("Tracklab").getChildFile("private");
        REQUIRE(planted.createDirectory().wasOk());
        giveToOtherUser(planted);
        const tracklab_test::ScopedEnv noRuntime("XDG_RUNTIME_DIR", nullptr);

        const auto base = detail::resolvePrivateBase({});
        CHECK_FALSE(base.cleanable);
        CHECK(base.folder == juce::File::getSpecialLocation(juce::File::tempDirectory));
    }

    TEST_CASE("default base: a symbolic link in place of the folder is not used")
    {
        const FakeUser user;
        const tracklab_test::ScopedEnv noRuntime("XDG_RUNTIME_DIR", nullptr);
        const auto target = user.root.dir().getChildFile("elsewhere");
        REQUIRE(target.createDirectory().wasOk());
        const auto link = user.cacheHome.getChildFile("Tracklab").getChildFile("private");
        REQUIRE(link.getParentDirectory().createDirectory().wasOk());
        REQUIRE(juce::File::createSymbolicLink(link, target.getFullPathName(), false));

        const auto base = detail::resolvePrivateBase({});
        CHECK_FALSE(base.cleanable);
        CHECK(target.findChildFiles(juce::File::findFilesAndDirectories, false).isEmpty());
    }

    TEST_CASE("an engine without tempDirectory keeps its private cache in the base of the user, mode 0700")
    {
        const FakeUser user;
        EngineOptions options;  // tempDirectory left empty
        auto engine = createEngine(options);
        REQUIRE(engine != nullptr);

        const auto cache = engine->getPropertyStorage().getAppCacheFolder();
        CHECK(cache.getParentDirectory().getFullPathName() == user.runtime.getChildFile("tracklab").getFullPathName());
        CHECK(cache.getFileName().startsWith("tracklab-engine-cache"));
        CHECK(modeOf(cache) == 0700U);
        CHECK(ownedByUs(cache));
    }

    //==========================================================================
    TEST_CASE("cleanup: a symbolic link named like a private cache is left alone, and so is its target")
    {
        const tracklab_test::ScopedTempDir temp;
        const auto base = temp.dir().getChildFile("base");
        REQUIRE(base.createDirectory().wasOk());
        const auto target = makeOrphan(temp.dir(), "precious-folder");
        const auto link = base.getChildFile("tracklab-engine-cacheLINK0001");
        REQUIRE(juce::File::createSymbolicLink(link, target.getFullPathName(), false));

        detail::removeOrphanedPrivateCaches(base);

        CHECK(link.isSymbolicLink());
        CHECK(target.getChildFile("thumbnail.tmp").existsAsFile());
    }

    TEST_CASE("cleanup: a folder that belongs to another user is skipped")
    {
        if (!canChangeOwner())
        {
            tracklab_test::skipTest("needs root to give a folder to another user");
            return;
        }
        const tracklab_test::ScopedTempDir temp;
        const auto foreign = makeOrphan(temp.dir(), "tracklab-engine-cacheFOREIGN1");
        giveToOtherUser(foreign);
        const auto own = makeOrphan(temp.dir(), "tracklab-engine-cacheOWN00001");

        detail::removeOrphanedPrivateCaches(temp.dir());

        CHECK(foreign.getChildFile("thumbnail.tmp").existsAsFile());
        CHECK_FALSE(own.exists());
    }

    TEST_CASE("cleanup: a lock file of another user does not make a folder an orphan")
    {
        if (!canChangeOwner())
        {
            tracklab_test::skipTest("needs root to give a file to another user");
            return;
        }
        const tracklab_test::ScopedTempDir temp;
        const auto folder = makeOrphan(temp.dir(), "tracklab-engine-cacheLOCKED01");
        // JUCE cannot open this lock file as a normal user and still reports the lock as taken.
        const auto lockFile = detail::lockFileFor(folder.getFileName());
        REQUIRE(lockFile != juce::File());
        REQUIRE(lockFile.create().wasOk());
        giveToOtherUser(lockFile);

        detail::removeOrphanedPrivateCaches(temp.dir());

        const auto stillThere = folder.getChildFile("thumbnail.tmp").existsAsFile();
        const auto lockStillThere = lockFile.existsAsFile();
        lockFile.deleteFile();
        CHECK(stillThere);
        CHECK(lockStillThere);  // not ours: not deleted either
    }

    TEST_CASE("the lock file of a finished engine is gone")
    {
        const tracklab_test::ScopedTempDir temp;
        juce::File lockFile;
        {
            EngineOptions options;
            options.tempDirectory = temp.dir();
            auto engine = createEngine(options);
            REQUIRE(engine != nullptr);
            const auto cache = engine->getPropertyStorage().getAppCacheFolder();
            lockFile = detail::lockFileFor(cache.getFileName());
            CHECK(lockFile.existsAsFile());  // held while the engine runs
        }
        CHECK_FALSE(lockFile.exists());
    }

    TEST_CASE("default cache folder (Linux): a relative XDG_CACHE_HOME counts as not set")
    {
        const tracklab_test::ScopedTempDir temp;
        const auto home = temp.dir().getChildFile("home");
        const tracklab_test::ScopedEnv homeEnv("HOME", home.getFullPathName().toRawUTF8());
        const tracklab_test::ScopedEnv xdg("XDG_CACHE_HOME", "relative/cache");

        CHECK(defaultCacheDirectory().getFullPathName() ==
              home.getChildFile(".cache").getChildFile("Tracklab").getFullPathName());
    }
#endif
}
