#include "test_support.h"

#include <cstdlib>
#include <iostream>
#include <sstream>

namespace spike_test
{

namespace
{
std::filesystem::path g_cliExecutable;
std::filesystem::path g_spikeGainBundle;
int g_skipped = 0;

juce::var parseJson(const std::string& text)
{
    juce::var result;
    if (juce::JSON::parse(juce::String(text), result).failed())
        return {};
    return result;
}
}  // namespace

void setPathsFromArgs(int argc, char** argv)
{
    const std::string cliPrefix = "--spike-cli=";
    const std::string gainPrefix = "--spike-gain-vst3=";
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg.rfind(cliPrefix, 0) == 0)
            g_cliExecutable = arg.substr(cliPrefix.size());
        else if (arg.rfind(gainPrefix, 0) == 0)
            g_spikeGainBundle = arg.substr(gainPrefix.size());
    }
    if (g_spikeGainBundle.empty())
        if (const char* env = std::getenv("SPIKE_GAIN_VST3"))
            g_spikeGainBundle = env;
}

std::filesystem::path cliExecutable()
{
    return g_cliExecutable;
}

std::filesystem::path spikeGainBundle()
{
    return g_spikeGainBundle;
}

void skipTest(const std::string& reason)
{
    const char* require = std::getenv("SPIKE_REQUIRE_TOOLS");
    if (require != nullptr && std::string(require) == "1")
        FAIL("required tool missing (SPIKE_REQUIRE_TOOLS=1): " << reason);

    ++g_skipped;
    std::cerr << "[  SKIPPED ] " << reason << std::endl;
}

int skippedCount()
{
    return g_skipped;
}

CliRun runCli(const std::vector<std::string>& args)
{
    std::ostringstream out;
    std::ostringstream err;
    CliRun run;
    run.exitCode = spike::runCli(args, out, err);
    run.out = out.str();
    run.err = err.str();
    run.json = parseJson(run.out);
    return run;
}

CliRun runCliProcess(const std::vector<std::string>& args)
{
    CliRun run;
    juce::StringArray command;
    command.add(juce::String(cliExecutable().string()));
    for (const auto& a : args)
        command.add(juce::String(a));

    juce::ChildProcess process;
    if (!process.start(command, juce::ChildProcess::wantStdOut))
        return run;

    run.out = process.readAllProcessOutput().toStdString();
    process.waitForProcessToFinish(120000);
    run.exitCode = static_cast<int>(process.getExitCode());
    run.json = parseJson(run.out);
    return run;
}

}  // namespace spike_test
