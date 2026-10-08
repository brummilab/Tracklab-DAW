// Shared helpers of the spike tests: doctest include, paths passed in by CTest, skip handling, CLI/JSON helpers.
#pragma once

#include <juce_core/juce_core.h>

#include <doctest.h>

#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include "fixtures-gen/Fixtures.h"
#include "spike_common.h"

namespace spike_test
{

/** Reads --spike-cli=... and --spike-gain-vst3=... (given by CTest) from the command line. */
void setPathsFromArgs(int argc, char** argv);

/** Path of the spike_cli executable (--spike-cli=...). Empty if not given. */
std::filesystem::path cliExecutable();

/** Path of the built SpikeGain.vst3 bundle (--spike-gain-vst3=... or env SPIKE_GAIN_VST3). */
std::filesystem::path spikeGainBundle();

/** Registers a skipped test: prints the reason, makes the process exit with code 77 (CTest "skipped",
    never "passed") unless something failed. If env SPIKE_REQUIRE_TOOLS=1 (CI), a missing tool is a failure. */
void skipTest(const std::string& reason);

/** Number of skipTest() calls so far. */
int skippedCount();

/** Parsed command line result: exit code, raw stdout text and the parsed JSON object. */
struct CliRun
{
    int exitCode = -1;
    std::string out;
    std::string err;
    juce::var json;  // parsed `out`; void if `out` is not one valid JSON value

    bool ok() const { return json.getProperty("ok", false); }
    juce::var get(const char* key) const { return json.getProperty(key, juce::var()); }
    bool has(const char* key) const { return json.hasProperty(key); }
    double number(const char* key) const { return static_cast<double>(get(key)); }
    std::int64_t integer(const char* key) const
    {
        return static_cast<std::int64_t>(static_cast<juce::int64>(get(key)));
    }
    std::string text(const char* key) const { return get(key).toString().toStdString(); }
};

/** Runs spike::runCli in-process. */
CliRun runCli(const std::vector<std::string>& args);

/** Runs the real spike_cli executable as a child process. */
CliRun runCliProcess(const std::vector<std::string>& args);

}  // namespace spike_test

/** A failing operation must say why, and must not be the stub's placeholder. */
#define SPIKE_REQUIRE_IMPLEMENTED(result)                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        INFO("error: " << (result).error);                                                                             \
        REQUIRE_MESSAGE((result).error != ::spike::kNotImplemented, "operation is not implemented");                   \
    } while (false)

/** Skips the current test case (reports it, returns from the enclosing function) if `condition` is false. */
#define SPIKE_SKIP_UNLESS(condition, reason)                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            ::spike_test::skipTest(reason);                                                                            \
            return;                                                                                                    \
        }                                                                                                              \
    } while (false)

/** |actual - expected| <= tolerance, with the three numbers in the failure message. */
#define SPIKE_CHECK_NEAR(actual, expected, tolerance)                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        const double spikeActual_ = (actual);                                                                          \
        INFO(#actual " = " << spikeActual_ << ", expected " << (expected) << " +- " << (tolerance));                   \
        CHECK(std::abs(spikeActual_ - (expected)) <= (tolerance));                                                     \
    } while (false)
