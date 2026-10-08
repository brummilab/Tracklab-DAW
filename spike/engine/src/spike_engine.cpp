#include "spike_engine.h"

#include <map>

namespace spike::detail
{

juce::File toJuceFile(const std::filesystem::path& path)
{
    // juce::File only takes absolute paths; the CLI accepts paths relative to the working directory.
    std::error_code ec;
    const auto absolute = path.is_absolute() ? path : std::filesystem::absolute(path, ec);
    const auto utf8 = absolute.u8string();
    return juce::File(
        juce::String::fromUTF8(reinterpret_cast<const char*>(utf8.c_str()), static_cast<int>(utf8.size())));
}

std::filesystem::path toStdPath(const juce::File& file)
{
    const auto utf8 = file.getFullPathName().toStdString();
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

std::string toStd(const juce::String& text)
{
    return text.toStdString();
}

namespace
{

/** Settings in memory only. The default PropertyStorage writes Settings.xml into the user's application data
    folder; a spike (and later the tests of Tracklab) must not depend on or change that. */
class MemoryPropertyStorage final : public te::PropertyStorage
{
public:
    explicit MemoryPropertyStorage(juce::File scratch) : PropertyStorage("TracklabEngineSpike"), dir(std::move(scratch))
    {
    }

    juce::File getAppCacheFolder() override { return subFolder("cache"); }
    juce::File getAppPrefsFolder() override { return subFolder("prefs"); }

    void removeProperty(te::SettingID setting) override { values.erase(key(setting)); }

    juce::var getProperty(te::SettingID setting, const juce::var& defaultValue) override
    {
        return lookup(key(setting), defaultValue);
    }

    void setProperty(te::SettingID setting, const juce::var& value) override { values[key(setting)] = value; }

    std::unique_ptr<juce::XmlElement> getXmlProperty(te::SettingID setting) override { return lookupXml(key(setting)); }

    void setXmlProperty(te::SettingID setting, const juce::XmlElement& xml) override
    {
        values[key(setting)] = xml.toString();
    }

    void removePropertyItem(te::SettingID setting, juce::StringRef item) override { values.erase(key(setting, item)); }

    juce::var getPropertyItem(te::SettingID setting, juce::StringRef item, const juce::var& defaultValue) override
    {
        return lookup(key(setting, item), defaultValue);
    }

    void setPropertyItem(te::SettingID setting, juce::StringRef item, const juce::var& value) override
    {
        values[key(setting, item)] = value;
    }

    std::unique_ptr<juce::XmlElement> getXmlPropertyItem(te::SettingID setting, juce::StringRef item) override
    {
        return lookupXml(key(setting, item));
    }

    void setXmlPropertyItem(te::SettingID setting, juce::StringRef item, const juce::XmlElement& xml) override
    {
        values[key(setting, item)] = xml.toString();
    }

    juce::File getDefaultLoadSaveDirectory(juce::StringRef) override { return dir; }
    juce::File getDefaultLoadSaveDirectory(te::ProjectItem::Category) override { return dir; }
    juce::String getUserName() override { return "spike"; }

private:
    juce::File dir;
    std::map<juce::String, juce::var> values;

    juce::File subFolder(const char* name) const
    {
        auto f = dir.getChildFile(name);
        f.createDirectory();
        return f;
    }

    static juce::String key(te::SettingID setting) { return juce::String(settingToString(setting)); }

    static juce::String key(te::SettingID setting, juce::StringRef item)
    {
        return key(setting) + "_" + juce::String(item);
    }

    juce::var lookup(const juce::String& k, const juce::var& defaultValue) const
    {
        const auto it = values.find(k);
        return it != values.end() ? it->second : defaultValue;
    }

    std::unique_ptr<juce::XmlElement> lookupXml(const juce::String& k) const
    {
        const auto it = values.find(k);
        return it != values.end() ? juce::parseXML(it->second.toString()) : nullptr;
    }
};

}  // namespace

//==============================================================================
class SpikeEngineBehaviour final : public te::EngineBehaviour
{
public:
    // Headless: no system audio device is opened. record-12 installs the hosted device itself.
    bool autoInitialiseDeviceManager() override { return false; }
    bool addSystemAudioIODeviceTypes() override { return false; }

    juce::File getFileForNewAudioRecording(te::Track& track, const juce::String& fileExtension) override
    {
        if (recordingDir == juce::File())
            return {};

        const int index = te::getAudioTracks(track.edit).indexOf(dynamic_cast<te::AudioTrack*>(&track));
        return recordingDir.getChildFile("input-" + juce::String(index + 1).paddedLeft('0', 2) + fileExtension);
    }

    juce::File recordingDir;
};

//==============================================================================
class SpikeUIBehaviour final : public te::UIBehaviour
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

    // The engine reports failures (e.g. "Couldn't record!") through alerts; keep the text for the caller.
    void showWarningAlert(const juce::String& title, const juce::String& message) override
    {
        lastWarning = title + ": " + message;
    }

    void showWarningMessage(const juce::String& message) override { lastWarning = message; }
    void showInfoMessage(const juce::String&) override {}

    // No live waveform while recording: a headless recorder does not need one, and it adds work per block.
    bool shouldGenerateLiveWaveformsWhenRecording() override { return false; }

    juce::String lastWarning;

private:
    class TaskThread final : public juce::Thread
    {
    public:
        explicit TaskThread(te::ThreadPoolJobWithProgress& t) : juce::Thread("spike task"), task(t) {}

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

//==============================================================================
SpikeEngine::SpikeEngine() : scratchDir(juce::File::createTempFile("tracklab-spike-engine"))
{
    scratchDir.createDirectory();

    auto behaviour = std::make_unique<SpikeEngineBehaviour>();
    auto ui = std::make_unique<SpikeUIBehaviour>();
    engineBehaviour = behaviour.get();
    uiBehaviour = ui.get();
    engine = std::make_unique<te::Engine>(std::make_unique<MemoryPropertyStorage>(scratchDir), std::move(ui),
                                          std::move(behaviour));
}

SpikeEngine::~SpikeEngine()
{
    engine.reset();
    scratchDir.deleteRecursively();
}

void SpikeEngine::setRecordingDirectory(const juce::File& dir)
{
    engineBehaviour->recordingDir = dir;
}

juce::String SpikeEngine::takeLastWarning()
{
    auto text = uiBehaviour->lastWarning;
    uiBehaviour->lastWarning = {};
    return text;
}

}  // namespace spike::detail
