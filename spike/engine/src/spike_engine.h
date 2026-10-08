// Engine factory of the spike (internal, not part of spike_common.h).
//
// Every spike operation creates its own SpikeEngine and destroys it before returning
// (TRACKTION_ENABLE_SINGLETONS=0): the tests call the operations many times in one process.
#pragma once

#include <tracktion_engine/tracktion_engine.h>

#include <filesystem>
#include <memory>
#include <string>

namespace spike::detail
{

namespace te = tracktion;

/** UTF-8 safe conversion between std::filesystem and JUCE paths (Windows paths are UTF-16). */
juce::File toJuceFile(const std::filesystem::path& path);
std::filesystem::path toStdPath(const juce::File& file);

/** Formats a juce::String error as std::string. */
std::string toStd(const juce::String& text);

class SpikeEngineBehaviour;
class SpikeUIBehaviour;

/** A headless Tracktion Engine with in-memory settings.

    - The settings never touch the user's Tracktion settings file: they live in memory, and the prefs/cache
      folders Tracktion asks for are a private temporary folder that is deleted with the engine.
    - The device manager is not initialised automatically, so no system audio device is opened (headless CI).
      record-12 installs the HostedAudioDeviceInterface instead.
    - Background tasks (e.g. renders) run on a worker thread while the calling (message) thread keeps
      dispatching messages; warnings from the engine are collected instead of shown. */
class SpikeEngine
{
public:
    SpikeEngine();
    ~SpikeEngine();

    SpikeEngine(const SpikeEngine&) = delete;
    SpikeEngine& operator=(const SpikeEngine&) = delete;

    te::Engine& get() noexcept { return *engine; }

    /** Recordings go to `dir/input-NN.<ext>`, NN = 1-based index of the target audio track. */
    void setRecordingDirectory(const juce::File& dir);

    /** The last warning/alert text the engine reported through UIBehaviour (empty if none), and clears it. */
    juce::String takeLastWarning();

private:
    juce::File scratchDir;
    SpikeEngineBehaviour* engineBehaviour = nullptr;  // owned by `engine`
    SpikeUIBehaviour* uiBehaviour = nullptr;          // owned by `engine`
    std::unique_ptr<te::Engine> engine;
};

}  // namespace spike::detail
