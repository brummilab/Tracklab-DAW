// Engine factory (M1-01), additions for the lead decisions after the first test round:
//   1  writes are collected (no IO per setProperty) and done by a timer, flush or destruction
//   3  an unreadable settings.xml is kept as settings.xml.corrupt-<date>-<time> before it is rewritten
//   4  confirmation requests of the headless UIBehaviour are answered with "cancel" at once
//   7  juce::var types (int, int64, double, bool, String) survive a restart without loss
#include "engine/engine_factory.h"

#include "engine_test_options.h"
#include "test_support.h"

#include <cmath>
#include <limits>

namespace
{

using namespace tracklab::engine;

EngineOptions fileOptions(const juce::File& dir)
{
    auto options = tracklab_test::testOptions();
    options.storage = SettingsStorage::file;
    options.settingsDirectory = dir;
    return options;
}

/** Stores `value` under a probe setting, restarts the engine and returns what the next engine reads. */
juce::var roundTrip(const juce::var& value)
{
    const tracklab_test::ScopedTempDir temp;
    {
        auto engine = createEngine(fileOptions(temp.dir()));
        engine->getPropertyStorage().setProperty(te::SettingID::compCrossfadeMs, value);
        engine->getPropertyStorage().flushSettingsToDisk();
    }
    auto engine = createEngine(fileOptions(temp.dir()));
    return engine->getPropertyStorage().getProperty(te::SettingID::compCrossfadeMs, "missing");
}

}  // namespace

TEST_SUITE("engine")
{
    TEST_CASE("file storage keeps the type and the exact value of a setting across a restart")
    {
        const auto asInt = roundTrip(juce::var(-123456));
        CHECK(asInt.isInt());
        CHECK(static_cast<int>(asInt) == -123456);

        const auto asInt64 = roundTrip(juce::var(static_cast<juce::int64>(1) << 40));
        CHECK(asInt64.isInt64());
        CHECK(static_cast<juce::int64>(asInt64) == (static_cast<juce::int64>(1) << 40));

        for (const double d : {0.1, -2.5e-300, 1.0 / 3.0, 123456789.123456789, std::numeric_limits<double>::max(), 0.0})
        {
            const auto back = roundTrip(juce::var(d));
            CHECK(back.isDouble());
            CHECK(static_cast<double>(back) == d);  // bit-exact, not "close"
        }

        const auto yes = roundTrip(juce::var(true));
        const auto no = roundTrip(juce::var(false));
        CHECK(yes.isBool());
        CHECK(no.isBool());
        CHECK(static_cast<bool>(yes));
        CHECK_FALSE(static_cast<bool>(no));

        const juce::String text = juce::String::fromUTF8("Beispiel <a> & \"b\"\nzweite Zeile \xc3\xa4\xe2\x82\xac");
        const auto asString = roundTrip(juce::var(text));
        CHECK(asString.isString());
        CHECK(asString.toString() == text);

        CHECK(roundTrip(juce::var("")).toString().isEmpty());
    }

    TEST_CASE("file storage does not write on setProperty, only on timer, flush or destruction")
    {
        const tracklab_test::ScopedTempDir temp;
        const auto file = temp.dir().getChildFile("settings.xml");

        auto engine = createEngine(fileOptions(temp.dir()));
        REQUIRE(engine != nullptr);
        engine->getPropertyStorage().setProperty(te::SettingID::compCrossfadeMs, 42);
        CHECK_FALSE(file.exists());  // no IO per setProperty

        // The collected change is written by the message-thread timer (2 s) without any further call.
        juce::MessageManager::getInstance()->runDispatchLoopUntil(2600);
        CHECK(file.existsAsFile());
    }

    TEST_CASE("an unreadable settings.xml is kept as settings.xml.corrupt-<date>-<time>")
    {
        const tracklab_test::ScopedTempDir temp;
        const juce::String garbage = "<PROPERTIES><VALUE name=\"truncated";
        REQUIRE(temp.dir().getChildFile("settings.xml").replaceWithText(garbage));

        {
            auto engine = createEngine(fileOptions(temp.dir()));
            REQUIRE(engine != nullptr);
            engine->getPropertyStorage().setProperty(te::SettingID::compCrossfadeMs, 42);
            engine->getPropertyStorage().flushSettingsToDisk();
        }

        const auto kept = temp.dir().findChildFiles(juce::File::findFiles, false, "settings.xml.corrupt-*");
        REQUIRE(kept.size() == 1);
        CHECK(kept.getFirst().loadFileAsString() == garbage);
        // YYYYMMDD-HHMMSS follows the prefix.
        CHECK(kept.getFirst().getFileName().fromFirstOccurrenceOf("corrupt-", false, false).length() == 15);
        CHECK(juce::parseXML(temp.dir().getChildFile("settings.xml")) != nullptr);
    }

    TEST_CASE("a readable settings.xml is not renamed")
    {
        const tracklab_test::ScopedTempDir temp;
        {
            auto engine = createEngine(fileOptions(temp.dir()));
            engine->getPropertyStorage().setProperty(te::SettingID::compCrossfadeMs, 42);
        }
        {
            auto engine = createEngine(fileOptions(temp.dir()));
            REQUIRE(engine != nullptr);
        }
        CHECK(temp.dir().findChildFiles(juce::File::findFiles, false, "*.corrupt-*").isEmpty());
    }

    TEST_CASE("the UIBehaviour answers confirmation requests with cancel at once")
    {
        auto engine = createEngine(tracklab_test::testOptions());
        REQUIRE(engine != nullptr);
        auto& ui = engine->getUIBehaviour();

        int okCalls = 0;
        bool okValue = true;
        ui.showOkCancelAlertBoxAsync("Muster", "Beispiel", {}, {},
                                     [&](bool ok)
                                     {
                                         ++okCalls;
                                         okValue = ok;
                                     });
        int yncCalls = 0;
        int yncValue = -1;
        ui.showYesNoCancelAlertBoxAsync("Muster", "Beispiel", {}, {}, {},
                                        [&](int answer)
                                        {
                                            ++yncCalls;
                                            yncValue = answer;
                                        });

        // No message-loop iteration in between: the answer is there when the call returns.
        CHECK(okCalls == 1);
        CHECK_FALSE(okValue);
        CHECK(yncCalls == 1);
        CHECK(yncValue == 0);  // 0 = cancel
    }
}
