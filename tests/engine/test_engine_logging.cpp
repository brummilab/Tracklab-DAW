// Engine factory (O-04): log lines of the factory and the settings storage never contain the user's home path
// (it holds the account name and log files can be shared); the home folder is written as "~".
#include "engine/engine_factory.h"
#include "engine/settings_storage.h"

#include "scoped_env.h"
#include "test_support.h"

namespace
{

using namespace tracklab::engine;

/** Collects every message written through juce::Logger while it is alive; restores the previous logger afterwards. */
class CapturingLogger final : public juce::Logger
{
public:
    CapturingLogger() : previous(juce::Logger::getCurrentLogger()) { juce::Logger::setCurrentLogger(this); }
    ~CapturingLogger() override { juce::Logger::setCurrentLogger(previous); }

    juce::String all() const
    {
        const juce::ScopedLock guard(lock);
        return messages.joinIntoString("\n");
    }

    int count() const
    {
        const juce::ScopedLock guard(lock);
        return messages.size();
    }

private:
    juce::Logger* previous;
    mutable juce::CriticalSection lock;
    juce::StringArray messages;

    void logMessage(const juce::String& message) override
    {
        const juce::ScopedLock guard(lock);
        messages.add(message);
    }
};

/** Normalised to '/' so that the expectations do not depend on the separator the implementation prints. */
juce::String slashed(const juce::String& path)
{
    return path.replaceCharacter('\\', '/');
}

}  // namespace

TEST_SUITE("engine")
{
    TEST_CASE("pathForLog replaces the home folder by ~")
    {
        const auto home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
        REQUIRE(home != juce::File());

        CHECK(slashed(detail::pathForLog(home)) == "~");
        CHECK(slashed(detail::pathForLog(home.getChildFile("Muster"))) == "~/Muster");
        CHECK(slashed(detail::pathForLog(home.getChildFile("Muster").getChildFile("settings.xml"))) ==
              "~/Muster/settings.xml");
    }

    TEST_CASE("pathForLog leaves paths outside the home folder unchanged")
    {
        const tracklab_test::ScopedTempDir temp;
        const auto home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
        const auto outside = temp.dir().getChildFile("settings.xml");
        if (outside.isAChildOf(home) || outside == home)
        {
            tracklab_test::skipTest("the temp folder is inside the home folder");
            return;
        }
        CHECK(detail::pathForLog(outside) == outside.getFullPathName());
    }

    TEST_CASE("pathForLog replaces whole path components only")
    {
        const auto home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
        REQUIRE(home != juce::File());

        // Same characters at the start, but a different folder: not inside the home folder.
        const auto sibling = home.getSiblingFile(home.getFileName() + "-andere").getChildFile("Muster");
        CHECK(detail::pathForLog(sibling) == sibling.getFullPathName());
        CHECK_FALSE(detail::pathForLog(sibling).startsWith("~"));
    }

#if JUCE_LINUX
    // The home folder comes from $HOME on Linux, so a temporary folder can play the part of the home folder.
    TEST_CASE("an unreadable settings file under the home folder is logged with ~, not the home path")
    {
        const tracklab_test::ScopedTempDir temp;
        const auto home = temp.dir().getChildFile("home-Muster");
        const auto settingsDir = home.getChildFile(".config").getChildFile("Tracklab");
        REQUIRE(settingsDir.createDirectory().wasOk());
        REQUIRE(settingsDir.getChildFile("settings.xml").replaceWithText("<PROPERTIES><VALUE name=\"truncated"));

        const tracklab_test::ScopedEnv homeEnv("HOME", home.getFullPathName().toRawUTF8());
        REQUIRE(juce::File::getSpecialLocation(juce::File::userHomeDirectory) == home);

        juce::String logged;
        int lines = 0;
        {
            const CapturingLogger logger;
            EngineOptions options;
            options.storage = SettingsStorage::file;
            options.settingsDirectory = settingsDir;
            options.tempDirectory = temp.dir();
            auto engine = createEngine(options);
            REQUIRE(engine != nullptr);
            logged = logger.all();
            lines = logger.count();
        }

        REQUIRE(lines > 0);  // the corrupt file has to be reported, otherwise this test proves nothing
        CHECK_FALSE(logged.contains(home.getFullPathName()));
        CHECK(logged.contains("~"));
        CHECK(logged.contains("settings.xml"));
    }

    TEST_CASE("a failed atomic write under the home folder is logged with ~, not the home path")
    {
        const tracklab_test::ScopedTempDir temp;
        const auto home = temp.dir().getChildFile("home-Muster");
        REQUIRE(home.createDirectory().wasOk());
        // A file where the folder of the target should be: the folder cannot be created, also as root.
        const auto blocker = home.getChildFile("blocker");
        REQUIRE(blocker.replaceWithText("Muster"));
        const auto target = blocker.getChildFile("Tracklab").getChildFile("settings.xml");

        const tracklab_test::ScopedEnv homeEnv("HOME", home.getFullPathName().toRawUTF8());

        bool written = true;
        juce::String logged;
        int lines = 0;
        {
            const CapturingLogger logger;
            written = detail::writeFileAtomically(target, "<PROPERTIES/>");
            logged = logger.all();
            lines = logger.count();
        }

        CHECK_FALSE(written);
        REQUIRE(lines > 0);
        CHECK_FALSE(logged.contains(home.getFullPathName()));
        CHECK(logged.contains("~"));
    }
#endif
}
