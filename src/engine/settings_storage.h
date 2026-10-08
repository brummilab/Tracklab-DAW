// Settings storage of the Tracklab engine (internal to the engine module, see engine_factory.h for the contract).
//
// Both variants keep every value in memory as a juce::var. The file variant additionally persists them to
// `settings.xml`. Nothing here is reachable from the audio thread: settings are read and written by the message
// thread and by background jobs only (docs/realtime.md).
#pragma once

#include <tracktion_engine/tracktion_engine.h>

#include <atomic>
#include <map>
#include <memory>

namespace tracklab::engine::detail
{

namespace te = tracktion;

/** Writes `content` to `target` atomically: the text goes to a temporary file in the same folder (same file system,
    so that the final rename cannot degrade to a copy), is flushed to disk, and then replaces `target`.
    On any failure `target` stays as it was, no temporary file is left behind, and nothing is thrown.
    Creates the folder of `target` if it is missing. Allocates and does IO: never call it from the audio thread. */
[[nodiscard]] bool writeFileAtomically(const juce::File& target, const juce::String& content);

/** `file` as shown in a log: the user's home folder (File::userHomeDirectory) at the start of the path is replaced by
    `~` (only as a whole path component: a sibling that merely starts with the same characters is left alone). Paths
    outside the home folder are unchanged. Log files can be shared; the home path contains the account name. */
[[nodiscard]] juce::String pathForLog(const juce::File& file);

/** Base of the two variants: all PropertyStorage accessors on top of one map.
    A lock guards the map because background jobs may read settings while the message thread writes them. */
class TracklabPropertyStorage : public te::PropertyStorage
{
public:
    TracklabPropertyStorage();

    void removeProperty(te::SettingID setting) override;
    juce::var getProperty(te::SettingID setting, const juce::var& defaultValue) override;
    void setProperty(te::SettingID setting, const juce::var& value) override;
    std::unique_ptr<juce::XmlElement> getXmlProperty(te::SettingID setting) override;
    void setXmlProperty(te::SettingID setting, const juce::XmlElement& xml) override;

    void removePropertyItem(te::SettingID setting, juce::StringRef item) override;
    juce::var getPropertyItem(te::SettingID setting, juce::StringRef item, const juce::var& defaultValue) override;
    void setPropertyItem(te::SettingID setting, juce::StringRef item, const juce::var& value) override;
    std::unique_ptr<juce::XmlElement> getXmlPropertyItem(te::SettingID setting, juce::StringRef item) override;
    void setXmlPropertyItem(te::SettingID setting, juce::StringRef item, const juce::XmlElement& xml) override;

    /** Never the login or full name of the system user: the name ends up in saved projects. */
    juce::String getUserName() override;
    juce::String getApplicationVersion() override;

    /** The base class would create a PropertiesFile in the user's folder; this storage has none. */
    juce::PropertiesFile& getPropertiesFile() override;

protected:
    /** Called after every change (lock not held); the file variant schedules a write. */
    virtual void valuesChanged() {}

    static juce::String key(te::SettingID setting);
    static juce::String key(te::SettingID setting, juce::StringRef item);

    mutable juce::CriticalSection lock;
    std::map<juce::String, juce::var> values;

private:
    juce::var lookup(const juce::String& k, const juce::var& defaultValue) const;
    std::unique_ptr<juce::XmlElement> lookupXml(const juce::String& k) const;
    void store(const juce::String& k, const juce::var& value);
    void erase(const juce::String& k);

    std::unique_ptr<juce::PropertiesFile> legacyPropertiesFile;
};

/** Settings in memory only; prefs and cache folders are a private temporary folder deleted with the storage. */
class MemoryPropertyStorage final : public TracklabPropertyStorage
{
public:
    MemoryPropertyStorage();
    ~MemoryPropertyStorage() override;

    juce::File getAppCacheFolder() override;
    juce::File getAppPrefsFolder() override;
    juce::File getDefaultLoadSaveDirectory(juce::StringRef label) override;
    juce::File getDefaultLoadSaveDirectory(te::ProjectItem::Category category) override;

private:
    juce::File scratchDir;

    juce::File subFolder(const char* name) const;
};

/** Settings in `<directory>/settings.xml`.

    No IO per setProperty(): a change only marks the storage dirty and starts a 2 s message-thread timer (like
    Tracktion's own PropertiesFile); the timer, flushSettingsToDisk() and the destructor write the whole file.
    The cache folder is a private temporary folder: Tracktion fills it with throw-away files, and the settings folder
    stays a place that holds settings.xml only. */
class FilePropertyStorage final : public TracklabPropertyStorage, private juce::Timer
{
public:
    explicit FilePropertyStorage(juce::File settingsDirectory);
    ~FilePropertyStorage() override;

    juce::File getAppCacheFolder() override;
    juce::File getAppPrefsFolder() override;
    void flushSettingsToDisk() override;

private:
    juce::File directory;
    juce::File cacheDir;
    std::atomic<bool> dirty{false};

    juce::File settingsFile() const { return directory.getChildFile("settings.xml"); }

    void load();
    bool writeNow();
    void valuesChanged() override;
    void timerCallback() override;
};

}  // namespace tracklab::engine::detail
