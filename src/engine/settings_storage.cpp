#include "engine/settings_storage.h"

#include <bit>

namespace tracklab::engine::detail
{

namespace
{

constexpr int kWriteDelayMs = 2000;  // same delay as Tracktion's own PropertiesFile, so that bursts become one write

/** Attribute values and tags of settings.xml. The root/element names match juce::PropertiesFile's XML format. */
constexpr const char* kRootTag = "PROPERTIES";
constexpr const char* kValueTag = "VALUE";

/** One value as `<VALUE name=".." type=".." val=".."/>`. The type attribute keeps the juce::var type across a
    restart (a plain PropertiesFile turns everything into text). Doubles are stored as their IEEE bit pattern in hex:
    a decimal text would need care to round-trip, and settings are not meant to be edited by hand. */
std::unique_ptr<juce::XmlElement> encodeValue(const juce::String& name, const juce::var& value)
{
    auto element = std::make_unique<juce::XmlElement>(kValueTag);
    element->setAttribute("name", name);

    if (value.isBool())
    {
        element->setAttribute("type", "bool");
        element->setAttribute("val", static_cast<bool>(value) ? "1" : "0");
    }
    else if (value.isInt())
    {
        element->setAttribute("type", "int");
        element->setAttribute("val", static_cast<int>(value));
    }
    else if (value.isInt64())
    {
        element->setAttribute("type", "int64");
        element->setAttribute("val", juce::String(static_cast<juce::int64>(value)));
    }
    else if (value.isDouble())
    {
        const auto bits = std::bit_cast<juce::uint64>(static_cast<double>(value));
        element->setAttribute("type", "double");
        element->setAttribute("val", juce::String::toHexString(static_cast<juce::int64>(bits)));
    }
    else
    {
        // Strings, and the string form of anything else (the engine does not store arrays or objects).
        element->setAttribute("type", "string");
        element->setAttribute("val", value.toString());
    }
    return element;
}

bool decodeValue(const juce::XmlElement& element, juce::String& name, juce::var& value)
{
    if (!element.hasAttribute("name") || !element.hasAttribute("val"))
        return false;

    name = element.getStringAttribute("name");
    const auto& type = element.getStringAttribute("type", "string");
    const auto& text = element.getStringAttribute("val");

    if (type == "bool")
        value = text == "1";
    else if (type == "int")
        value = text.getIntValue();
    else if (type == "int64")
        value = text.getLargeIntValue();
    else if (type == "double")
        value = std::bit_cast<double>(static_cast<juce::uint64>(text.getHexValue64()));
    else
        value = text;
    return true;
}

}  // namespace

//==============================================================================
juce::String pathForLog(const juce::File& file)
{
    const auto& path = file.getFullPathName();
    const auto home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);

    // A home folder that is the file system root would turn every path into "~/...": nothing to hide there.
    if (home == juce::File() || home.getParentDirectory() == home)
        return path;
    if (file == home)
        return "~";

    // isAChildOf() compares whole path components, so "/home/a-other" is not below "/home/a". The rest keeps the
    // native separator, as in the full path.
    if (file.isAChildOf(home))
        return "~" + path.substring(home.getFullPathName().length());
    return path;
}

//==============================================================================
bool writeFileAtomically(const juce::File& target, const juce::String& content)
{
    if (!target.getParentDirectory().createDirectory())
    {
        juce::Logger::writeToLog("Tracklab: cannot create the folder of " + pathForLog(target));
        return false;
    }

    // Next to the target (same file system), deleted by the destructor if it is still there (failed write).
    const juce::TemporaryFile temporary(target);
    {
        juce::FileOutputStream out(temporary.getFile());
        if (out.failedToOpen())
        {
            juce::Logger::writeToLog("Tracklab: cannot write " + pathForLog(temporary.getFile()) + ": " +
                                     out.getStatus().getErrorMessage());
            return false;
        }

        const auto utf8 = content.toUTF8();
        if (!out.write(utf8.getAddress(), utf8.sizeInBytes() - 1))
        {
            juce::Logger::writeToLog("Tracklab: cannot write " + pathForLog(temporary.getFile()));
            return false;
        }
        // Data must be on disk before the rename, otherwise a crash could leave an empty file in place of the old one.
        out.flush();
        if (out.getStatus().failed())
        {
            juce::Logger::writeToLog("Tracklab: cannot flush " + pathForLog(temporary.getFile()));
            return false;
        }
    }

    // replaceFileIn() and not overwriteTargetFileWithTemporary(): both replace the target by renaming, but the latter
    // asserts in debug builds when that fails, and a full disk or a locked file is a normal condition for a settings
    // write.
    if (!temporary.getFile().replaceFileIn(target))
    {
        juce::Logger::writeToLog("Tracklab: cannot replace " + pathForLog(target));
        return false;
    }
    return true;
}

//==============================================================================
TracklabPropertyStorage::TracklabPropertyStorage() : te::PropertyStorage("Tracklab") {}

juce::String TracklabPropertyStorage::key(te::SettingID setting)
{
    return juce::String(settingToString(setting));
}

juce::String TracklabPropertyStorage::key(te::SettingID setting, juce::StringRef item)
{
    return key(setting) + "_" + juce::String(item);
}

juce::var TracklabPropertyStorage::lookup(const juce::String& k, const juce::var& defaultValue) const
{
    const juce::ScopedLock guard(lock);
    const auto it = values.find(k);
    return it != values.end() ? it->second : defaultValue;
}

std::unique_ptr<juce::XmlElement> TracklabPropertyStorage::lookupXml(const juce::String& k) const
{
    const juce::ScopedLock guard(lock);
    const auto it = values.find(k);
    return it != values.end() ? juce::parseXML(it->second.toString()) : nullptr;
}

void TracklabPropertyStorage::store(const juce::String& k, const juce::var& value)
{
    {
        const juce::ScopedLock guard(lock);
        values[k] = value;
    }
    valuesChanged();
}

void TracklabPropertyStorage::erase(const juce::String& k)
{
    bool removed = false;
    {
        const juce::ScopedLock guard(lock);
        removed = values.erase(k) > 0;
    }
    if (removed)
        valuesChanged();
}

void TracklabPropertyStorage::removeProperty(te::SettingID setting)
{
    erase(key(setting));
}

juce::var TracklabPropertyStorage::getProperty(te::SettingID setting, const juce::var& defaultValue)
{
    return lookup(key(setting), defaultValue);
}

void TracklabPropertyStorage::setProperty(te::SettingID setting, const juce::var& value)
{
    store(key(setting), value);
}

std::unique_ptr<juce::XmlElement> TracklabPropertyStorage::getXmlProperty(te::SettingID setting)
{
    return lookupXml(key(setting));
}

void TracklabPropertyStorage::setXmlProperty(te::SettingID setting, const juce::XmlElement& xml)
{
    store(key(setting), xml.toString());
}

void TracklabPropertyStorage::removePropertyItem(te::SettingID setting, juce::StringRef item)
{
    erase(key(setting, item));
}

juce::var TracklabPropertyStorage::getPropertyItem(te::SettingID setting, juce::StringRef item,
                                                   const juce::var& defaultValue)
{
    return lookup(key(setting, item), defaultValue);
}

void TracklabPropertyStorage::setPropertyItem(te::SettingID setting, juce::StringRef item, const juce::var& value)
{
    store(key(setting, item), value);
}

std::unique_ptr<juce::XmlElement> TracklabPropertyStorage::getXmlPropertyItem(te::SettingID setting,
                                                                              juce::StringRef item)
{
    return lookupXml(key(setting, item));
}

void TracklabPropertyStorage::setXmlPropertyItem(te::SettingID setting, juce::StringRef item,
                                                 const juce::XmlElement& xml)
{
    store(key(setting, item), xml.toString());
}

juce::String TracklabPropertyStorage::getUserName()
{
    return "Tracklab";
}

juce::String TracklabPropertyStorage::getApplicationVersion()
{
    return TRACKLAB_VERSION_STRING;
}

juce::PropertiesFile& TracklabPropertyStorage::getPropertiesFile()
{
    // Nothing in Tracktion calls this. The base class would open `<prefs folder>/Settings.xml`, which collides with
    // our settings.xml on case-insensitive file systems, so a caller gets a file in the private cache folder instead.
    if (legacyPropertiesFile == nullptr)
    {
        juce::PropertiesFile::Options options;
        options.storageFormat = juce::PropertiesFile::storeAsXML;
        legacyPropertiesFile =
            std::make_unique<juce::PropertiesFile>(getAppCacheFolder().getChildFile("properties-unused.xml"), options);
    }
    return *legacyPropertiesFile;
}

//==============================================================================
MemoryPropertyStorage::MemoryPropertyStorage(std::unique_ptr<CacheFolder> cacheFolder)
    : cache(std::move(cacheFolder)), scratchDir(juce::File::createTempFile("tracklab-engine"))
{
    scratchDir.createDirectory();
}

MemoryPropertyStorage::~MemoryPropertyStorage()
{
    scratchDir.deleteRecursively();
}

juce::File MemoryPropertyStorage::subFolder(const char* name) const
{
    auto folder = scratchDir.getChildFile(name);
    folder.createDirectory();
    return folder;
}

juce::File MemoryPropertyStorage::getAppCacheFolder()
{
    return cache->getFolder();
}

juce::File MemoryPropertyStorage::getAppPrefsFolder()
{
    return subFolder("prefs");
}

juce::File MemoryPropertyStorage::getDefaultLoadSaveDirectory(juce::StringRef)
{
    return scratchDir;
}

juce::File MemoryPropertyStorage::getDefaultLoadSaveDirectory(te::ProjectItem::Category)
{
    return scratchDir;
}

//==============================================================================
FilePropertyStorage::FilePropertyStorage(juce::File settingsDirectory, std::unique_ptr<CacheFolder> cacheFolder)
    : directory(std::move(settingsDirectory)), cache(std::move(cacheFolder))
{
    directory.createDirectory();
    load();
}

FilePropertyStorage::~FilePropertyStorage()
{
    stopTimer();
    if (dirty)
        writeNow();
}

juce::File FilePropertyStorage::getAppCacheFolder()
{
    return cache->getFolder();
}

juce::File FilePropertyStorage::getAppPrefsFolder()
{
    directory.createDirectory();
    return directory;
}

void FilePropertyStorage::load()
{
    const auto file = settingsFile();
    if (!file.existsAsFile())
        return;

    const auto xml = juce::parseXML(file);
    if (xml == nullptr || !xml->hasTagName(kRootTag))
    {
        // Unreadable (e.g. truncated by a crash of an older version): start with defaults, but keep the file as
        // evidence instead of overwriting it with the next write.
        const auto copy = file.getSiblingFile(file.getFileName() + ".corrupt-" +
                                              juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S"))
                              .getNonexistentSibling(false);
        if (file.moveFileTo(copy))
            juce::Logger::writeToLog("Tracklab: unreadable settings kept as " + pathForLog(copy));
        else
            juce::Logger::writeToLog("Tracklab: unreadable settings file " + pathForLog(file));
        return;
    }

    const juce::ScopedLock guard(lock);
    for (const auto* element : xml->getChildWithTagNameIterator(kValueTag))
    {
        juce::String name;
        juce::var value;
        if (decodeValue(*element, name, value))
            values[name] = value;
    }
}

bool FilePropertyStorage::writeNow()
{
    juce::XmlElement root(kRootTag);
    {
        const juce::ScopedLock guard(lock);
        // Cleared with the snapshot: a change made while the file is written marks the storage dirty again.
        dirty = false;
        for (const auto& [name, value] : values)
            root.addChildElement(encodeValue(name, value).release());
    }

    if (writeFileAtomically(settingsFile(), root.toString()))
        return true;

    dirty = true;  // tried again by the next change, flush or destruction
    return false;
}

void FilePropertyStorage::flushSettingsToDisk()
{
    stopTimer();
    writeNow();
}

void FilePropertyStorage::valuesChanged()
{
    dirty = true;
    // juce::Timer is for the message thread only. A change from another thread is written by the next timer start,
    // flush or at destruction at the latest.
    if (juce::MessageManager::existsAndIsCurrentThread() && !isTimerRunning())
        startTimer(kWriteDelayMs);
}

void FilePropertyStorage::timerCallback()
{
    stopTimer();
    if (dirty)
        writeNow();
}

}  // namespace tracklab::engine::detail
