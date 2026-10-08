// Cache folder of the Tracklab engine (internal to the engine module, see engine_factory.h for the contract).
//
// Tracktion keeps throw-away files (thumbnails, file-mapping data) in PropertyStorage::getAppCacheFolder(). The app
// keeps them in a permanent folder; CLI and tests use a private folder that is deleted with the engine. A private
// folder of a crashed engine is found and deleted by the next engine start (removeOrphanedPrivateCaches()).
#pragma once

#include <tracktion_engine/tracktion_engine.h>

#include <memory>

namespace tracklab::engine::detail
{

/** Every private cache folder has this prefix; the cleanup touches nothing else. */
inline constexpr const char* kPrivateCachePrefix = "tracklab-engine-cache";

class CacheFolder
{
public:
    /** A permanent folder that is created when missing (with its parents) and never deleted. */
    static std::unique_ptr<CacheFolder> makePersistent(const juce::File& folder);

    /** A new folder `<base>/tracklab-engine-cache<random>` (mode 0700 on Linux) that is deleted again by the
        destructor. `base` is created when missing; use resolvePrivateBase() to get a safe one.
        While the object lives, the folder is marked as in use: a juce::InterProcessLock named after the folder is
        held, the operating system releases it when the process dies. Another engine start can therefore tell a
        folder of a running engine from one left behind by a crash, however old the folder looks. */
    static std::unique_ptr<CacheFolder> makePrivate(const juce::File& base);

    ~CacheFolder();

    CacheFolder(const CacheFolder&) = delete;
    CacheFolder& operator=(const CacheFolder&) = delete;

    /** The folder; created again if somebody removed it meanwhile. */
    [[nodiscard]] juce::File getFolder() const;

private:
    struct InUseMarker;

    CacheFolder(juce::File folderToUse, std::unique_ptr<InUseMarker> marker);

    juce::File folder;
    std::unique_ptr<InUseMarker> inUse;  // null for a persistent folder
};

/** Where private cache folders go, and whether orphans may be cleaned up there. */
struct PrivateBase
{
    juce::File folder;
    bool cleanable = false;
};

/** `configured` (EngineOptions::tempDirectory) as it is, if not empty. Otherwise a folder of this user only:
    - Linux: `$XDG_RUNTIME_DIR/tracklab` (only if the variable is set, absolute, and the folder belongs to the user),
      else `<defaultCacheDirectory()>/private`; created with mode 0700, and it must be a real directory that belongs
      to the user. No other user can then plant or swap anything in it.
    - Windows: the temp folder (already per user).
    If no such folder is usable, the system temp folder with `cleanable == false`: the engine still gets a fresh
    0700 folder there, but nothing is cleaned up in a folder shared with other users. */
PrivateBase resolvePrivateBase(const juce::File& configured);

/** The file JUCE's InterProcessLock uses for a private cache folder name (Linux); an empty File where there is none
    (Windows: a mutex). */
juce::File lockFileFor(const juce::String& folderName);

/** Deletes every directory `tracklab-engine-cache*` directly in `base` that no running engine uses (left behind by a
    crash), regardless of its age. Folders of engines in this process and in other processes are kept. Other entries
    (also the `tracklab-engine*` scratch folders of the in-memory storage) are never touched, and so is every
    candidate that is a symbolic link, not a directory, or not owned by the user. Symbolic links are never followed
    when deleting. */
void removeOrphanedPrivateCaches(const juce::File& base);

}  // namespace tracklab::engine::detail
