// load-vst3: scan a VST3 bundle with the engine's plugin format manager, load the first plugin and render one block.
//
// The plugin is processed directly (one AudioPluginInstance::processBlock call) instead of through an Edit: the
// check is "the engine can scan and host VST3", and a single block of a known sine gives an exact level change.
#include <cmath>

#include "spike_common.h"
#include "spike_engine.h"

namespace spike
{

namespace
{

namespace te = tracktion;
using detail::SpikeEngine;
using detail::toJuceFile;
using detail::toStd;

constexpr double kPi = 3.14159265358979323846;
constexpr double kGainMinDb = -24.0;
constexpr double kGainMaxDb = 12.0;

double rmsDb(const juce::AudioBuffer<float>& buffer, int channel, int numSamples)
{
    double sum = 0.0;
    const float* data = buffer.getReadPointer(channel);
    for (int i = 0; i < numSamples; ++i)
        sum += static_cast<double>(data[i]) * static_cast<double>(data[i]);
    const double rms = std::sqrt(sum / static_cast<double>(numSamples));
    return 20.0 * std::log10(std::max(rms, 1.0e-10));
}

juce::AudioProcessorParameter* findParameter(juce::AudioPluginInstance& instance, const juce::String& name)
{
    // A VST3 host only sees the parameter's name and a numeric ID, so SpikeGain names its parameter "gainDb".
    for (auto* param : instance.getParameters())
        if (param->getName(64) == name)
            return param;
    return nullptr;
}

}  // namespace

Vst3Result loadVst3(const std::filesystem::path& bundle, std::optional<double> gainDb)
{
    Vst3Result result;

    if (gainDb.has_value() && !(*gainDb >= kGainMinDb && *gainDb <= kGainMaxDb))
    {
        result.error = "--gain-db must be within -24..+12 dB";
        return result;
    }

    const auto bundleFile = toJuceFile(bundle);
    if (!bundleFile.exists())
    {
        result.error = "VST3 bundle does not exist: " + bundle.string();
        return result;
    }

    SpikeEngine engine;
    auto& formats = engine.get().getPluginManager().pluginFormatManager;

    juce::AudioPluginFormat* vst3 = nullptr;
    for (auto* format : formats.getFormats())
        if (format->getName() == "VST3")
            vst3 = format;
    if (vst3 == nullptr)
    {
        result.error = "the engine has no VST3 format (JUCE_PLUGINHOST_VST3 not set?)";
        return result;
    }

    juce::OwnedArray<juce::PluginDescription> found;
    if (vst3->fileMightContainThisPluginType(bundleFile.getFullPathName()))
        vst3->findAllTypesForFile(found, bundleFile.getFullPathName());
    result.numScanned = found.size();
    if (found.isEmpty())
    {
        result.error = "no VST3 plugin found in " + bundle.string();
        return result;
    }
    result.pluginName = toStd(found[0]->name);

    juce::String error;
    auto instance = formats.createPluginInstance(*found[0], kVst3TestSampleRate, kVst3TestBlockFrames, error);
    if (instance == nullptr)
    {
        result.error = "cannot load " + result.pluginName + ": " + toStd(error);
        return result;
    }

    if (gainDb.has_value())
    {
        auto* param = findParameter(*instance, "gainDb");
        if (param == nullptr)
        {
            result.error = "the plugin has no parameter gainDb";
            return result;
        }
        param->setValue(param->getValueForText(juce::String(*gainDb, 6)));
    }

    instance->enableAllBuses();
    instance->prepareToPlay(kVst3TestSampleRate, kVst3TestBlockFrames);

    const int numChannels = std::max(instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels());
    if (numChannels < 1 || instance->getTotalNumOutputChannels() < 1)
    {
        result.error = "the plugin has no audio output";
        return result;
    }

    // One block of the documented sine on every input channel.
    juce::AudioBuffer<float> buffer(numChannels, kVst3TestBlockFrames);
    buffer.clear();
    const double w = 2.0 * kPi * kVst3TestFrequencyHz / kVst3TestSampleRate;
    for (int ch = 0; ch < instance->getTotalNumInputChannels(); ++ch)
        for (int n = 0; n < kVst3TestBlockFrames; ++n)
            buffer.setSample(ch, n, static_cast<float>(kVst3TestAmplitude * std::sin(w * static_cast<double>(n))));
    result.inputRmsDb = rmsDb(buffer, 0, kVst3TestBlockFrames);

    juce::MidiBuffer midi;
    instance->processBlock(buffer, midi);
    instance->releaseResources();

    result.rendered = true;
    result.outputRmsDb = rmsDb(buffer, 0, kVst3TestBlockFrames);
    result.gainDb = result.outputRmsDb - result.inputRmsDb;
    result.ok = true;
    result.error.clear();
    return result;
}

}  // namespace spike
