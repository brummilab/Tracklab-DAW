// Fake audio backend for the io tests (M1-06): a juce::AudioIODeviceType with fake devices, so that choosing, opening
// and restoring audio devices can be tested on a machine without audio hardware (headless CI). Nothing here opens a
// sound card, a socket or a file.
//
// The fake hardware (all names are made up):
//   type "Fake Separate"  (separate inputs and outputs, like JACK or WASAPI)
//     inputs   "Fake Mic Interface"   4 channels "Mic 1".."Mic 4"
//              "Fake Line Interface"  2 channels "Line 1", "Line 2"
//     outputs  "Fake Monitor Speakers" 2 channels "Left", "Right"
//              "Fake Headphones"       2 channels "HP L", "HP R"; 48000 Hz and 256 frames only
//     rates 44100/48000/96000, buffer sizes 64/128/256/512 (default 256)
//   type "Fake Duplex"    (one device for both directions, like ASIO)
//     "Fake Duplex Interface" 8 in "In 1".."In 8", 8 out "Out 1".."Out 8"; rates 44100/48000,
//                             buffer sizes 32/64/128/256 (default 128)
//     "Fake Second Box"       2 in "Box In 1".."Box In 2", 2 out "Box Out 1".."Box Out 2"; 48000 Hz, 128 frames only
//   type "JACK"           (separate, no devices: JACK without a server, or without pw-jack)
//
// A combined device (separate type, input and output given) offers the intersection of the rates and buffer sizes of
// its two halves, like a real driver that has to run both at once.
#pragma once

#include "test_support.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <memory>
#include <utility>
#include <vector>

namespace tracklab_test::fake
{

inline const char* const kSeparateType = "Fake Separate";
inline const char* const kMicInterface = "Fake Mic Interface";
inline const char* const kLineInterface = "Fake Line Interface";
inline const char* const kSpeakers = "Fake Monitor Speakers";
inline const char* const kHeadphones = "Fake Headphones";

inline const char* const kDuplexType = "Fake Duplex";
inline const char* const kDuplexInterface = "Fake Duplex Interface";
inline const char* const kSecondBox = "Fake Second Box";

inline const char* const kJackType = "JACK";

struct DeviceSpec
{
    juce::String name;
    juce::StringArray inputChannelNames;
    juce::StringArray outputChannelNames;
    juce::Array<double> sampleRates;
    juce::Array<int> bufferSizes;
    int defaultBufferSize = 0;
};

/** One call of AudioIODevice::open(): what the device manager asked the "hardware" for. */
struct OpenCall
{
    juce::String inputDevice;
    juce::String outputDevice;
    juce::BigInteger inputChannels;
    juce::BigInteger outputChannels;
    double sampleRate = 0.0;
    int bufferSize = 0;
};

/** What the fake hardware saw. Shared by the types and devices of one engine; tests read it. */
struct Backend
{
    std::vector<OpenCall> opens;
    int devicesCreated = 0;
    int devicesStarted = 0;
    juce::AudioIODeviceCallback* callback = nullptr;  ///< the callback of the running device (nullptr = stopped)
};

class FakeDevice final : public juce::AudioIODevice
{
public:
    FakeDevice(const juce::String& typeName, const juce::String& outputName, const juce::String& inputName,
               const DeviceSpec* inputSpec, const DeviceSpec* outputSpec, std::shared_ptr<Backend> sharedBackend)
        : juce::AudioIODevice(outputName.isNotEmpty() ? outputName : inputName, typeName), inputDeviceName(inputName),
          outputDeviceName(outputName), backend(std::move(sharedBackend))
    {
        if (inputSpec != nullptr)
            inputNames = inputSpec->inputChannelNames;
        if (outputSpec != nullptr)
            outputNames = outputSpec->outputChannelNames;

        const DeviceSpec* first = outputSpec != nullptr ? outputSpec : inputSpec;
        rates = first->sampleRates;
        buffers = first->bufferSizes;
        defaultBuffer = first->defaultBufferSize;
        if (inputSpec != nullptr && outputSpec != nullptr && inputSpec != outputSpec)
        {
            rates.removeIf([&](double r) { return !inputSpec->sampleRates.contains(r); });
            buffers.removeIf([&](int b) { return !inputSpec->bufferSizes.contains(b); });
            if (!buffers.contains(defaultBuffer) && !buffers.isEmpty())
                defaultBuffer = buffers.getFirst();
        }
    }

    ~FakeDevice() override { stop(); }

    juce::StringArray getOutputChannelNames() override { return outputNames; }
    juce::StringArray getInputChannelNames() override { return inputNames; }
    juce::Array<double> getAvailableSampleRates() override { return rates; }
    juce::Array<int> getAvailableBufferSizes() override { return buffers; }
    int getDefaultBufferSize() override { return defaultBuffer; }

    juce::String open(const juce::BigInteger& inputChannels, const juce::BigInteger& outputChannels, double sampleRate,
                      int bufferSize) override
    {
        if (!rates.contains(sampleRate))
            return "fake device: unsupported sample rate " + juce::String(sampleRate);
        if (!buffers.contains(bufferSize))
            return "fake device: unsupported buffer size " + juce::String(bufferSize);

        backend->opens.push_back(
            {inputDeviceName, outputDeviceName, inputChannels, outputChannels, sampleRate, bufferSize});

        juce::BigInteger inputMask;
        inputMask.setRange(0, inputNames.size(), true);
        juce::BigInteger outputMask;
        outputMask.setRange(0, outputNames.size(), true);
        activeInputs = inputChannels & inputMask;
        activeOutputs = outputChannels & outputMask;
        currentRate = sampleRate;
        currentBuffer = bufferSize;
        opened = true;
        return {};
    }

    void close() override
    {
        stop();
        opened = false;
    }

    bool isOpen() override { return opened; }

    void start(juce::AudioIODeviceCallback* newCallback) override
    {
        if (newCallback == nullptr || !opened)
            return;
        stop();
        callback = newCallback;
        backend->callback = newCallback;
        ++backend->devicesStarted;
        playing = true;
        newCallback->audioDeviceAboutToStart(this);
    }

    void stop() override
    {
        if (callback != nullptr)
        {
            auto* old = callback;
            callback = nullptr;
            backend->callback = nullptr;
            playing = false;
            old->audioDeviceStopped();
        }
    }

    bool isPlaying() override { return playing; }
    juce::String getLastError() override { return {}; }
    int getCurrentBufferSizeSamples() override { return currentBuffer; }
    double getCurrentSampleRate() override { return currentRate; }
    int getCurrentBitDepth() override { return 32; }
    juce::BigInteger getActiveOutputChannels() const override { return activeOutputs; }
    juce::BigInteger getActiveInputChannels() const override { return activeInputs; }
    int getOutputLatencyInSamples() override { return 0; }
    int getInputLatencyInSamples() override { return 0; }

    const juce::String& inputName() const { return inputDeviceName; }
    const juce::String& outputName() const { return outputDeviceName; }

private:
    juce::String inputDeviceName;
    juce::String outputDeviceName;
    juce::StringArray inputNames;
    juce::StringArray outputNames;
    juce::Array<double> rates;
    juce::Array<int> buffers;
    int defaultBuffer = 0;
    std::shared_ptr<Backend> backend;
    juce::AudioIODeviceCallback* callback = nullptr;
    juce::BigInteger activeInputs;
    juce::BigInteger activeOutputs;
    double currentRate = 0.0;
    int currentBuffer = 0;
    bool opened = false;
    bool playing = false;
};

class FakeDeviceType final : public juce::AudioIODeviceType
{
public:
    /** Separate inputs and outputs. */
    static std::unique_ptr<FakeDeviceType> separate(const juce::String& typeName, std::vector<DeviceSpec> inputs,
                                                    std::vector<DeviceSpec> outputs, std::shared_ptr<Backend> backend)
    {
        return std::unique_ptr<FakeDeviceType>(
            new FakeDeviceType(typeName, true, std::move(inputs), std::move(outputs), std::move(backend)));
    }

    /** One device for both directions. */
    static std::unique_ptr<FakeDeviceType> duplex(const juce::String& typeName, std::vector<DeviceSpec> devices,
                                                  std::shared_ptr<Backend> backend)
    {
        auto outputs = devices;
        return std::unique_ptr<FakeDeviceType>(
            new FakeDeviceType(typeName, false, std::move(devices), std::move(outputs), std::move(backend)));
    }

    void scanForDevices() override {}

    juce::StringArray getDeviceNames(bool wantInputNames) const override
    {
        juce::StringArray names;
        for (const auto& spec : (wantInputNames || !separateIO ? inputs : outputs))
            names.add(spec.name);
        return names;
    }

    int getDefaultDeviceIndex(bool forInput) const override { return getDeviceNames(forInput).isEmpty() ? -1 : 0; }

    int getIndexOfDevice(juce::AudioIODevice* device, bool asInput) const override
    {
        if (auto* fake = dynamic_cast<FakeDevice*>(device))
        {
            const auto& name = asInput ? fake->inputName() : fake->outputName();
            return getDeviceNames(asInput).indexOf(name);
        }
        return -1;
    }

    bool hasSeparateInputsAndOutputs() const override { return separateIO; }

    juce::AudioIODevice* createDevice(const juce::String& outputDeviceName,
                                      const juce::String& inputDeviceName) override
    {
        if (outputDeviceName.isEmpty() && inputDeviceName.isEmpty())
            return nullptr;

        const DeviceSpec* in = nullptr;
        const DeviceSpec* out = nullptr;
        if (separateIO)
        {
            if (inputDeviceName.isNotEmpty() && (in = find(inputs, inputDeviceName)) == nullptr)
                return nullptr;
            if (outputDeviceName.isNotEmpty() && (out = find(outputs, outputDeviceName)) == nullptr)
                return nullptr;
        }
        else
        {
            const auto& name = outputDeviceName.isNotEmpty() ? outputDeviceName : inputDeviceName;
            if (inputDeviceName.isNotEmpty() && outputDeviceName.isNotEmpty() && inputDeviceName != outputDeviceName)
                return nullptr;
            in = out = find(inputs, name);
            if (in == nullptr)
                return nullptr;
        }

        ++backend->devicesCreated;
        return new FakeDevice(getTypeName(), outputDeviceName, inputDeviceName, in, out, backend);
    }

private:
    FakeDeviceType(const juce::String& typeName, bool separateInputsAndOutputs, std::vector<DeviceSpec> inputSpecs,
                   std::vector<DeviceSpec> outputSpecs, std::shared_ptr<Backend> sharedBackend)
        : juce::AudioIODeviceType(typeName), separateIO(separateInputsAndOutputs), inputs(std::move(inputSpecs)),
          outputs(std::move(outputSpecs)), backend(std::move(sharedBackend))
    {
    }

    static const DeviceSpec* find(const std::vector<DeviceSpec>& specs, const juce::String& name)
    {
        for (const auto& spec : specs)
            if (spec.name == name)
                return &spec;
        return nullptr;
    }

    bool separateIO;
    std::vector<DeviceSpec> inputs;
    std::vector<DeviceSpec> outputs;
    std::shared_ptr<Backend> backend;
};

//==============================================================================
inline juce::StringArray numbered(const juce::String& prefix, int count)
{
    juce::StringArray names;
    for (int i = 1; i <= count; ++i)
        names.add(prefix + " " + juce::String(i));
    return names;
}

/** Which of the fake device types a test engine gets. */
struct Hardware
{
    bool separateType = true;    ///< "Fake Separate"
    bool duplexType = true;      ///< "Fake Duplex"
    bool jackType = true;        ///< "JACK"
    bool jackHasDevice = false;  ///< JACK with one input and one output client (a running server) instead of none
};

/** Adds the fake types to `manager` (in the order separate, duplex, JACK) before anything scans. The system device
    types are not there: the engine of the tests is created with DeviceMode::none. */
inline void addFakeTypes(juce::AudioDeviceManager& manager, const std::shared_ptr<Backend>& backend,
                         const Hardware& hardware = {})
{
    if (hardware.separateType)
    {
        auto device = [](const char* name, juce::StringArray ins, juce::StringArray outs, juce::Array<double> rates,
                         juce::Array<int> buffers, int defaultBuffer) {
            return DeviceSpec{name,         std::move(ins), std::move(outs), std::move(rates), std::move(buffers),
                              defaultBuffer};
        };

        const juce::Array<double> rates{44100.0, 48000.0, 96000.0};
        const juce::Array<int> buffers{64, 128, 256, 512};
        std::vector<DeviceSpec> inputs{device(kMicInterface, numbered("Mic", 4), {}, rates, buffers, 256),
                                       device(kLineInterface, numbered("Line", 2), {}, rates, buffers, 256)};
        std::vector<DeviceSpec> outputs{device(kSpeakers, {}, juce::StringArray{"Left", "Right"}, rates, buffers, 256),
                                        device(kHeadphones, {}, juce::StringArray{"HP L", "HP R"},
                                               juce::Array<double>{48000.0}, juce::Array<int>{256}, 256)};
        manager.addAudioDeviceType(
            FakeDeviceType::separate(kSeparateType, std::move(inputs), std::move(outputs), backend));
    }

    if (hardware.duplexType)
    {
        std::vector<DeviceSpec> devices{
            DeviceSpec{
                kDuplexInterface, numbered("In", 8), numbered("Out", 8), {44100.0, 48000.0}, {32, 64, 128, 256}, 128},
            DeviceSpec{kSecondBox, numbered("Box In", 2), numbered("Box Out", 2), {48000.0}, {128}, 128}};
        manager.addAudioDeviceType(FakeDeviceType::duplex(kDuplexType, std::move(devices), backend));
    }

    if (hardware.jackType)
    {
        std::vector<DeviceSpec> inputs;
        std::vector<DeviceSpec> outputs;
        if (hardware.jackHasDevice)
        {
            inputs.push_back(DeviceSpec{"Fake JACK Client", numbered("in", 2), {}, {48000.0}, {256}, 256});
            outputs.push_back(DeviceSpec{"Fake JACK Client", {}, numbered("out", 2), {48000.0}, {256}, 256});
        }
        manager.addAudioDeviceType(FakeDeviceType::separate(kJackType, std::move(inputs), std::move(outputs), backend));
    }
}

}  // namespace tracklab_test::fake
