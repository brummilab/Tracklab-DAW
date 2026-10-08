// spike_cli command line: argument parsing and JSON output. The operations live in spike_import.cpp,
// spike_render.cpp, spike_record12.cpp and spike_vst3.cpp.
#include "spike_common.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <exception>
#include <map>
#include <ostream>

#include "spike_engine.h"

namespace spike
{

namespace
{

constexpr int kExitOk = 0;
constexpr int kExitFailed = 1;
constexpr int kExitUsage = 2;

juce::String toJuceString(const std::string& utf8)
{
    return juce::String::fromUTF8(utf8.c_str(), static_cast<int>(utf8.size()));
}

juce::String pathText(const std::filesystem::path& path)
{
    const auto utf8 = path.u8string();
    return juce::String::fromUTF8(reinterpret_cast<const char*>(utf8.c_str()), static_cast<int>(utf8.size()));
}

std::filesystem::path pathFromArg(const std::string& arg)
{
    return std::filesystem::path(std::u8string(arg.begin(), arg.end()));
}

/** Writes exactly one JSON object and a newline. */
void printJson(std::ostream& out, juce::DynamicObject::Ptr object)
{
    out << juce::JSON::toString(juce::var(object.get()), true).toStdString() << '\n';
    out.flush();
}

int fail(std::ostream& out, int exitCode, const std::string& message)
{
    juce::DynamicObject::Ptr object = new juce::DynamicObject();
    object->setProperty("ok", false);
    object->setProperty("error", toJuceString(message.empty() ? std::string("unknown error") : message));
    printJson(out, object);
    return exitCode;
}

juce::DynamicObject::Ptr success()
{
    juce::DynamicObject::Ptr object = new juce::DynamicObject();
    object->setProperty("ok", true);
    return object;
}

/** Command line of one command: positional arguments and "--name value" options. */
struct Arguments
{
    std::vector<std::string> positional;
    std::map<std::string, std::string> options;
    std::string error;  // non-empty: usage error

    /** Parses a required or optional number option. Sets `error` if the text is not a finite number. */
    std::optional<double> number(const std::string& name)
    {
        const auto it = options.find(name);
        if (it == options.end())
            return std::nullopt;

        double value = 0.0;
        const auto& text = it->second;
        const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (ec != std::errc() || end != text.data() + text.size() || !std::isfinite(value))
        {
            error = "--" + name + " is not a number: " + text;
            return std::nullopt;
        }
        return value;
    }
};

/** Splits args into positional arguments and options; only the option names in `known` are accepted. */
Arguments parse(const std::vector<std::string>& args, std::initializer_list<const char*> known)
{
    Arguments parsed;
    for (std::size_t i = 1; i < args.size(); ++i)
    {
        const auto& arg = args[i];
        if (arg.rfind("--", 0) != 0)
        {
            parsed.positional.push_back(arg);
            continue;
        }

        const auto name = arg.substr(2);
        if (std::find_if(known.begin(), known.end(), [&](const char* k) { return name == k; }) == known.end())
        {
            parsed.error = "unknown option " + arg;
            return parsed;
        }
        if (i + 1 >= args.size())
        {
            parsed.error = "option " + arg + " needs a value";
            return parsed;
        }
        parsed.options[name] = args[++i];
    }
    return parsed;
}

int runImport(const std::vector<std::string>& args, std::ostream& out)
{
    auto parsed = parse(args, {});
    if (parsed.error.empty() && parsed.positional.size() != 1)
        parsed.error = "usage: import <file>";
    if (!parsed.error.empty())
        return fail(out, kExitUsage, parsed.error);

    const auto r = importFile(pathFromArg(parsed.positional[0]));
    if (!r.ok)
        return fail(out, kExitFailed, r.error);

    auto json = success();
    json->setProperty("sample_rate", r.sampleRate);
    json->setProperty("channels", r.numChannels);
    json->setProperty("length_samples", static_cast<juce::int64>(r.lengthSamples));
    printJson(out, json);
    return kExitOk;
}

int runRender(const std::vector<std::string>& args, std::ostream& out)
{
    auto parsed = parse(args, {"start", "end", "out"});
    const auto start = parsed.number("start");
    const auto end = parsed.number("end");
    if (parsed.error.empty()
        && (parsed.positional.size() != 1 || !start || !end || parsed.options.count("out") == 0))
        parsed.error = "usage: render-region <file> --start S --end E --out O";
    if (!parsed.error.empty())
        return fail(out, kExitUsage, parsed.error);

    const auto r = renderRegion(pathFromArg(parsed.positional[0]), *start, *end, pathFromArg(parsed.options["out"]));
    if (!r.ok)
        return fail(out, kExitFailed, r.error);

    auto json = success();
    json->setProperty("out", pathText(r.outFile));
    json->setProperty("sample_rate", r.sampleRate);
    json->setProperty("channels", r.numChannels);
    json->setProperty("bits_per_sample", r.bitsPerSample);
    json->setProperty("length_samples", static_cast<juce::int64>(r.lengthSamples));
    json->setProperty("integrated_lufs", r.loudness.integratedLufs);
    json->setProperty("true_peak_dbtp", r.loudness.truePeakDbtp);
    json->setProperty("lra", r.loudness.lra);
    printJson(out, json);
    return kExitOk;
}

int runRecord12(const std::vector<std::string>& args, std::ostream& out)
{
    auto parsed = parse(args, {"seconds", "out-dir"});
    const auto seconds = parsed.number("seconds");
    if (parsed.error.empty() && (!parsed.positional.empty() || !seconds))
        parsed.error = "usage: record-12 --seconds S [--out-dir D]";
    if (!parsed.error.empty())
        return fail(out, kExitUsage, parsed.error);

    std::error_code ec;
    const auto outDir = parsed.options.count("out-dir") != 0 ? pathFromArg(parsed.options["out-dir"])
                                                              : std::filesystem::current_path(ec) / "record-12";
    const auto r = record12(*seconds, outDir);
    if (!r.ok)
        return fail(out, kExitFailed, r.error);

    juce::Array<juce::var> files;
    for (const auto& f : r.files)
        files.add(pathText(f));

    auto json = success();
    json->setProperty("inputs", r.numInputs);
    json->setProperty("tracks", r.numTracks);
    json->setProperty("files", files);
    json->setProperty("length_samples", static_cast<juce::int64>(r.lengthSamples));
    json->setProperty("missing_blocks", r.missingBlocks);
    json->setProperty("worst_deviation_db", r.worstDeviationDb);
    json->setProperty("graph_latency_samples", r.graphLatencySamples);
    json->setProperty("clip_start_samples", static_cast<juce::int64>(r.clipStartSamples));
    printJson(out, json);
    return kExitOk;
}

int runLoadVst3(const std::vector<std::string>& args, std::ostream& out)
{
    auto parsed = parse(args, {"gain-db"});
    const auto gain = parsed.number("gain-db");
    if (parsed.error.empty() && parsed.positional.size() != 1)
        parsed.error = "usage: load-vst3 <bundle> [--gain-db G]";
    if (!parsed.error.empty())
        return fail(out, kExitUsage, parsed.error);

    const auto r = loadVst3(pathFromArg(parsed.positional[0]), gain);
    if (!r.ok)
        return fail(out, kExitFailed, r.error);

    auto json = success();
    json->setProperty("plugin", toJuceString(r.pluginName));
    json->setProperty("scanned", r.numScanned);
    json->setProperty("rendered", r.rendered);
    json->setProperty("input_rms_db", r.inputRmsDb);
    json->setProperty("output_rms_db", r.outputRmsDb);
    json->setProperty("gain_db", r.gainDb);
    printJson(out, json);
    return kExitOk;
}

int runDumpPcm(const std::vector<std::string>& args, std::ostream& out)
{
    auto parsed = parse(args, {"out"});
    if (parsed.error.empty() && (parsed.positional.size() != 1 || parsed.options.count("out") == 0))
        parsed.error = "usage: dump-pcm <file> --out <raw>";
    if (!parsed.error.empty())
        return fail(out, kExitUsage, parsed.error);

    const auto r = dumpPcm(pathFromArg(parsed.positional[0]), pathFromArg(parsed.options["out"]));
    if (!r.ok)
        return fail(out, kExitFailed, r.error);

    auto json = success();
    json->setProperty("sample_rate", r.sampleRate);
    json->setProperty("channels", r.numChannels);
    json->setProperty("length_samples", static_cast<juce::int64>(r.lengthSamples));
    printJson(out, json);
    return kExitOk;
}

}  // namespace

int runCli(const std::vector<std::string>& args, std::ostream& out, std::ostream& err)
{
    const std::string usage = "commands: import, render-region, record-12, load-vst3, dump-pcm";
    if (args.empty())
    {
        err << usage << '\n';
        return fail(out, kExitUsage, "no command given; " + usage);
    }

    // The contract is "one JSON object on stdout, never an exception": anything unexpected becomes a failure.
    try
    {
        const auto& command = args[0];
        if (command == "import")
            return runImport(args, out);
        if (command == "render-region")
            return runRender(args, out);
        if (command == "record-12")
            return runRecord12(args, out);
        if (command == "load-vst3")
            return runLoadVst3(args, out);
        if (command == "dump-pcm")
            return runDumpPcm(args, out);

        err << usage << '\n';
        return fail(out, kExitUsage, "unknown command: " + command);
    }
    catch (const std::exception& e)
    {
        return fail(out, kExitFailed, std::string("internal error: ") + e.what());
    }
}

}  // namespace spike
