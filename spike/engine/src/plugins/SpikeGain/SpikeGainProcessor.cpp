// SpikeGain: the engine spike's own VST3 test plugin (no third-party binary in the repo, R13).
//
// One parameter "gainDb" (-24..+12 dB, default kSpikeGainDefaultDb = -6 dB), no editor, no smoothing: the gain
// applies from the first sample of the first block, so load-vst3 can check the level change of a single block
// to +-0.01 dB. The parameter is also named "gainDb" because a VST3 host only sees the name and a numeric ID.
#include <juce_audio_processors/juce_audio_processors.h>

#include "spike_common.h"
#include "spike_realtime.h"

namespace
{

constexpr float kMinGainDb = -24.0f;
constexpr float kMaxGainDb = 12.0f;

class SpikeGainProcessor final : public juce::AudioProcessor
{
public:
    SpikeGainProcessor()
        : AudioProcessor(BusesProperties()
                             .withInput("Input", juce::AudioChannelSet::stereo(), true)
                             .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
        auto param = std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"gainDb", 1}, "gainDb", juce::NormalisableRange<float>(kMinGainDb, kMaxGainDb),
            static_cast<float>(spike::kSpikeGainDefaultDb), juce::AudioParameterFloatAttributes().withLabel("dB"));
        gainDb = param.get();
        addParameter(param.release());
    }

    const juce::String getName() const override { return "SpikeGain"; }

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        const auto out = layouts.getMainOutputChannelSet();
        return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) &&
               layouts.getMainInputChannelSet() == out;
    }

    void prepareToPlay(double, int) override {}
    void releaseResources() override {}

    // Audio thread: reads one atomic parameter value and scales the buffer in place. No allocation, no locks.
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) noexcept SPIKE_NONBLOCKING override
    {
        const float gain = std::pow(10.0f, gainDb->get() / 20.0f);
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        float* const* channels = buffer.getArrayOfWritePointers();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = channels[ch];
            for (int i = 0; i < numSamples; ++i)
                data[i] *= gain;
        }
    }

    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    // The state is just the gain value, so that a host restoring a session gets the same level back.
    void getStateInformation(juce::MemoryBlock& destData) override
    {
        juce::MemoryOutputStream(destData, false).writeFloat(gainDb->get());
    }

    void setStateInformation(const void* data, int sizeInBytes) override
    {
        if (sizeInBytes >= static_cast<int>(sizeof(float)))
        {
            juce::MemoryInputStream in(data, static_cast<size_t>(sizeInBytes), false);
            *gainDb = juce::jlimit(kMinGainDb, kMaxGainDb, in.readFloat());
        }
    }

private:
    juce::AudioParameterFloat* gainDb = nullptr;  // owned by the AudioProcessor

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpikeGainProcessor)
};

}  // namespace

// Entry point of the JUCE plugin wrapper (declared by juce_audio_processors).
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpikeGainProcessor();
}
