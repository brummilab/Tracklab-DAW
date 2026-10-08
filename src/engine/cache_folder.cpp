#include "engine/cache_folder.h"

#include "engine/engine_factory.h"
#include "engine/settings_storage.h"

#include <mutex>
#include <set>

#if JUCE_LINUX || JUCE_BSD
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#define TRACKLAB_POSIX_FILES 1
#else
#define TRACKLAB_POSIX_FILES 0
#endif

namespace tracklab::engine::detail
{

namespace
{

/** Names of the private cache folders of the engines alive in this process.
    POSIX record locks belong to the process, not to the file descriptor: a second InterProcessLock on the same file
    in this process would succeed and, when closed, would release the lock of the running engine as well. So this
    process never probes its own folders with the lock; it knows them from this set. The folder name is the key (the
    path may be written in several ways, e.g. over a symbolic link; the name has 64 random bits). */
class LiveFolders
{
public:
    static LiveFolders& get()
    {
        static LiveFolders instance;
        return instance;
    }

    void add(const juce::String& name)
    {
        const std::scoped_lock guard(mutex);
        names.insert(name);
    }

    void remove(const juce::String& name)
    {
        const std::scoped_lock guard(mutex);
        names.erase(name);
    }

    bool contains(const juce::String& name)
    {
        const std::scoped_lock guard(mutex);
        return names.contains(name);
    }

private:
    std::mutex mutex;
    std::set<juce::String> names;
};

/** What lstat() says about a path: a symbolic link is described as a link, never as its target. */
struct EntryInfo
{
    bool directory = false;
    bool regularFile = false;
    bool symbolicLink = false;
    bool ownedByUser = false;
};

EntryInfo inspect(const juce::File& file)
{
#if TRACKLAB_POSIX_FILES
    struct stat st
    {
    };
    if (::lstat(file.getFullPathName().toRawUTF8(), &st) != 0)
        return {};
    return {S_ISDIR(st.st_mode), S_ISREG(st.st_mode), S_ISLNK(st.st_mode), st.st_uid == ::geteuid()};
#else
    // Windows: the temp folder is per user already, and there are no lock files (named mutex).
    EntryInfo info;
    info.symbolicLink = file.isSymbolicLink();
    info.directory = file.isDirectory() && !info.symbolicLink;
    info.regularFile = file.existsAsFile();
    info.ownedByUser = true;
    return info;
#endif
}

/** The lock name does not start with the folder prefix: on Linux JUCE puts the lock file into /var/tmp or /tmp,
    and that must never look like a cache folder to the cleanup. */
juce::String lockNameFor(const juce::String& folderName)
{
    return "tracklab-lock-" + folderName;
}

/** Creates one directory with mode 0700 and reports whether this call made it (false: exists already, or failed).
    The parent has to exist. */
bool createFreshPrivateDirectory(const juce::File& folder)
{
#if TRACKLAB_POSIX_FILES
    return ::mkdir(folder.getFullPathName().toRawUTF8(), 0700) == 0;
#else
    return !folder.exists() && folder.createDirectory().wasOk();
#endif
}

/** A real directory (no link) of this user with mode 0700, created if missing. */
bool ensureOwnPrivateDirectory(const juce::File& folder)
{
#if TRACKLAB_POSIX_FILES
    if (!folder.getParentDirectory().createDirectory())
        return false;
    if (::mkdir(folder.getFullPathName().toRawUTF8(), 0700) != 0 && errno != EEXIST)
        return false;

    // Judged after creation: whoever made the folder first, it has to be ours now.
    const auto info = inspect(folder);
    if (!info.directory || info.symbolicLink || !info.ownedByUser)
        return false;
    return ::chmod(folder.getFullPathName().toRawUTF8(), 0700) == 0;
#else
    return folder.createDirectory().wasOk();
#endif
}

/** Whether the lock file of a lock that InterProcessLock::enter() reported as taken is really ours.
    JUCE tests `handle != 0` after open(), but a failed open() returns -1: when the lock file cannot be opened (it
    belongs to another user, or it could not be created), enter() still says "taken". A file that exists, is a regular
    file and belongs to this user is the only case in which the lock means something. */
bool lockFileIsOurs([[maybe_unused]] const juce::String& folderName)
{
#if TRACKLAB_POSIX_FILES
    const auto info = inspect(lockFileFor(folderName));
    return info.regularFile && !info.symbolicLink && info.ownedByUser;
#else
    return true;
#endif
}

/** JUCE leaves the lock file of an InterProcessLock behind (Linux: an empty file; Windows uses a mutex without a
    file). Without this, every engine run would leave one. */
void removeLockFile([[maybe_unused]] const juce::String& folderName)
{
#if TRACKLAB_POSIX_FILES
    if (lockFileIsOurs(folderName))
        lockFileFor(folderName).deleteFile();
#endif
}

}  // namespace

//==============================================================================
juce::File lockFileFor([[maybe_unused]] const juce::String& folderName)
{
#if TRACKLAB_POSIX_FILES
    // The same choice as juce_SharedCode_posix.h makes for its lock files.
    juce::File lockFolder("/var/tmp");
    if (!lockFolder.isDirectory())
        lockFolder = juce::File("/tmp");
    return lockFolder.getChildFile(lockNameFor(folderName));
#else
    return {};
#endif
}

PrivateBase resolvePrivateBase(const juce::File& configured)
{
    if (configured != juce::File())
        return {configured, true};

#if TRACKLAB_POSIX_FILES
    const auto runtime = juce::SystemStats::getEnvironmentVariable("XDG_RUNTIME_DIR", {});
    if (runtime.isNotEmpty() && juce::File::isAbsolutePath(runtime))
    {
        const juce::File runtimeFolder(runtime);
        if (const auto info = inspect(runtimeFolder); info.directory && info.ownedByUser)
            if (const auto folder = runtimeFolder.getChildFile("tracklab"); ensureOwnPrivateDirectory(folder))
                return {folder, true};
    }

    if (const auto folder = defaultCacheDirectory().getChildFile("private"); ensureOwnPrivateDirectory(folder))
        return {folder, true};

    juce::Logger::writeToLog("Tracklab: no private folder of this user for caches; using the shared temp folder, "
                             "folders left by a crash are not cleaned up there");
    return {juce::File::getSpecialLocation(juce::File::tempDirectory), false};
#else
    return {juce::File::getSpecialLocation(juce::File::tempDirectory), true};
#endif
}

//==============================================================================
struct CacheFolder::InUseMarker
{
    explicit InUseMarker(const juce::String& folderNameToMark)
        : name(folderNameToMark), lock(lockNameFor(folderNameToMark))
    {
        LiveFolders::get().add(name);
        if (lock.enter(0))
        {
            held = true;
            if (!lockFileIsOurs(name))
            {
                // Taken only on paper (see lockFileIsOurs): this folder is not protected against a cleanup.
                lock.exit();
                held = false;
            }
        }
        if (!held)
            juce::Logger::writeToLog("Tracklab: cannot mark the cache folder " + name +
                                     " as in use; after a crash it stays until it is removed by hand");
    }

    // Runs after the folder was deleted: a probe that still sees the folder either finds the lock held (kept) or
    // has nothing left to delete.
    ~InUseMarker()
    {
        if (held)
            lock.exit();
        removeLockFile(name);
        LiveFolders::get().remove(name);
    }

    InUseMarker(const InUseMarker&) = delete;
    InUseMarker& operator=(const InUseMarker&) = delete;

    juce::String name;
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
    // Symbolic links are removed, never followed (the default of deleteRecursively, stated for the reader).
    if (inUse != nullptr)
        folder.deleteRecursively(false);
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
    if (!base.createDirectory())
        juce::Logger::writeToLog("Tracklab: cannot create the folder " + pathForLog(base));

    // 64 random bits: a taken name is practically impossible, but cheap to skip.
    constexpr int kAttempts = 16;
    for (int attempt = 0; attempt < kAttempts; ++attempt)
    {
        const auto random = juce::String::toHexString(juce::Random::getSystemRandom().nextInt64());
        const auto name = juce::String(kPrivateCachePrefix) + random;
        const auto folder = base.getChildFile(name);

        // Marked before the folder exists, so that a cleanup in another process never sees it unmarked.
        auto marker = std::make_unique<InUseMarker>(name);
        if (createFreshPrivateDirectory(folder))
            return std::unique_ptr<CacheFolder>(new CacheFolder(folder, std::move(marker)));
    }

    // Not creatable (no permission, full disk): the engine still starts, as with a missing settings folder.
    juce::Logger::writeToLog("Tracklab: cannot create a private cache folder in " + pathForLog(base));
    return std::unique_ptr<CacheFolder>(
        new CacheFolder(base.getChildFile(juce::String(kPrivateCachePrefix) + "-unusable"), nullptr));
}

juce::File CacheFolder::getFolder() const
{
    folder.createDirectory();
    return folder;
}

//==============================================================================
void removeOrphanedPrivateCaches(const juce::File& base)
{
    const auto candidates =
        base.findChildFiles(juce::File::findDirectories, false, juce::String(kPrivateCachePrefix) + "*");
    for (const auto& candidate : candidates)
    {
        const auto name = candidate.getFileName();
        if (LiveFolders::get().contains(name))
            continue;

        // Only a real directory of this user. Anybody else's folder, or a link, may be a trap: it is not ours to
        // delete, and a link would lead the deletion somewhere else.
        const auto info = inspect(candidate);
        if (!info.directory || info.symbolicLink || !info.ownedByUser)
            continue;

        // Held by an engine of another process: in use. Free: the owner is gone, whatever the age of the folder.
        juce::InterProcessLock lock(lockNameFor(name));
        if (!lock.enter(0))
            continue;
        if (!lockFileIsOurs(name))
        {
            lock.exit();
            continue;
        }

        if (candidate.deleteRecursively(false))
            juce::Logger::writeToLog("Tracklab: deleted the cache folder " + pathForLog(candidate) +
                                     " of an engine that did not end properly");
        lock.exit();
        removeLockFile(name);
    }
}

}  // namespace tracklab::engine::detail
