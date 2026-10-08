// M1-06: io.list_device_types, io.list_devices and io.get_device on fake hardware (tests/io/fake_audio_backend.h).
#include "io_test_session.h"

#include <initializer_list>
#include <string>
#include <vector>

namespace
{

using namespace tracklab_test::io_helpers;
using tracklab_test::ScopedTempDir;
namespace error_code = tracklab::core::error_code;
namespace fake = tracklab_test::fake;

/** The entry of `types` (array of objects with "name") called `name`, or null. */
const Json* findByName(const Json& list, const std::string& name)
{
    for (const auto& entry : list)
        if (entry.at("name") == name)
            return &entry;
    return nullptr;
}

std::vector<std::string> namesOf(const Json& list)
{
    std::vector<std::string> names;
    for (const auto& entry : list)
        names.push_back(entry.at("name").get<std::string>());
    return names;
}

Json strings(std::initializer_list<const char*> values)
{
    Json array = Json::array();
    for (const char* v : values)
        array.push_back(v);
    return array;
}

}  // namespace

TEST_SUITE("io")
{
    //==========================================================================
    // io.list_device_types
    TEST_CASE("io.list_device_types lists the fake types in the order of the device manager")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());

        const auto result = session.ok("io.list_device_types");
        REQUIRE(result.at("types").is_array());
        CHECK(namesOf(result.at("types")) ==
              std::vector<std::string>{fake::kSeparateType, fake::kDuplexType, fake::kJackType});
    }

    TEST_CASE("io.list_device_types reports separate inputs/outputs and the device counts per type")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());

        const auto result = session.ok("io.list_device_types");
        const auto* separate = findByName(result.at("types"), fake::kSeparateType);
        const auto* duplex = findByName(result.at("types"), fake::kDuplexType);
        const auto* jack = findByName(result.at("types"), fake::kJackType);
        REQUIRE(separate != nullptr);
        REQUIRE(duplex != nullptr);
        REQUIRE(jack != nullptr);

        CHECK(separate->at("separate_inputs_and_outputs") == true);
        CHECK(separate->at("input_device_count") == 2);
        CHECK(separate->at("output_device_count") == 2);

        CHECK(duplex->at("separate_inputs_and_outputs") == false);
        CHECK(duplex->at("input_device_count") == 2);
        CHECK(duplex->at("output_device_count") == 2);

        CHECK(jack->at("input_device_count") == 0);
        CHECK(jack->at("output_device_count") == 0);
    }

    TEST_CASE("io.list_device_types has an empty current_type and no hints while no device is open and JACK is fine")
    {
        const ScopedTempDir temp;
        fake::Hardware hardware;
        hardware.jackHasDevice = true;
        const Session session(temp.dir(), hardware);

        const auto result = session.ok("io.list_device_types");
        CHECK(result.at("current_type") == "");
        CHECK(result.at("hints").empty());
    }

    TEST_CASE("io.list_device_types names the type of the open device as current_type")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.setDevice(micAndSpeakersParams());
        CHECK(session.ok("io.list_device_types").at("current_type") == fake::kSeparateType);
    }

    TEST_CASE("io.list_device_types does not touch the hardware: nothing is created or opened")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());
        session.ok("io.list_device_types");
        session.ok("io.get_device");
        CHECK(session.backend->devicesCreated == 0);
        CHECK(session.backend->opens.empty());
    }

    //==========================================================================
    // JACK without devices: the hint (Linux only)
    TEST_CASE("an empty JACK type gives the hint to start Tracklab with pw-jack (Linux)")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());  // JACK type without devices

        const auto result = session.ok("io.list_device_types");
        REQUIRE(result.at("hints").is_array());
#if JUCE_LINUX
        REQUIRE(result.at("hints").size() == 1);
        const auto hint = result.at("hints")[0].get<std::string>();
        CHECK(contains(hint, "pw-jack"));
        CHECK(contains(hint, "JACK"));
        // The type itself is still listed, with no devices.
        const auto* jack = findByName(result.at("types"), fake::kJackType);
        REQUIRE(jack != nullptr);
        CHECK(jack->at("input_device_count") == 0);
#else
        CHECK(result.at("hints").empty());  // the hint is about PipeWire/JACK on Linux only
#endif
    }

    TEST_CASE("a JACK type with devices gives no pw-jack hint")
    {
        const ScopedTempDir temp;
        fake::Hardware hardware;
        hardware.jackHasDevice = true;
        const Session session(temp.dir(), hardware);

        const auto result = session.ok("io.list_device_types");
        for (const auto& hint : result.at("hints"))
            CHECK_FALSE(contains(hint.get<std::string>(), "pw-jack"));
    }

    TEST_CASE("other empty types give no pw-jack hint, only JACK does")
    {
        const ScopedTempDir temp;
        fake::Hardware hardware;
        hardware.jackType = false;  // no JACK type at all, the other two have devices
        const Session session(temp.dir(), hardware);

        const auto result = session.ok("io.list_device_types");
        for (const auto& hint : result.at("hints"))
            CHECK_FALSE(contains(hint.get<std::string>(), "pw-jack"));
    }

    //==========================================================================
    // io.list_devices
    TEST_CASE("io.list_devices lists inputs and outputs with channel names, rates and buffer sizes per type")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());

        const auto result = session.ok("io.list_devices");
        const auto& types = result.at("types");
        CHECK(namesOf(types) == std::vector<std::string>{fake::kSeparateType, fake::kDuplexType, fake::kJackType});

        const auto* separate = findByName(types, fake::kSeparateType);
        REQUIRE(separate != nullptr);
        CHECK(separate->at("separate_inputs_and_outputs") == true);
        CHECK(namesOf(separate->at("inputs")) == std::vector<std::string>{fake::kMicInterface, fake::kLineInterface});
        CHECK(namesOf(separate->at("outputs")) == std::vector<std::string>{fake::kSpeakers, fake::kHeadphones});

        const auto* mic = findByName(separate->at("inputs"), fake::kMicInterface);
        REQUIRE(mic != nullptr);
        CHECK(mic->at("channel_names") == strings({"Mic 1", "Mic 2", "Mic 3", "Mic 4"}));
        CHECK(mic->at("sample_rates") == Json::array({44100, 48000, 96000}));
        CHECK(mic->at("buffer_sizes") == Json::array({64, 128, 256, 512}));
        CHECK(mic->at("default_buffer_size") == 256);

        const auto* line = findByName(separate->at("inputs"), fake::kLineInterface);
        REQUIRE(line != nullptr);
        CHECK(line->at("channel_names") == strings({"Line 1", "Line 2"}));

        const auto* speakers = findByName(separate->at("outputs"), fake::kSpeakers);
        REQUIRE(speakers != nullptr);
        CHECK(speakers->at("channel_names") == strings({"Left", "Right"}));

        const auto* headphones = findByName(separate->at("outputs"), fake::kHeadphones);
        REQUIRE(headphones != nullptr);
        CHECK(headphones->at("channel_names") == strings({"HP L", "HP R"}));
        CHECK(headphones->at("sample_rates") == Json::array({48000}));
        CHECK(headphones->at("buffer_sizes") == Json::array({256}));
        CHECK(headphones->at("default_buffer_size") == 256);
    }

    TEST_CASE("io.list_devices shows a combined device in inputs (input channels) and outputs (output channels)")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());

        const auto result = session.ok("io.list_devices", Json{{"type", fake::kDuplexType}});
        REQUIRE(result.at("types").size() == 1);
        const auto& duplex = result.at("types")[0];
        CHECK(duplex.at("name") == fake::kDuplexType);
        CHECK(duplex.at("separate_inputs_and_outputs") == false);
        CHECK(namesOf(duplex.at("inputs")) == std::vector<std::string>{fake::kDuplexInterface, fake::kSecondBox});
        CHECK(namesOf(duplex.at("outputs")) == std::vector<std::string>{fake::kDuplexInterface, fake::kSecondBox});

        const auto* in = findByName(duplex.at("inputs"), fake::kDuplexInterface);
        const auto* out = findByName(duplex.at("outputs"), fake::kDuplexInterface);
        REQUIRE(in != nullptr);
        REQUIRE(out != nullptr);
        CHECK(in->at("channel_names") == strings({"In 1", "In 2", "In 3", "In 4", "In 5", "In 6", "In 7", "In 8"}));
        CHECK(out->at("channel_names") ==
              strings({"Out 1", "Out 2", "Out 3", "Out 4", "Out 5", "Out 6", "Out 7", "Out 8"}));
        CHECK(in->at("sample_rates") == Json::array({44100, 48000}));
        CHECK(in->at("buffer_sizes") == Json::array({32, 64, 128, 256}));
        CHECK(in->at("default_buffer_size") == 128);

        const auto* box = findByName(duplex.at("outputs"), fake::kSecondBox);
        REQUIRE(box != nullptr);
        CHECK(box->at("channel_names") == strings({"Box Out 1", "Box Out 2"}));
    }

    TEST_CASE("io.list_devices of an empty JACK type has no inputs and no outputs")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());

        const auto result = session.ok("io.list_devices", Json{{"type", fake::kJackType}});
        REQUIRE(result.at("types").size() == 1);
        CHECK(result.at("types")[0].at("inputs").empty());
        CHECK(result.at("types")[0].at("outputs").empty());
    }

    TEST_CASE("io.list_devices with an unknown type fails with handler_failed that names the type")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());

        const auto outcome = session.run("io.list_devices", Json{{"type", "Muster Driver"}});
        CHECK_FALSE(outcome.ok);
        CHECK(outcome.error.code == error_code::handlerFailed);
        CHECK(contains(outcome.error.message, "Muster Driver"));
    }

    TEST_CASE("listing devices opens nothing")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());

        session.ok("io.list_devices");
        CHECK(session.backend->opens.empty());
        CHECK(session.getDevice().at("open") == false);
    }

    //==========================================================================
    // io.get_device
    TEST_CASE("io.get_device without an open device reports open=false and empty values")
    {
        const ScopedTempDir temp;
        const Session session(temp.dir());

        const auto setup = session.getDevice();
        CHECK(setup.at("open") == false);
        CHECK(setup.at("input_device") == "");
        CHECK(setup.at("output_device") == "");
        CHECK(setup.at("sample_rate") == 0);
        CHECK(setup.at("buffer_size") == 0);
        CHECK(setup.at("active_input_channels").empty());
        CHECK(setup.at("active_output_channels").empty());
    }
}
