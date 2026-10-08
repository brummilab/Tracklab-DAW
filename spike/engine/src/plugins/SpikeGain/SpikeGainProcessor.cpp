// STUB (test-writer): a pass-through VST3 effect without parameters so that the plugin target builds and
// can be scanned and loaded. The implementer replaces it with the real SpikeGain: one parameter "gainDb"
// (-24..+12 dB, default kSpikeGainDefaultDb = -6 dB, no smoothing), no editor.
#include <juce_audio_processors/juce_audio_processors.h>

namespace
{

class SpikeGainStub final : public juce::AudioProcessor
{
public:
    SpikeGainStub()
        : AudioProcessor(BusesProperties()
                             .withInput("Input", juce::AudioChannelSet::stereo(), true)
                             .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    using juce::AudioProcessor::processBlock;
    const juce::String getName() const override { return "SpikeGain"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}  // pass-through
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
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};

}  // namespace

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpikeGainStub();
}
