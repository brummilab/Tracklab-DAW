// Engine factory (M1-01): headless creation, settings storage (in memory / file), user name, version, UIBehaviour.
// Only the headless variants are used: no audio device is touched, no dialog is shown, and the user's real settings
// folder stays untouched (file storage always points at a temporary folder).
#include "engine/engine_factory.h"

#include "engine_test_options.h"
#include "test_support.h"

#include <atomic>
#include <chrono>

namespace
{

using namespace tracklab::engine;

constexpr auto kProbeSetting = te::SettingID::compCrossfadeMs;

EngineOptions fileOptions(const juce::File& dir)
{
    auto options = tracklab_test::testOptions();
    options.storage = SettingsStorage::file;
    options.settingsDirectory = dir;
    return options;
}

juce::File settingsFileIn(const juce::File& dir)
{
    return dir.getChildFile("settings.xml");
}

/** A job that finishes at once; records that it ran. */
class QuickJob final : public te::ThreadPoolJobWithProgress
{
public:
    QuickJob() : te::ThreadPoolJobWithProgress("quick job") {}

    JobStatus runJob() override
    {
        ran = true;
        return jobHasFinished;
    }

    float getCurrentTaskProgress() override { return 1.0f; }

    std::atomic<bool> ran{false};
};

}  // namespace

TEST_SUITE("engine")
{
    TEST_CASE("a headless engine can be created and destroyed twice in a row")
    {
        for (int round = 0; round < 2; ++round)
        {
            auto engine = createEngine(tracklab_test::testOptions());
            REQUIRE(engine != nullptr);
            // Touching a sub-object proves that the engine is fully constructed, not just allocated.
            CHECK(engine->getUIBehaviour().getAllOpenEdits().isEmpty());
            engine.reset();
            CHECK(engine == nullptr);
        }
    }

    TEST_CASE("two engines with file storage can follow each other in one process")
    {
        const tracklab_test::ScopedTempDir temp;
        for (int round = 0; round < 2; ++round)
        {
            auto engine = createEngine(fileOptions(temp.dir()));
            REQUIRE(engine != nullptr);
        }
    }

    TEST_CASE("DeviceMode::none opens no audio device")
    {
        auto options = tracklab_test::testOptions();
        options.devices = DeviceMode::none;
        auto engine = createEngine(options);
        REQUIRE(engine != nullptr);
        CHECK(engine->getDeviceManager().deviceManager.getCurrentAudioDevice() == nullptr);
    }

    TEST_CASE("the default settings folder is Tracklab inside the user application data folder")
    {
        const auto expected =
            juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Tracklab");
        CHECK(defaultSettingsDirectory().getFullPathName() == expected.getFullPathName());
    }

    //==========================================================================
    TEST_CASE("in-memory storage keeps its values for the life of the engine")
    {
        auto engine = createEngine(tracklab_test::testOptions());
        REQUIRE(engine != nullptr);
        auto& storage = engine->getPropertyStorage();
        CHECK(static_cast<int>(storage.getProperty(kProbeSetting, 7)) == 7);
        storage.setProperty(kProbeSetting, 42);
        CHECK(static_cast<int>(storage.getProperty(kProbeSetting, 7)) == 42);
    }

    TEST_CASE("in-memory storage writes no settings file")
    {
        const tracklab_test::ScopedTempDir temp;
        const auto userFolder = defaultSettingsDirectory();
        const auto userFolderBefore = tracklab_test::snapshotOf(userFolder);

        auto options = tracklab_test::testOptions();
        options.storage = SettingsStorage::inMemory;
        options.settingsDirectory = temp.dir();  // has to be ignored by the in-memory variant
        {
            auto engine = createEngine(options);
            REQUIRE(engine != nullptr);
            engine->getPropertyStorage().setProperty(kProbeSetting, 42);
            engine->getPropertyStorage().flushSettingsToDisk();
        }

        CHECK(tracklab_test::snapshotOf(temp.dir()).isEmpty());
        CHECK_FALSE(settingsFileIn(temp.dir()).exists());
        CHECK(tracklab_test::snapshotOf(userFolder) == userFolderBefore);
    }

    TEST_CASE("in-memory storage does not carry values from one engine to the next")
    {
        {
            auto engine = createEngine(tracklab_test::testOptions());
            REQUIRE(engine != nullptr);
            engine->getPropertyStorage().setProperty(kProbeSetting, 42);
        }
        auto engine = createEngine(tracklab_test::testOptions());
        REQUIRE(engine != nullptr);
        CHECK(static_cast<int>(engine->getPropertyStorage().getProperty(kProbeSetting, 7)) == 7);
    }

    //==========================================================================
    TEST_CASE("file storage writes settings.xml on flush and leaves no temporary file behind")
    {
        const tracklab_test::ScopedTempDir temp;
        auto engine = createEngine(fileOptions(temp.dir()));
        REQUIRE(engine != nullptr);

        engine->getPropertyStorage().setProperty(kProbeSetting, 42);
        engine->getPropertyStorage().flushSettingsToDisk();

        const auto file = settingsFileIn(temp.dir());
        REQUIRE(file.existsAsFile());
        CHECK(juce::parseXML(file) != nullptr);

        // The temporary file of the atomic write lives next to the target and is gone after the flush.
        const auto files = temp.dir().findChildFiles(juce::File::findFilesAndDirectories, true);
        CHECK(files.size() == 1);
    }

    TEST_CASE("file storage reads the values back in the next engine")
    {
        const tracklab_test::ScopedTempDir temp;
        {
            auto engine = createEngine(fileOptions(temp.dir()));
            REQUIRE(engine != nullptr);
            engine->getPropertyStorage().setProperty(kProbeSetting, 42);
            engine->getPropertyStorage().flushSettingsToDisk();
        }
        auto engine = createEngine(fileOptions(temp.dir()));
        REQUIRE(engine != nullptr);
        CHECK(static_cast<int>(engine->getPropertyStorage().getProperty(kProbeSetting, 7)) == 42);
    }

    TEST_CASE("file storage writes pending values when the engine is destroyed")
    {
        const tracklab_test::ScopedTempDir temp;
        {
            auto engine = createEngine(fileOptions(temp.dir()));
            REQUIRE(engine != nullptr);
            engine->getPropertyStorage().setProperty(kProbeSetting, 42);
        }
        REQUIRE(settingsFileIn(temp.dir()).existsAsFile());
        auto engine = createEngine(fileOptions(temp.dir()));
        REQUIRE(engine != nullptr);
        CHECK(static_cast<int>(engine->getPropertyStorage().getProperty(kProbeSetting, 7)) == 42);
    }

    TEST_CASE("file storage keeps XML items and property items")
    {
        const tracklab_test::ScopedTempDir temp;
        {
            auto engine = createEngine(fileOptions(temp.dir()));
            REQUIRE(engine != nullptr);
            auto& storage = engine->getPropertyStorage();
            storage.setPropertyItem(kProbeSetting, "item", "value");
            const juce::XmlElement xml("Muster");
            storage.setXmlProperty(te::SettingID::customMidiControllers, xml);
            storage.flushSettingsToDisk();
        }
        auto engine = createEngine(fileOptions(temp.dir()));
        REQUIRE(engine != nullptr);
        auto& storage = engine->getPropertyStorage();
        CHECK(storage.getPropertyItem(kProbeSetting, "item", "none").toString() == "value");
        const auto xml = storage.getXmlProperty(te::SettingID::customMidiControllers);
        REQUIRE(xml != nullptr);
        CHECK(xml->getTagName() == "Muster");
    }

    TEST_CASE("file storage creates a missing settings folder")
    {
        const tracklab_test::ScopedTempDir temp;
        const auto missing = temp.dir().getChildFile("Tracklab");
        REQUIRE_FALSE(missing.exists());

        auto engine = createEngine(fileOptions(missing));
        REQUIRE(engine != nullptr);
        engine->getPropertyStorage().setProperty(kProbeSetting, 42);
        engine->getPropertyStorage().flushSettingsToDisk();
        CHECK(settingsFileIn(missing).existsAsFile());
    }

    TEST_CASE("file storage starts with defaults when settings.xml is unreadable and rewrites it as valid XML")
    {
        const tracklab_test::ScopedTempDir temp;
        REQUIRE(settingsFileIn(temp.dir()).replaceWithText("<PROPERTIES><VALUE name=\"truncated"));

        auto engine = createEngine(fileOptions(temp.dir()));
        REQUIRE(engine != nullptr);
        CHECK(static_cast<int>(engine->getPropertyStorage().getProperty(kProbeSetting, 7)) == 7);

        engine->getPropertyStorage().setProperty(kProbeSetting, 42);
        engine->getPropertyStorage().flushSettingsToDisk();
        CHECK(juce::parseXML(settingsFileIn(temp.dir())) != nullptr);
    }

    //==========================================================================
    TEST_CASE("the user name is Tracklab, not the system user (in-memory storage)")
    {
        auto engine = createEngine(tracklab_test::testOptions());
        REQUIRE(engine != nullptr);
        const auto name = engine->getPropertyStorage().getUserName();
        CHECK(name == "Tracklab");
        // For any account that is not itself called "Tracklab" this also proves that no system name is used.
        if (juce::SystemStats::getFullUserName() != "Tracklab")
            CHECK(name != juce::SystemStats::getFullUserName());
        if (juce::SystemStats::getLogonName() != "Tracklab")
            CHECK(name != juce::SystemStats::getLogonName());
    }

    TEST_CASE("the user name is Tracklab, not the system user (file storage)")
    {
        const tracklab_test::ScopedTempDir temp;
        auto engine = createEngine(fileOptions(temp.dir()));
        REQUIRE(engine != nullptr);
        CHECK(engine->getPropertyStorage().getUserName() == "Tracklab");
    }

    TEST_CASE("the application version is the CMake project version (in-memory storage)")
    {
        auto engine = createEngine(tracklab_test::testOptions());
        REQUIRE(engine != nullptr);
        CHECK(engine->getPropertyStorage().getApplicationVersion() == juce::String(TRACKLAB_EXPECTED_VERSION));
    }

    TEST_CASE("the application version is the CMake project version (file storage)")
    {
        const tracklab_test::ScopedTempDir temp;
        auto engine = createEngine(fileOptions(temp.dir()));
        REQUIRE(engine != nullptr);
        CHECK(engine->getPropertyStorage().getApplicationVersion() == juce::String(TRACKLAB_EXPECTED_VERSION));
    }

    //==========================================================================
    TEST_CASE("the UIBehaviour runs a progress task to completion without blocking")
    {
        auto engine = createEngine(tracklab_test::testOptions());
        REQUIRE(engine != nullptr);

        QuickJob job;
        const auto start = std::chrono::steady_clock::now();
        engine->getUIBehaviour().runTaskWithProgressBar(job);
        const auto elapsed = std::chrono::steady_clock::now() - start;

        CHECK(job.ran.load());
        CHECK(elapsed < std::chrono::seconds(5));
    }

    TEST_CASE("the UIBehaviour opens no window for alerts and messages")
    {
        auto engine = createEngine(tracklab_test::testOptions());
        REQUIRE(engine != nullptr);
        auto& ui = engine->getUIBehaviour();

        ui.showWarningAlert("Muster", "Beispiel");
        ui.showWarningMessage("Beispiel");
        ui.showInfoMessage("Beispiel");
        // Windows are created asynchronously: let the message queue run before looking.
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);

        CHECK(juce::Desktop::getInstance().getNumComponents() == 0);
    }

    TEST_CASE("the UIBehaviour opens no window for confirmation requests and never confirms them")
    {
        auto engine = createEngine(tracklab_test::testOptions());
        REQUIRE(engine != nullptr);
        auto& ui = engine->getUIBehaviour();

        bool okConfirmed = false;
        int yesNoCancelAnswer = 0;  // 1 = yes, 2 = no, 0 = cancel
        ui.showOkCancelAlertBoxAsync("Muster", "Beispiel", {}, {}, [&](bool ok) { okConfirmed = ok; });
        ui.showYesNoCancelAlertBoxAsync("Muster", "Beispiel", {}, {}, {},
                                        [&](int answer) { yesNoCancelAnswer = answer; });
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);

        CHECK(juce::Desktop::getInstance().getNumComponents() == 0);
        // Whether the callback is called (with "cancel") or not is up to the implementation; "yes" never is.
        CHECK_FALSE(okConfirmed);
        CHECK(yesNoCancelAnswer != 1);
    }
}
