// tracklab-cli io (O-14, contract in cli.h): one io.* command of the registry, without a project, the settings in
// --settings-dir. The hardware is the fake device types of tests/io (CliHooks::addAudioDeviceTypes), so nothing here
// touches a sound card; the few process tests only use what works on a machine without audio devices.
#include "cli/cli_test_support.h"

#include "io/io_test_session.h"

#include <cstdlib>
#include <memory>

using namespace tracklab_test::cli;
namespace fake = tracklab_test::fake;
namespace io_helpers = tracklab_test::io_helpers;

namespace
{

/** One `tracklab-cli io ...` call in-process, with the fake hardware. `backend` is what the fake hardware saw. */
struct IoRun
{
    CliRun run;
    std::shared_ptr<fake::Backend> backend;
};

IoRun runIo(const std::vector<std::string>& args, const juce::File& settingsDir, const fake::Hardware& hardware = {})
{
    IoRun result;
    result.backend = std::make_shared<fake::Backend>();
    tracklab::cli::CliHooks hooks;
    hooks.addAudioDeviceTypes = [backend = result.backend, hardware](juce::AudioDeviceManager& manager)
    { fake::addFakeTypes(manager, backend, hardware); };

    std::vector<std::string> full{"io"};
    full.insert(full.end(), args.begin(), args.end());
    full.insert(full.end(), {"--settings-dir", path(settingsDir)});
    result.run = runCli(full, hooks);
    return result;
}

/** `io <id> [<params>]`; the params are one argument of JSON text. */
IoRun runIoCommand(const std::string& id, const Json& params, const juce::File& settingsDir,
                   const fake::Hardware& hardware = {})
{
    return runIo({id, params.dump()}, settingsDir, hardware);
}

/** The result of the command inside the CLI's success object (null if the call failed). */
Json resultOf(const CliRun& run)
{
    return run.json.is_object() && run.json.contains("result") ? run.json["result"] : Json();
}

/** The usual hand-test setup call. */
Json setMicAndSpeakers(const juce::File& settingsDir)
{
    const auto set = runIoCommand("io.set_device", io_helpers::micAndSpeakersParams(), settingsDir);
    REQUIRE_MESSAGE(set.run.ok(), set.run.out);
    return resultOf(set.run);
}

}  // namespace

TEST_SUITE("cli")
{
    //==========================================================================
    // Success: the result of the command, one JSON line

    TEST_CASE("io: io.get_device without a stored setup reports no open device (exit 0, one JSON line)")
    {
        ScopedTempDir settings;

        const auto [run, backend] = runIo({"io.get_device"}, settings.dir());

        CHECK(run.exitCode == 0);
        CHECK(isSingleJsonLine(run));
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(resultOf(run) == io_helpers::noDeviceSetup());
        CHECK(backend->opens.empty());
    }

    TEST_CASE("io: explicit empty params {} are the same as no params")
    {
        ScopedTempDir settings;

        const auto run = runIoCommand("io.get_device", Json::object(), settings.dir()).run;

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(resultOf(run) == io_helpers::noDeviceSetup());
    }

    TEST_CASE("io: io.list_device_types returns exactly what the registry command returns (no second code path)")
    {
        ScopedTempDir settings;
        ScopedTempDir referenceSettings;
        const io_helpers::Session reference(referenceSettings.dir());

        const auto run = runIo({"io.list_device_types"}, settings.dir()).run;

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(resultOf(run) == reference.ok("io.list_device_types"));
        CHECK(resultOf(run).at("types").size() == 3);  // Fake Separate, Fake Duplex, JACK
    }

    TEST_CASE("io: io.list_devices passes the params on and returns what the registry command returns")
    {
        ScopedTempDir settings;
        ScopedTempDir referenceSettings;
        const io_helpers::Session reference(referenceSettings.dir());

        const auto all = runIoCommand("io.list_devices", Json::object(), settings.dir()).run;
        const auto one = runIoCommand("io.list_devices", Json{{"type", fake::kDuplexType}}, settings.dir()).run;

        REQUIRE_MESSAGE(all.ok(), all.out);
        REQUIRE_MESSAGE(one.ok(), one.out);
        CHECK(resultOf(all) == reference.ok("io.list_devices"));
        CHECK(resultOf(one) == reference.ok("io.list_devices", Json{{"type", fake::kDuplexType}}));
        CHECK(resultOf(one).at("types").size() == 1);
    }

    TEST_CASE("io: io.set_device (not undoable, so run-commands refuses it) works and returns the new setup")
    {
        ScopedTempDir settings;

        const auto [run, backend] = runIoCommand("io.set_device", io_helpers::micAndSpeakersParams(), settings.dir());

        CHECK(run.exitCode == 0);
        CHECK(isSingleJsonLine(run));
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(resultOf(run) == io_helpers::micAndSpeakersSetup());
        REQUIRE_FALSE(backend->opens.empty());  // the (fake) hardware was really asked to open the device
        CHECK(backend->opens.back().inputDevice == fake::kMicInterface);
        CHECK(backend->opens.back().outputDevice == fake::kSpeakers);
        CHECK(backend->opens.back().bufferSize == 128);
    }

    //==========================================================================
    // Settings folder

    TEST_CASE("io: the chosen device survives two calls (two program starts) on the same --settings-dir")
    {
        ScopedTempDir settings;
        setMicAndSpeakers(settings.dir());

        const auto [second, backend] = runIo({"io.get_device"}, settings.dir());

        CHECK(second.exitCode == 0);
        REQUIRE_MESSAGE(second.ok(), second.out);
        CHECK(resultOf(second) == io_helpers::micAndSpeakersSetup());
        // The second call opened the stored device again, as stored.
        REQUIRE_FALSE(backend->opens.empty());
        CHECK(backend->opens.back().inputDevice == fake::kMicInterface);
        CHECK(backend->opens.back().outputDevice == fake::kSpeakers);
        CHECK(backend->opens.back().sampleRate == doctest::Approx(48000.0));
        CHECK(backend->opens.back().bufferSize == 128);
    }

    TEST_CASE("io: a change of the setup in the second call is kept for the third")
    {
        ScopedTempDir settings;
        setMicAndSpeakers(settings.dir());
        const auto change = runIoCommand("io.set_device", Json{{"buffer_size", 256}}, settings.dir()).run;
        REQUIRE_MESSAGE(change.ok(), change.out);

        const auto third = runIo({"io.get_device"}, settings.dir()).run;

        REQUIRE_MESSAGE(third.ok(), third.out);
        CHECK(resultOf(third).at("buffer_size") == 256);
        CHECK(resultOf(third).at("input_device") == fake::kMicInterface);
    }

    TEST_CASE("io: --settings-dir receives settings.xml (the folder is created if missing)")
    {
        ScopedTempDir temp;
        const auto settings = temp.dir().getChildFile("handtest/settings");
        REQUIRE_FALSE(settings.exists());

        setMicAndSpeakers(settings);

        const auto file = settings.getChildFile("settings.xml");
        REQUIRE(file.existsAsFile());  // written before runCli returned: the engine is gone
        CHECK(file.loadFileAsString().contains(fake::kMicInterface));
    }

    TEST_CASE("io: different --settings-dir folders are independent")
    {
        ScopedTempDir first;
        ScopedTempDir second;
        setMicAndSpeakers(first.dir());

        const auto other = runIo({"io.get_device"}, second.dir()).run;

        REQUIRE_MESSAGE(other.ok(), other.out);
        CHECK(resultOf(other) == io_helpers::noDeviceSetup());
    }

    //==========================================================================
    // Failures of the command and of the parameters: exit 1, code and pointer of the registry

    TEST_CASE("io: a command that fails exits with 1, the message names the value, the stored setup stays")
    {
        ScopedTempDir settings;
        setMicAndSpeakers(settings.dir());

        const auto run = runIoCommand("io.set_device", Json{{"sample_rate", 22050}}, settings.dir()).run;

        CHECK(run.exitCode == 1);
        CHECK(isSingleJsonLine(run));
        CHECK_FALSE(run.ok());
        CHECK(run.errorCode() == "handler_failed");
        CHECK(io_helpers::contains(run.errorMessage(), "22050"));

        const auto after = runIo({"io.get_device"}, settings.dir()).run;
        REQUIRE_MESSAGE(after.ok(), after.out);
        CHECK(resultOf(after) == io_helpers::micAndSpeakersSetup());
    }

    TEST_CASE("io: a device that does not exist is refused with its name (exit 1)")
    {
        ScopedTempDir settings;

        const auto run =
            runIoCommand("io.set_device", Json{{"output_device", "Muster-Geraet gibt es nicht"}}, settings.dir()).run;

        CHECK(run.exitCode == 1);
        CHECK(run.errorCode() == "handler_failed");
        CHECK(io_helpers::contains(run.errorMessage(), "Muster-Geraet gibt es nicht"));
    }

    TEST_CASE("io: parameters of the wrong type are refused with invalid_params and the pointer of the field")
    {
        ScopedTempDir settings;

        const auto run = runIo({"io.set_device", R"({"sample_rate":"fast"})"}, settings.dir()).run;

        CHECK(run.exitCode == 1);
        CHECK(isSingleJsonLine(run));
        CHECK(run.errorCode() == "invalid_params");
        REQUIRE(run.json["error"].contains("pointer"));
        CHECK(run.json["error"]["pointer"] == "/sample_rate");
    }

    TEST_CASE("io: a parameter the schema does not know is refused with invalid_params and a pointer")
    {
        ScopedTempDir settings;

        const auto run = runIoCommand("io.set_device", Json{{"volume", 11}}, settings.dir()).run;

        CHECK(run.exitCode == 1);
        CHECK(run.errorCode() == "invalid_params");
        CHECK(run.json["error"].contains("pointer"));
    }

    TEST_CASE("io: params for a command without params are refused with invalid_params")
    {
        ScopedTempDir settings;

        const auto run = runIoCommand("io.get_device", Json{{"type", "x"}}, settings.dir()).run;

        CHECK(run.exitCode == 1);
        CHECK(run.errorCode() == "invalid_params");
        CHECK(run.json["error"].contains("pointer"));
    }

    TEST_CASE("io: params that are not valid JSON (or not an object) fail and nothing is executed")
    {
        ScopedTempDir settings;
        setMicAndSpeakers(settings.dir());

        for (const char* text : {R"({"buffer_size":)", "{not json", "[1]", "42", "\"text\""})
        {
            CAPTURE(text);
            const auto run = runIo({"io.set_device", text}, settings.dir()).run;

            // Exit 1 (a failed operation) or 2 (usage): the lead decides, see the report of the test writer.
            CHECK((run.exitCode == 1 || run.exitCode == 2));
            CHECK(isSingleJsonLine(run));
            CHECK_FALSE(run.ok());
            CHECK_FALSE(run.errorCode().empty());
        }
        const auto after = runIo({"io.get_device"}, settings.dir()).run;
        REQUIRE_MESSAGE(after.ok(), after.out);
        CHECK(resultOf(after) == io_helpers::micAndSpeakersSetup());
    }

    TEST_CASE("io: an io.* id the registry does not know exits with 1 and unknown_command")
    {
        ScopedTempDir settings;

        const auto run = runIo({"io.no_such_command"}, settings.dir()).run;

        CHECK(run.exitCode == 1);
        CHECK(isSingleJsonLine(run));
        CHECK(run.errorCode() == "unknown_command");
    }

    //==========================================================================
    // Only io.* commands

    TEST_CASE("io: a command that is not an io.* command is a usage error (exit 2) and is not executed")
    {
        ScopedTempDir settings;

        const auto version = runIo({"app.version"}, settings.dir()).run;
        CHECK(version.exitCode == 2);
        CHECK(isSingleJsonLine(version));
        CHECK_FALSE(version.ok());
        CHECK(version.errorCode() == "usage");
        CHECK_FALSE(version.has("result"));  // app.version exists, but did not run

        ScopedTempDir projects;
        const auto created =
            runIoCommand("project.new", Json{{"folder", path(projects.dir())}, {"name", "Muster"}}, settings.dir()).run;
        CHECK(created.exitCode == 2);
        CHECK(created.errorCode() == "usage");
        CHECK_FALSE(projects.dir().getChildFile("Muster").exists());  // project.new did not run
    }

    TEST_CASE("io: the prefix is \"io.\" exactly: lookalikes are usage errors")
    {
        ScopedTempDir settings;

        for (const char* id : {"io", "iox.get_device", "get_device", "IO.get_device", "edit.io.get_device"})
        {
            CAPTURE(id);
            const auto run = runIo({id}, settings.dir()).run;

            CHECK(run.exitCode == 2);
            CHECK(isSingleJsonLine(run));
            CHECK(run.errorCode() == "usage");
        }
    }

    //==========================================================================
    // Usage errors (exit 2): nothing is started

    TEST_CASE("io: without a command id it is a usage error")
    {
        ScopedTempDir settings;

        const auto run = runIo({}, settings.dir()).run;

        CHECK(run.exitCode == 2);
        CHECK(isSingleJsonLine(run));
        CHECK(run.errorCode() == "usage");
        CHECK_FALSE(run.errorMessage().empty());
    }

    TEST_CASE("io: more than one params argument is a usage error")
    {
        ScopedTempDir settings;

        const auto run = runIo({"io.get_device", "{}", "{}"}, settings.dir()).run;

        CHECK(run.exitCode == 2);
        CHECK(run.errorCode() == "usage");
    }

    TEST_CASE("io: an unknown option is a usage error")
    {
        ScopedTempDir settings;

        const auto run = runIo({"io.get_device", "--no-such-option"}, settings.dir()).run;

        CHECK(run.exitCode == 2);
        CHECK(isSingleJsonLine(run));
        CHECK(run.errorCode() == "usage");
    }

    TEST_CASE("io: --settings-dir without a value is a usage error")
    {
        const auto run = runCli({"io", "io.get_device", "--settings-dir"});

        CHECK(run.exitCode == 2);
        CHECK(isSingleJsonLine(run));
        CHECK(run.errorCode() == "usage");
    }

    //==========================================================================
    // The real executable: only what works without audio hardware

    TEST_CASE("process: io.list_device_types prints one JSON line and exits with 0 on a machine without a sound card")
    {
        ScopedTempDir settings;

        const auto run = runCliProcess({"io", "io.list_device_types", "--settings-dir", path(settings.dir())});

        CHECK(run.exitCode == 0);
        CHECK(isSingleJsonLine(run));
        REQUIRE_MESSAGE(run.ok(), run.out);
        REQUIRE(resultOf(run).is_object());
        CHECK(resultOf(run).contains("types"));
        CHECK(resultOf(run).contains("current_type"));
    }

    TEST_CASE("process: io.get_device answers with a setup (same shape as in the registry)")
    {
        ScopedTempDir settings;

        const auto run = runCliProcess({"io", "io.get_device", "--settings-dir", path(settings.dir())});

        CHECK(run.exitCode == 0);
        REQUIRE_MESSAGE(run.ok(), run.out);
        for (const char* key : {"open", "type", "input_device", "output_device", "sample_rate", "buffer_size",
                                "active_input_channels", "active_output_channels"})
        {
            CAPTURE(key);
            CHECK(resultOf(run).contains(key));
        }
    }

    TEST_CASE("process: the settings of an io call are written to --settings-dir")
    {
        ScopedTempDir temp;
        const auto settings = temp.dir().getChildFile("settings");

        const auto run = runCliProcess({"io", "io.list_devices", "--settings-dir", path(settings)});

        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(settings.getChildFile("settings.xml").existsAsFile());
    }

    TEST_CASE("process: a command that is not an io.* command exits with status 2 and is not executed")
    {
        ScopedTempDir settings;

        const auto run = runCliProcess({"io", "app.version", "--settings-dir", path(settings.dir())});

        CHECK(run.exitCode == 2);
        CHECK(isSingleJsonLine(run));
        CHECK(run.errorCode() == "usage");
        CHECK_FALSE(run.has("result"));
    }

    TEST_CASE("process: an io.* command that fails exits with status 1 and prints one JSON error line")
    {
        ScopedTempDir settings;

        const auto run = runCliProcess(
            {"io", "io.set_device", R"({"type":"Muster-Treiber"})", "--settings-dir", path(settings.dir())});

        CHECK(run.exitCode == 1);
        CHECK(isSingleJsonLine(run));
        CHECK(run.errorCode() == "handler_failed");
        CHECK(io_helpers::contains(run.errorMessage(), "Muster-Treiber"));
    }

#if JUCE_LINUX
    TEST_CASE("process: with --settings-dir nothing is written to the user's settings folder")
    {
        // The settings folder of the user is ~/.config/Tracklab: the child process gets a HOME of its own.
        ScopedTempDir home;
        ScopedTempDir settings;
        const char* previous = std::getenv("HOME");
        const std::string previousHome = previous != nullptr ? previous : "";
        REQUIRE(setenv("HOME", path(home.dir()).c_str(), 1) == 0);

        const auto run = runCliProcess({"io", "io.get_device", "--settings-dir", path(settings.dir())});

        if (previous != nullptr)
            setenv("HOME", previousHome.c_str(), 1);
        else
            unsetenv("HOME");
        REQUIRE_MESSAGE(run.ok(), run.out);
        CHECK(settings.dir().getChildFile("settings.xml").existsAsFile());
        CHECK_FALSE(home.dir().getChildFile(".config/Tracklab").exists());
    }
#endif
}
