#include "engine/engine_factory.h"

#include "engine/cache_folder.h"
#include "engine/settings_storage.h"

namespace tracklab::engine
{

namespace
{

/** No audio device unless the app asks for it (headless CLI and CI have none, and probing hardware can hang). */
class TracklabEngineBehaviour final : public te::EngineBehaviour
{
public:
    explicit TracklabEngineBehaviour(DeviceMode m) : mode(m) {}

    bool autoInitialiseDeviceManager() override { return mode == DeviceMode::automatic; }
    bool addSystemAudioIODeviceTypes() override { return mode == DeviceMode::automatic; }

private:
    DeviceMode mode;
};

/** A UIBehaviour that never shows anything and never waits for a person. */
class HeadlessUIBehaviour final : public te::UIBehaviour
{
public:
    // Runs the job on a worker thread and keeps dispatching messages meanwhile: render jobs post work to the
    // message thread (e.g. edit updates, file mapping) and would dead-lock if this thread just waited.
    void runTaskWithProgressBar(te::ThreadPoolJobWithProgress& task) override
    {
        TaskThread thread(task);
        thread.startThread();

        while (thread.isThreadRunning())
            if (!juce::MessageManager::getInstance()->runDispatchLoopUntil(5))
                break;

        thread.stopThread(-1);
    }

    // The engine reports failures through alerts. There is no window to show them in, so they go to the log.
    void showWarningAlert(const juce::String& title, const juce::String& message) override
    {
        juce::Logger::writeToLog("Tracklab engine warning: " + title + ": " + message);
    }

    void showWarningMessage(const juce::String& message) override
    {
        juce::Logger::writeToLog("Tracklab engine warning: " + message);
    }

    void showInfoMessage(const juce::String&) override {}

    // Nobody can answer: the question is declined at once (deterministic, never hangs). Declining is the safe
    // answer to "overwrite?", "discard?" and "delete?" alike.
    void showOkCancelAlertBoxAsync(const juce::String&, const juce::String&, const juce::String&, const juce::String&,
                                   std::function<void(bool)> callback) override
    {
        if (callback)
            callback(false);
    }

    void showYesNoCancelAlertBoxAsync(const juce::String&, const juce::String&, const juce::String&,
                                      const juce::String&, const juce::String&,
                                      std::function<void(int)> callback) override
    {
        if (callback)
            callback(0);  // 0 = cancel
    }

    // No live waveform while recording: a headless recorder does not need one, and it adds work per block.
    bool shouldGenerateLiveWaveformsWhenRecording() override { return false; }

private:
    class TaskThread final : public juce::Thread
    {
    public:
        explicit TaskThread(te::ThreadPoolJobWithProgress& t) : juce::Thread("tracklab task"), task(t) {}

        void run() override
        {
            while (!threadShouldExit())
                if (task.runJob() == juce::ThreadPoolJob::jobHasFinished)
                    break;
        }

    private:
        te::ThreadPoolJobWithProgress& task;
    };
};

}  // namespace

juce::File defaultSettingsDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Tracklab");
}

juce::File defaultCacheDirectory()
{
    // Environment variables are the documented way to find these folders; JUCE has no special location for them.
#if JUCE_WINDOWS
    const auto localAppData = juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA", {});
    const auto base = localAppData.isNotEmpty() ? juce::File(localAppData)
                                                : juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                                                      .getChildFile("AppData")
                                                      .getChildFile("Local");
    return base.getChildFile("Tracklab").getChildFile("cache");
#else
    // XDG Base Directory spec: an empty value or a relative path counts as not set.
    const auto xdg = juce::SystemStats::getEnvironmentVariable("XDG_CACHE_HOME", {});
    if (xdg.isNotEmpty() && juce::File::isAbsolutePath(xdg))
        return juce::File(xdg).getChildFile("Tracklab");
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile(".cache")
        .getChildFile("Tracklab");
#endif
}

std::unique_ptr<te::Engine> createEngine(const EngineOptions& options)
{
    // The base of the private caches is a folder of this user (not the shared temp folder) unless the caller names
    // one: in a folder that other users can write to, the cleanup below could be led astray.
    const auto privateBase = detail::resolvePrivateBase(options.tempDirectory);

    // First, so that the folder of this engine is not even a candidate. Folders of running engines are kept.
    if (privateBase.cleanable)
        detail::removeOrphanedPrivateCaches(privateBase.folder);

    auto cache = options.cache == CacheMode::persistent
                     ? detail::CacheFolder::makePersistent(
                           options.cacheDirectory == juce::File() ? defaultCacheDirectory() : options.cacheDirectory)
                     : detail::CacheFolder::makePrivate(privateBase.folder);

    std::unique_ptr<te::PropertyStorage> storage;
    if (options.storage == SettingsStorage::file)
        storage = std::make_unique<detail::FilePropertyStorage>(
            options.settingsDirectory == juce::File() ? defaultSettingsDirectory() : options.settingsDirectory,
            std::move(cache));
    else
        storage = std::make_unique<detail::MemoryPropertyStorage>(std::move(cache));

    return std::make_unique<te::Engine>(std::move(storage), std::make_unique<HeadlessUIBehaviour>(),
                                        std::make_unique<TracklabEngineBehaviour>(options.devices));
}

}  // namespace tracklab::engine
