#include "engine/cache_folder.h"

#include "engine/settings_storage.h"

#include <mutex>
#include <set>

namespace tracklab::engine::detail
{

namespace
{

/** Private cache folders of the engines alive in this process (full paths).
    POSIX record locks belong to the process, not to the file descriptor: a second InterProcessLock on the same file
    in this process would succeed and, when closed, would release the lock of the running engine as well. So this
    process never probes its own folders with the lock; it knows them from this set. */
class LiveFolders
{
public:
    static LiveFolders& get()
    {
        static LiveFolders instance;
        return instance;
    }

    void add(const juce::String& path)
    {
        const std::scoped_lock guard(mutex);
        paths.insert(path);
    }

    void remove(const juce::String& path)
    {
        const std::scoped_lock guard(mutex);
        paths.erase(path);
    }

    bool contains(const juce::String& path)
    {
        const std::scoped_lock guard(mutex);
        return paths.contains(path);
    }

private:
    std::mutex mutex;
    std::set<juce::String> paths;
};

/** The lock name does not start with the folder prefix: on Linux JUCE puts the lock file into /var/tmp or /tmp,
    and that must never look like a cache folder to the cleanup. */
juce::String lockNameFor(const juce::String& folderName)
{
    return "tracklab-lock-" + folderName;
}

/** JUCE leaves the lock file of an InterProcessLock behind (Linux: an empty file in /var/tmp or /tmp, same choice as
    in juce_SharedCode_posix.h; Windows uses a mutex without a file). Without this, every engine run would leave one. */
void removeLockFile([[maybe_unused]] const juce::String& lockName)
{
#if JUCE_LINUX || JUCE_BSD
    juce::File lockFolder("/var/tmp");
    if (!lockFolder.isDirectory())
        lockFolder = juce::File("/tmp");
    lockFolder.getChildFile(lockName).deleteFile();
#endif
}

juce::File chooseBase(const juce::File& base)
{
    return base == juce::File() ? juce::File::getSpecialLocation(juce::File::tempDirectory) : base;
}

}  // namespace

//==============================================================================
struct CacheFolder::InUseMarker
{
    InUseMarker(const juce::File& folderToMark, const juce::String& lockNameToUse)
        : path(folderToMark.getFullPathName()), lockName(lockNameToUse), lock(lockNameToUse)
    {
        LiveFolders::get().add(path);
        held = lock.enter(0);
        if (!held)
            juce::Logger::writeToLog("Tracklab: cannot mark the cache folder " + pathForLog(folderToMark) +
                                     " as in use; after a crash it stays until it is removed by hand");
    }

    // Runs after the folder was deleted: a probe that still sees the folder either finds the lock held (kept) or
    // has nothing left to delete.
    ~InUseMarker()
    {
        if (held)
            lock.exit();
        removeLockFile(lockName);
        LiveFolders::get().remove(path);
    }

    InUseMarker(const InUseMarker&) = delete;
    InUseMarker& operator=(const InUseMarker&) = delete;

    juce::String path;
    juce::String lockName;
    juce::InterProcessLock lock;
    bool held = false;
};

//==============================================================================
CacheFolder::CacheFolder(juce::File folderToUse, std::unique_ptr<InUseMarker> marker)
    : folder(std::move(folderToUse)), inUse(std::move(marker))
{
}

CacheFolder::~CacheFolder()
{
    if (inUse != nullptr)
        folder.deleteRecursively();
    // `inUse` is destroyed after this body, i.e. after the folder was deleted.
}

std::unique_ptr<CacheFolder> CacheFolder::makePersistent(const juce::File& folder)
{
    if (!folder.createDirectory())
        juce::Logger::writeToLog("Tracklab: cannot create the cache folder " + pathForLog(folder));
    return std::unique_ptr<CacheFolder>(new CacheFolder(folder, nullptr));
}

std::unique_ptr<CacheFolder> CacheFolder::makePrivate(const juce::File& base)
{
    const auto parent = chooseBase(base);
    if (!parent.createDirectory())
        juce::Logger::writeToLog("Tracklab: cannot create the folder " + pathForLog(parent));

    // 64 random bits: a taken name is practically impossible, but cheap to skip.
    juce::File folder;
    do
    {
        const auto random = juce::String::toHexString(juce::Random::getSystemRandom().nextInt64());
        folder = parent.getChildFile(juce::String(kPrivateCachePrefix) + random);
    } while (folder.exists());

    // Marked before the folder exists, so that a cleanup never sees it unmarked.
    auto marker = std::make_unique<InUseMarker>(folder, lockNameFor(folder.getFileName()));
    if (!folder.createDirectory())
        juce::Logger::writeToLog("Tracklab: cannot create the cache folder " + pathForLog(folder));
    return std::unique_ptr<CacheFolder>(new CacheFolder(folder, std::move(marker)));
}

juce::File CacheFolder::getFolder() const
{
    folder.createDirectory();
    return folder;
}

//==============================================================================
void removeOrphanedPrivateCaches(const juce::File& base)
{
    const auto parent = chooseBase(base);
    const auto candidates =
        parent.findChildFiles(juce::File::findDirectories, false, juce::String(kPrivateCachePrefix) + "*");
    for (const auto& candidate : candidates)
    {
        if (LiveFolders::get().contains(candidate.getFullPathName()))
            continue;

        // Held by an engine of another process: in use. Free: the owner is gone, whatever the age of the folder.
        const auto lockName = lockNameFor(candidate.getFileName());
        juce::InterProcessLock lock(lockName);
        if (!lock.enter(0))
            continue;

        if (candidate.deleteRecursively())
            juce::Logger::writeToLog("Tracklab: deleted the cache folder " + pathForLog(candidate) +
                                     " of an engine that did not end properly");
        lock.exit();
        removeLockFile(lockName);
    }
}

}  // namespace tracklab::engine::detail
