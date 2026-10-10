#include "project/project_session.h"

#include "core/undo_levels.h"

#include <algorithm>
#include <climits>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#if JUCE_LINUX || JUCE_MAC
#include <fcntl.h>
#include <unistd.h>
#endif

namespace tracklab::project
{

namespace
{

/** How long after a save / open the "modified" flag is looked at once more, see ProjectSession::Impl::SettleTimer.
    Tracktion's PluginChangeTimer marks the Edit changed 500 ms after the last plugin change; 650 leaves some slack. */
constexpr int settleDelayMs = 650;

/** Longest project name in UTF-8 bytes. The name is the base of derived file names: `<name>.<YYYYMMDD-HHMMSS>.tracklab`
    (backup, +24), `<name>.tracklab.autosave` (+18) and `<name>.tracklab_temp<8 hex>.autosave` (+27); file systems allow
    255 bytes per name, so 120 leaves a wide margin. */
constexpr int maxProjectNameBytes = 120;

constexpr const char* autosaveExtension = ".autosave";

[[noreturn]] void fail(std::string_view code, const std::string& message)
{
    throw core::CommandFailure(code, message);
}

std::string quoted(const juce::File& file)
{
    return "\"" + file.getFullPathName().toStdString() + "\"";
}

/** Why `name` cannot be a project name, empty if it can. The project name is used as folder name and as file name, and
    the same project has to open on Windows and Linux, so the rules are those of Windows (the stricter ones): none of
    `<>:"/\|?*`, no control characters (0-31), no blanks at the ends, no dot at the end, none of the reserved device
    names (CON, PRN, AUX, NUL, COM1-9, LPT1-9, in any case, also with an extension such as "nul.txt"). Everything
    else is allowed: umlauts, spaces inside, and `#@,;&`. */
std::string nameProblem(const juce::String& name)
{
    if (name.isEmpty())
        return "it is empty";
    if (name == "." || name == "..")
        return "it is a folder reference, not a name";
    if (name.getNumBytesAsUTF8() > static_cast<size_t>(maxProjectNameBytes))
        return "it is longer than " + std::to_string(maxProjectNameBytes) + " bytes (UTF-8)";
    for (const auto character : name)
    {
        if (character < 32)
            return "it contains a control character";
        if (juce::String("<>:\"/\\|?*").containsChar(character))
            return "it contains the character '" + juce::String::charToString(character).toStdString() +
                   "', which is not allowed in file names";
    }
    if (name != name.trim())
        return "it starts or ends with a blank";
    if (name.endsWithChar('.'))
        return "it ends with a dot";

    // Windows reserves the device names also with an extension: "NUL.txt" is the device, too.
    const auto stem = name.upToFirstOccurrenceOf(".", false, false).toUpperCase();
    for (const char* reserved : {"CON", "PRN", "AUX", "NUL"})
        if (stem == reserved)
            return "\"" + stem.toStdString() + "\" is a reserved name on Windows";
    for (const char* device : {"COM", "LPT"})
        if (stem.length() == 4 && stem.startsWith(device) && stem[3] >= '1' && stem[3] <= '9')
            return "\"" + stem.toStdString() + "\" is a reserved name on Windows";
    return {};
}

void requirePlainName(const juce::String& name)
{
    if (const auto problem = nameProblem(name); !problem.empty())
        fail(error_code::invalidProjectName, "\"" + name.toStdString() + "\" is not a valid project name: " + problem);
}

void requireParentFolder(const juce::File& folder)
{
    if (!folder.isDirectory())
        fail(error_code::folderNotFound,
             "The folder " + quoted(folder) + " does not exist; it is not created automatically");
}

/** Length in UTF-16 code units, which is what Windows' MAX_PATH counts (a character outside the BMP is two). */
std::size_t utf16Length(const juce::String& text)
{
    std::size_t length = 0;
    for (const auto character : text)
        length += character > 0xFFFF ? 2 : 1;
    return length;
}

/** Windows opens paths of at most 259 characters (long path support is off by default), and the same project has to
    work there, so every platform refuses what would not: the longest file that belongs to the project (a backup with
    counter and temporary suffix, see longestDerivedPathLength) has to fit. Checked before anything is created. */
void requirePathLength(const juce::File& parentFolder, const juce::String& name)
{
    const auto length = longestDerivedPathLength(utf16Length(parentFolder.getFullPathName()), utf16Length(name));
    if (length > static_cast<std::size_t>(maxPathLength))
        fail(error_code::pathTooLong, "The project would need paths of up to " + std::to_string(length) +
                                          " characters, more than the " + std::to_string(maxPathLength) +
                                          " that Windows opens; use a shorter project name or a shorter folder (" +
                                          quoted(parentFolder) + ")");
}

juce::File projectFileIn(const juce::File& projectFolder, const juce::String& name)
{
    return projectFolder.getChildFile(name + fileExtension);
}

juce::File autosaveFileOf(const juce::File& projectFile)
{
    return projectFile.getSiblingFile(projectFile.getFileName() + autosaveExtension);
}

juce::File backupsFolderOf(const juce::File& projectFile)
{
    return projectFile.getParentDirectory().getChildFile("Backups");
}

/** The stamp of a backup name, `<YYYYMMDD-HHMMSS>` in local time of the session's clock. */
juce::String backupStamp(const juce::Time& time)
{
    return time.formatted("%Y%m%d-%H%M%S");
}

/** What a backup file name says: the time and the counter (1 = no suffix). */
struct BackupName
{
    juce::Time time;
    int counter = 1;
};

/** The parts of the name of a backup of project `projectName`; empty if `fileName` is not such a name (other projects'
    backups and foreign files in Backups/ are none of our business: they are neither listed, counted nor deleted). */
std::optional<BackupName> parseBackupName(const juce::String& projectName, const juce::String& fileName)
{
    const auto prefix = projectName + ".";
    const juce::String suffix(fileExtension);
    if (!fileName.startsWith(prefix) || !fileName.endsWith(suffix) ||
        fileName.length() < prefix.length() + 15 + suffix.length())
        return std::nullopt;
    const auto middle = fileName.substring(prefix.length(), fileName.length() - suffix.length());
    const auto stamp = middle.substring(0, 15);
    const auto counterText = middle.substring(15);
    if (stamp.length() != 15 || stamp[8] != '-')
        return std::nullopt;
    for (int i = 0; i < 15; ++i)
        if (i != 8 && !juce::CharacterFunctions::isDigit(stamp[i]))
            return std::nullopt;

    int counter = 1;
    if (counterText.isNotEmpty())
    {
        if (!counterText.startsWithChar('-') || counterText.length() < 2 || counterText.length() > 3 ||
            !counterText.substring(1).containsOnly("0123456789"))
            return std::nullopt;
        counter = counterText.substring(1).getIntValue();
        if (counter < 2)
            return std::nullopt;
    }
    return BackupName{juce::Time(stamp.substring(0, 4).getIntValue(), stamp.substring(4, 6).getIntValue() - 1,
                                 stamp.substring(6, 8).getIntValue(), stamp.substring(9, 11).getIntValue(),
                                 stamp.substring(11, 13).getIntValue(), stamp.substring(13, 15).getIntValue(), 0, true),
                      counter};
}

/** A backup file of project `projectName` for `time` that is newer than every backup that exists: `<name>.<stamp>.tracklab`,
    and for further backups in the same second `<name>.<stamp>-2.tracklab`, `-3`, ... (one more than the highest counter
    of that second, so that the order stays right when the rotation has removed the lower ones). Never an overwrite: a
    backup is a version somebody may want. At maxBackupCounter the last name is reused. */
juce::File newBackupFile(const juce::File& backupsFolder, const juce::String& projectName, const juce::Time& time)
{
    const auto base = projectName + "." + backupStamp(time);
    int highest = 0;
    for (const auto& file : backupsFolder.findChildFiles(juce::File::findFiles, false))
        if (file.getFileName().startsWith(base))
            if (const auto parsed = parseBackupName(projectName, file.getFileName()))
                highest = std::max(highest, parsed->counter);
    if (highest == 0)
        return backupsFolder.getChildFile(base + fileExtension);
    return backupsFolder.getChildFile(base + "-" + juce::String(std::min(highest + 1, maxBackupCounter)) +
                                      fileExtension);
}

/** The backups of the project in `backupsFolder`, newest first (by the time in the name, then the counter). */
std::vector<BackupInfo> collectBackups(const juce::File& backupsFolder, const juce::String& projectName)
{
    struct Entry
    {
        BackupInfo info;
        int counter = 1;
    };
    std::vector<Entry> entries;
    for (const auto& file : backupsFolder.findChildFiles(juce::File::findFiles, false))
        if (const auto parsed = parseBackupName(projectName, file.getFileName()))
            entries.push_back({BackupInfo{file.getFileName(), parsed->time, file.getSize()}, parsed->counter});
    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b)
              {
                  if (a.info.time != b.info.time)
                      return a.info.time > b.info.time;
                  return a.counter > b.counter;
              });
    std::vector<BackupInfo> backups;
    backups.reserve(entries.size());
    for (const auto& entry : entries)
        backups.push_back(entry.info);
    return backups;
}

/** Deletes the oldest backups of the project beyond `keep`, but never `protectedName` (the backup that was just chosen
    for a restore: it has to survive the safety backup that is made first). A count <= 0 means "backups are off":
    nothing is deleted. */
void rotateBackups(const juce::File& backupsFolder, const juce::String& projectName, int keep,
                   const juce::String& protectedName = {})
{
    if (keep <= 0)
        return;
    const auto backups = collectBackups(backupsFolder, projectName);
    auto remaining = backups.size();
    for (auto it = backups.rbegin(); it != backups.rend() && remaining > static_cast<size_t>(keep); ++it)
    {
        if (it->name == protectedName)
            continue;
        backupsFolder.getChildFile(it->name).deleteFile();
        --remaining;
    }
}

/** Makes a rename durable: the directory entry is flushed (POSIX; Windows' ReplaceFileW is durable by itself). */
void syncFolder([[maybe_unused]] const juce::File& folder)
{
#if JUCE_LINUX || JUCE_MAC
    // NOLINTNEXTLINE(hicpp-vararg,cppcoreguidelines-pro-type-vararg): POSIX open() has no other signature
    const int descriptor = ::open(folder.getFullPathName().toRawUTF8(), O_RDONLY | O_DIRECTORY);
    if (descriptor >= 0)
    {
        ::fsync(descriptor);
        ::close(descriptor);
    }
#endif
}

/** `chars` are all hex digits and there is at least one. */
bool isHex(const juce::String& chars)
{
    return chars.isNotEmpty() && chars.containsOnly("0123456789abcdefABCDEF");
}

/** Removes what an interrupted atomic write left in the project folder: juce::TemporaryFile names its files
    `<stem>_temp<hex><extension>`, i.e. `<name>_temp<hex>.tracklab` for the project file and
    `<name>.tracklab_temp<hex>.autosave` for the autosave. Only files with this project's names are touched. */
void removeInterruptedWrites(const juce::File& projectFile)
{
    const auto isLeftover = [](const juce::String& fileName, const juce::String& stem, const juce::String& extension)
    {
        const auto prefix = stem + "_temp";
        return fileName.startsWith(prefix) && fileName.endsWith(extension) &&
               fileName.length() > prefix.length() + extension.length() &&
               isHex(fileName.substring(prefix.length(), fileName.length() - extension.length()));
    };
    for (const auto& file : projectFile.getParentDirectory().findChildFiles(juce::File::findFiles, false))
    {
        const auto fileName = file.getFileName();
        if (isLeftover(fileName, projectFile.getFileNameWithoutExtension(), fileExtension) ||
            isLeftover(fileName, projectFile.getFileName(), autosaveExtension))
            file.deleteFile();
    }

    // The same in Backups/: the safety backup of a restore is written like a project file, so its temporary file is
    // `<name>.<stamp>[-N]_temp<hex>.tracklab`. Only that shape is removed: the part before `_temp` has to be the name of
    // a backup of this project, so that nothing else in that folder is ever touched.
    const auto name = projectFile.getFileNameWithoutExtension();
    const juce::String extension(fileExtension);
    for (const auto& file : backupsFolderOf(projectFile).findChildFiles(juce::File::findFiles, false))
    {
        const auto fileName = file.getFileName();
        const auto marker = fileName.lastIndexOf("_temp");
        if (marker < 0 || !fileName.endsWith(extension))
            continue;
        const auto hex = fileName.substring(marker + 5, fileName.length() - extension.length());
        if (isHex(hex) && parseBackupName(name, fileName.substring(0, marker) + extension).has_value())
            file.deleteFile();
    }
}

/** What to add to the error of a project file that cannot be read: the newest of the backups and the autosave (E41), so
    that the user knows where the work is. The autosave counts as newer than a backup when its modification time is
    later than the backup's time in its name. */
std::string recoveryHint(const juce::File& projectFile)
{
    const auto backups = collectBackups(backupsFolderOf(projectFile), projectFile.getFileNameWithoutExtension());
    const auto autosave = autosaveFileOf(projectFile);
    if (autosave.existsAsFile() && (backups.empty() || autosave.getLastModificationTime() > backups.front().time))
        return "; the autosave " + quoted(autosave) + " may hold the latest work";
    if (!backups.empty())
        return "; the newest backup is " + quoted(backupsFolderOf(projectFile).getChildFile(backups.front().name));
    return {};
}

/** The copy of the project file as it is before a save, made right before the save replaces it (so that a save that
    fails leaves no backup: takeBack removes what was made). Best effort: a backup that cannot be made does not stop the
    save. */
struct BackupCopy
{
    juce::File source;  ///< the project file as it is now; a missing file (or none) means "no backup"
    juce::File target;

    void make()
    {
        if (!source.existsAsFile() || !target.getParentDirectory().createDirectory().wasOk())
            return;
        const bool existed = target.exists();  // two saves within a second share a name; the later one wins
        juce::TemporaryFile temporary(target);
        if (!source.copyFileTo(temporary.getFile()) || !temporary.overwriteTargetFileWithTemporary())
            return;
        made = !existed;
    }

    void takeBack() const
    {
        if (made)
            target.deleteFile();
    }

    bool made = false;  ///< a new file was created by make()
};

/** Folder and the four sub folders of a project. Remembers whether the project folder is new, so that a failed
    creation can take it back (only what this call created is ever removed). */
struct ProjectFolders
{
    ProjectFolders(const juce::File& folder, const juce::File& file) : projectFolder(folder)
    {
        if (file.exists() || (folder.exists() && !folder.isDirectory()))
            fail(error_code::projectExists, "A project " + quoted(file) + " exists already");
    }

    void create()
    {
        createdProjectFolder = !projectFolder.exists();
        if (const auto result = projectFolder.createDirectory(); result.failed())
            fail(error_code::saveFailed, "Cannot create the project folder " + quoted(projectFolder) + ": " +
                                             result.getErrorMessage().toStdString());
        for (const char* name : subFolderNames)
            if (const auto result = projectFolder.getChildFile(name).createDirectory(); result.failed())
                fail(error_code::saveFailed, "Cannot create the folder " + quoted(projectFolder.getChildFile(name)) +
                                                 ": " + result.getErrorMessage().toStdString());
    }

    void takeBack() const
    {
        if (createdProjectFolder)
            projectFolder.deleteRecursively();
    }

    juce::File projectFolder;
    bool createdProjectFolder = false;
};

/** Writes the state of `edit` to `target` so that `target` is either the old file or the complete new one, never
    something in between: a temporary file next to it is written and flushed (fsync), and only then replaces `target`
    (rename on Linux, ReplaceFileW on Windows). Everything that goes wrong is save_failed; the temporary file is
    removed by juce::TemporaryFile's destructor in every case. `backup`, if given, is made after the hook has agreed and
    right before the replace. */
void writeProjectFile(te::Edit& edit, const juce::File& target, const ProjectSession::BeforeReplaceHook& hook,
                      BackupCopy* backup = nullptr)
{
    // The plugin states go into the tree first, then the version: the file must say which format it is in.
    edit.flushState();
    edit.state.setProperty(formatVersionProperty, currentFormatVersion, nullptr);

    const auto xml = edit.state.createXml();
    if (xml == nullptr)
        fail(error_code::saveFailed, "The project state cannot be converted to XML");

    juce::TemporaryFile temporary(target);
    {
        juce::FileOutputStream stream(temporary.getFile());
        if (stream.failedToOpen())
            fail(error_code::saveFailed,
                 "Cannot write next to " + quoted(target) + ": " + stream.getStatus().getErrorMessage().toStdString());

        juce::XmlElement::TextFormat format;
        format.newLineChars = "\n";  // the same file on Windows and Linux
        xml->writeTo(stream, format);
        stream.flush();  // fsync: the bytes are on disk before the old file is replaced
        if (stream.getStatus().failed())
            fail(error_code::saveFailed, "Cannot write " + quoted(temporary.getFile()) + ": " +
                                             stream.getStatus().getErrorMessage().toStdString());
    }  // closed before the replace (Windows cannot replace an open file)

    if (hook)
    {
        bool carryOn = false;
        try
        {
            carryOn = hook(temporary.getFile(), target);
        }
        catch (...)
        {
            carryOn = false;
        }
        if (!carryOn)
            fail(error_code::saveFailed, "The save was aborted before " + quoted(target) + " was replaced");
    }

    if (backup != nullptr)
        backup->make();
    if (!temporary.overwriteTargetFileWithTemporary())
    {
        if (backup != nullptr)
            backup->takeBack();
        fail(error_code::saveFailed, "Cannot replace " + quoted(target) + " by the saved project");
    }
    syncFolder(target.getParentDirectory());
}

/** The project state of a file (project, autosave or backup) as a tree, migrated in memory to currentFormatVersion.
    `what` names the kind of file in the messages, `hint` is appended to a corrupt_project message. Fails with
    corrupt_project, project_too_new or migration_failed. Nothing is written. */
juce::ValueTree readState(const juce::File& file, const std::string& what, const std::string& hint)
{
    const auto corrupt = [&](const std::string& reason)
    {
        fail(error_code::corruptProject, "The " + what + " " + quoted(file) +
                                             " cannot be read as a Tracklab project (" + reason +
                                             "); the file was not changed" + hint);
    };

    const auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr)
        corrupt("not valid XML or empty");
    if (!xml->hasTagName("EDIT"))
        corrupt("the root element is not EDIT");
    auto state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid())
        corrupt("no project state");

    // In memory only: the file on disk is rewritten by the next save, never by reading it.
    if (const auto outcome = migrateState(state, builtInMigrationSteps(), currentFormatVersion); !outcome.ok)
        fail(outcome.code, "The " + what + " " + quoted(file) + ": " + outcome.message);
    state.setProperty(te::IDs::alwaysUseRelativePaths, true, nullptr);
    return state;
}

/** Do two states have the same content? Tracktion re-sorts the child nodes of the EDIT node (tracks by type) in an
    asynchronous step after structural changes, through the undo manager, and stamps `lastSignificantChange` whenever it
    marks the Edit changed. Neither is a change of the content, so both copies get the same normal form: no
    `lastSignificantChange`, the root's children in the order of TrackList::sortTracksByType. From there on the
    comparison is exact, the order of children counts everywhere (tracks, also an audio against a folder track, the
    plugins of a chain, clips). */
bool sameContent(const juce::ValueTree& a, const juce::ValueTree& b)
{
    auto left = a.createCopy();
    auto right = b.createCopy();
    for (auto* tree : {&left, &right})
    {
        tree->removeProperty(te::IDs::lastSignificantChange, nullptr);
        te::TrackList::sortTracksByType(*tree, nullptr);
    }
    return left.isEquivalentTo(right);
}

/** A clip source (or take source) property that holds a file path. */
struct SourcePathChange
{
    juce::ValueTree node;
    juce::String before;
    juce::String after;
};

void collectSourceNodes(const juce::ValueTree& node, std::vector<juce::ValueTree>& out)
{
    if ((te::Clip::isClipState(node) || node.hasType(te::IDs::TAKE)) && node.hasProperty(te::IDs::source))
        out.push_back(node);
    for (const auto& child : node)
        collectSourceNodes(child, out);
}

/** Save As moves the project file, relative media paths are relative to it. Media are not copied (own card), so every
    path that is stored is re-written relative to the NEW project file (an absolute one where there is no relative
    path, e.g. another drive on Windows): the sources keep being found. Resolved with the old file, so this runs before
    the file reference is switched. Media of a Tracktion project (project item ids) are not paths and stay as they
    are. */
std::vector<SourcePathChange> sourcePathChangesForMove(te::Edit& edit, const juce::File& newProjectFile)
{
    std::vector<juce::ValueTree> nodes;
    collectSourceNodes(edit.state, nodes);

    std::vector<SourcePathChange> changes;
    for (const auto& node : nodes)
    {
        const auto before = node.getProperty(te::IDs::source).toString();
        if (before.isEmpty() || te::ProjectItemRef(before).isProjectItemID())
            continue;
        const auto file = te::SourceFileReference::findFileFromString(edit, before);
        if (file == juce::File())
            continue;
        const auto after = file.getRelativePathFrom(newProjectFile.getParentDirectory());
        if (after != before)
            changes.push_back({node, before, after});
    }
    return changes;
}

/** Written without the undo manager: re-pointing a path is not a step the user took. */
void applySourcePaths(const std::vector<SourcePathChange>& changes, bool forward)
{
    for (const auto& change : changes)
    {
        auto node = change.node;
        node.setProperty(te::IDs::source, forward ? change.after : change.before, nullptr);
    }
}

}  // namespace

//==============================================================================
struct ProjectSession::Impl
{
    Impl(te::Engine& engineToUse, core::EditContext& contextToUse) : engine(engineToUse), context(contextToUse) {}

    ~Impl() { dropEdit(); }

    //==========================================================================
    /** Tracktion's own bookkeeping of "changed" is unreliable right after a project was created, opened or saved, in
        both directions: its EditChangeResetterTimer resets the flag unconditionally 200 ms after creation (a real
        change in that time is forgotten), and the PluginChangeTimer (500 ms after the last plugin change, private, not
        cancellable) and an asynchronous re-sort of the tracks set it again after a save (a project that was just saved
        turns "modified" by itself). So from creation / open / save for `settleDelayMs`, "modified" is decided by
        comparing the content with a snapshot of what was written or read (sameContent, after flushing the plugin
        states into the state). When the time is over, SettleTimer makes Tracktion's flag agree with the comparison once
        more (markAsChanged / resetChangedStatus) and Tracktion's flag is the truth again. */
    struct SettleTimer : private juce::Timer
    {
        explicit SettleTimer(Impl& owner) : impl(owner) {}
        using juce::Timer::startTimer;
        using juce::Timer::stopTimer;

    private:
        void timerCallback() override
        {
            stopTimer();
            impl.endSettling();
        }

        Impl& impl;
    };

    /** The autosave timer (M1-05): a message-thread timer, so that the autosave never runs in parallel with a command. */
    struct AutosaveTimer : private juce::Timer
    {
        explicit AutosaveTimer(Impl& owner) : impl(owner) {}
        using juce::Timer::startTimer;
        using juce::Timer::stopTimer;

    private:
        void timerCallback() override { impl.autosave(); }

        Impl& impl;
    };

    void beginSettling()
    {
        edit->flushState();  // so that the snapshot has what a later flushState would write anyway
        snapshot = edit->state.createCopy();
        settling = true;
        settleTimer.startTimer(settleDelayMs);
    }

    /** Makes Tracktion's flag say what the comparison says. Returns the comparison. */
    bool alignFlagWithContent()
    {
        edit->flushState();  // a real change of a plugin shows in the state only after this
        const bool differs = !sameContent(edit->state, snapshot);
        const bool flagged = edit->hasChangedSinceSaved();
        if (differs && !flagged)
            edit->markAsChanged();
        else if (!differs && flagged)
            edit->resetChangedStatus();
        return differs;
    }

    void endSettling()
    {
        if (edit == nullptr || !settling)
            return;
        edit->getUndoManager().dispatchPendingMessages();
        if (forceModified)
            edit->markAsChanged();
        else
            alignFlagWithContent();
        forceModified = false;
        settling = false;
        snapshot = {};
    }

    //==========================================================================
    /** The change messages of the UndoManager that set the flag are asynchronous: deliver them first, so that a change
        made a moment ago (in the same batch) is not overlooked and a project.close cannot drop it silently. While the
        project is settling the content decides, see above. */
    bool isModified()
    {
        if (edit == nullptr)
            return false;
        edit->getUndoManager().dispatchPendingMessages();
        if (settling)
            return forceModified || alignFlagWithContent();
        return edit->hasChangedSinceSaved();
    }

    void requireNoUnsavedChanges(const char* action)
    {
        if (isModified())
            fail(error_code::unsavedChanges, "The open project " + quoted(*fileRef) +
                                                 " has unsaved changes; save it or close it with discard before you " +
                                                 action);
    }

    te::Edit& requireEdit() const
    {
        if (edit == nullptr)
            fail(core::error_code::noEdit, "no project is open");
        return *edit;
    }

    ProjectInfo infoOf()
    {
        auto& e = requireEdit();
        return ProjectInfo{.file = *fileRef,
                           .name = fileRef->getFileNameWithoutExtension().toStdString(),
                           .formatVersion = formatVersionOf(e.state),
                           .modified = isModified()};
    }

    //==========================================================================
    /** Creates the Edit of `state` that belongs to `file`. The Edit reads the shared file lazily (Save As changes it). */
    std::unique_ptr<te::Edit> makeEdit(const juce::ValueTree& state, const std::shared_ptr<juce::File>& file) const
    {
        auto id = te::ProjectItemID::fromProperty(state, te::IDs::projectID);
        if (!id.isValid())
            id = te::ProjectItemID::createNewID(te::ProjectID{});

        std::unique_ptr<te::Edit> created;
        try
        {
            created = te::Edit::createEdit(
                te::Edit::Options{.engine = engine,
                                  .editState = state,
                                  .editProjectItemID = id,
                                  .numUndoLevelsToStore = core::defaultUndoLevels,
                                  // The Edit finds its media next to the project file; no Project/ProjectManager.
                                  .editFileRetriever = [file] { return *file; },
                                  // An empty project has no tracks; a file that has a tempo track gets none added.
                                  .numAudioTracks = 0,
                                  // Unity gain: a new project does not start with Tracktion's -3 dB of headroom.
                                  .defaultMasterVolumedB = 0.0f});
        }
        catch (...)
        {
            created.reset();
        }
        return created;
    }

    /** A freshly created or opened Edit: empty undo history, not modified (what Tracktion did while setting itself up
        is not a change of the user). */
    static void startClean(te::Edit& e)
    {
        e.getUndoManager().clearUndoHistory();
        e.resetChangedStatus();
    }

    /** The open Edit is closed. The context is cleared first: nothing may reach the Edit through it any more. */
    void dropEdit()
    {
        settleTimer.stopTimer();
        autosaveTimer.stopTimer();
        settling = false;
        forceModified = false;
        recovery.reset();
        autosaved = {};
        context.setEdit(nullptr);
        edit.reset();  // before the engine goes away
        fileRef = std::make_shared<juce::File>();
    }

    /** Makes `fresh` the open project. The previous Edit is destroyed after the context points to the new one.
        `markModified`: the state is not what the project file has (restored from an autosave or a backup), so it counts
        as modified although Tracktion does not know of a change. */
    void install(std::unique_ptr<te::Edit> fresh, std::shared_ptr<juce::File> file, bool markModified = false)
    {
        settleTimer.stopTimer();
        settling = false;
        context.setEdit(fresh.get());
        auto previous = std::move(edit);
        edit = std::move(fresh);
        fileRef = std::move(file);
        previous.reset();
        autosaved = {};
        beginSettling();
        forceModified = markModified;
        restartAutosaveTimer();
    }

    ProjectInfo finishSave(te::Edit& e)
    {
        e.resetChangedStatus();
        beginSettling();
        forceModified = false;
        // The saved file has everything: the autosave is obsolete, and an offered recovery is decided by saving.
        autosaveFileOf(*fileRef).deleteFile();
        recovery.reset();
        autosaved = {};
        return infoOf();
    }

    //==========================================================================
    void restartAutosaveTimer()
    {
        autosaveTimer.stopTimer();
        if (edit != nullptr && autosaveInterval.count() > 0)
            autosaveTimer.startTimer(
                static_cast<int>(std::min<std::chrono::milliseconds::rep>(autosaveInterval.count(), INT_MAX)));
    }

    /** What the autosave timer does: writes `<project>.tracklab.autosave` if the project changed since the last save and
        since the last autosave, through the same atomic write as a save. Never throws. */
    AutosaveResult autosave()
    {
        if (edit == nullptr)
            return AutosaveResult::noProject;
        if (!isModified())
            return AutosaveResult::notModified;
        if (recovery.has_value())
            return AutosaveResult::recoveryPending;  // the autosave of the crash is not decided yet: keep it
        if (edit->isSaveInhibited())
            return AutosaveResult::saveInhibited;

        try
        {
            // Tracktion's flag stays set after an autosave (it is not a save), so what was written is compared.
            edit->flushState();
            if (autosaved.isValid() && sameContent(edit->state, autosaved))
                return AutosaveResult::notModified;
            writeProjectFile(*edit, autosaveFileOf(*fileRef), hook);
            autosaved = edit->state.createCopy();
            return AutosaveResult::written;
        }
        catch (...)
        {
            return AutosaveResult::failed;
        }
    }

    juce::Time now() const { return clock ? clock() : juce::Time::getCurrentTime(); }

    juce::String projectName() const { return fileRef->getFileNameWithoutExtension(); }

    /** The backup of the project file as it is now, named after the clock. Empty source if backups are switched off. */
    BackupCopy backupOfCurrentFile() const
    {
        if (maxBackups <= 0)
            return {};
        return BackupCopy{*fileRef, newBackupFile(backupsFolderOf(*fileRef), projectName(), now())};
    }

    void rotate() const { rotateBackups(backupsFolderOf(*fileRef), projectName(), maxBackups); }

    /** After a failed write the project is as modified as before (the write itself changes the state tree only without
        the undo manager, but a flag set on the way would be wrong). */
    static void restoreModified(te::Edit& e, bool wasModified)
    {
        if (!wasModified)
            e.resetChangedStatus();
    }

    //==========================================================================
    te::Engine& engine;
    core::EditContext& context;
    BeforeReplaceHook hook;
    std::shared_ptr<juce::File> fileRef = std::make_shared<juce::File>();
    juce::ValueTree snapshot;  // the state that was written / read last, while `settling`
    bool settling = false;
    bool forceModified = false;  // while `settling`: modified although the content equals the snapshot (see install)
    SettleTimer settleTimer{*this};
    Clock clock;  // empty = juce::Time::getCurrentTime()
    int maxBackups = defaultMaxBackups;
    std::chrono::milliseconds autosaveInterval{defaultAutosaveIntervalMs};
    juce::ValueTree autosaved;             // the state of the autosave file, invalid = none written since open / save
    std::optional<RecoveryInfo> recovery;  // an offered, undecided recovery
    AutosaveTimer autosaveTimer{*this};
    std::unique_ptr<te::Edit> edit;  // last: destroyed first
};

//==============================================================================
ProjectSession::ProjectSession(te::Engine& engine, core::EditContext& context)
    : impl(std::make_unique<Impl>(engine, context))
{
    context.setEdit(nullptr);
}

ProjectSession::~ProjectSession() = default;

te::Edit* ProjectSession::edit() const noexcept
{
    return impl->edit.get();
}

void ProjectSession::setBeforeReplaceHook(BeforeReplaceHook hook)
{
    impl->hook = std::move(hook);
}

ProjectInfo ProjectSession::createProject(const juce::File& parentFolder, const juce::String& name)
{
    impl->requireNoUnsavedChanges("create a new project");
    requirePlainName(name);
    requireParentFolder(parentFolder);
    requirePathLength(parentFolder, name);

    const auto folder = parentFolder.getChildFile(name);
    const auto file = projectFileIn(folder, name);
    ProjectFolders folders(folder, file);

    // Template "empty": Tracktion's empty state, our format settings on top.
    auto state = te::createEmptyEdit(impl->engine);
    state.setProperty(te::IDs::alwaysUseRelativePaths, true, nullptr);
    state.setProperty(formatVersionProperty, currentFormatVersion, nullptr);

    auto fileRef = std::make_shared<juce::File>(file);
    auto created = impl->makeEdit(state, fileRef);
    if (created == nullptr)
        throw std::runtime_error("Tracktion could not create an empty project");

    folders.create();
    try
    {
        writeProjectFile(*created, file, impl->hook);
    }
    catch (...)
    {
        folders.takeBack();
        throw;
    }

    Impl::startClean(*created);
    impl->install(std::move(created), std::move(fileRef));
    impl->recovery.reset();
    return impl->infoOf();
}

ProjectInfo ProjectSession::openProject(const juce::File& file)
{
    impl->requireNoUnsavedChanges("open another project");
    if (!file.existsAsFile())
        fail(error_code::projectNotFound, "The project file " + quoted(file) + " does not exist");

    // An unreadable file names the newest backup / autosave: that is where the work is.
    const auto state = readState(file, "project file", recoveryHint(file));

    auto fileRef = std::make_shared<juce::File>(file);
    auto opened = impl->makeEdit(state, fileRef);
    if (opened == nullptr)
        fail(error_code::corruptProject, "The project file " + quoted(file) +
                                             " cannot be read as a Tracklab project (Tracktion could not load the "
                                             "project state); the file was not changed" +
                                             recoveryHint(file));

    // A write that was interrupted by a crash left a temporary file next to the project file.
    removeInterruptedWrites(file);

    // An autosave that is newer than the project file is a crash that lost work: offer it (never applied unasked).
    std::optional<RecoveryInfo> recovery;
    if (const auto autosave = autosaveFileOf(file);
        autosave.existsAsFile() && autosave.getLastModificationTime() > file.getLastModificationTime())
        recovery = RecoveryInfo{file.getLastModificationTime(), autosave.getLastModificationTime()};

    Impl::startClean(*opened);
    impl->install(std::move(opened), std::move(fileRef));
    impl->recovery = recovery;
    return impl->infoOf();
}

ProjectInfo ProjectSession::save()
{
    auto& edit = impl->requireEdit();
    if (edit.isSaveInhibited())
        fail(error_code::saveInhibited,
             "The project cannot be saved right now: an operation that changes it temporarily is running");

    const bool wasModified = impl->isModified();
    auto backup = impl->backupOfCurrentFile();  // the file as it is before this save replaces it
    try
    {
        writeProjectFile(edit, *impl->fileRef, impl->hook, &backup);
    }
    catch (...)
    {
        Impl::restoreModified(edit, wasModified);
        throw;
    }
    impl->rotate();
    return impl->finishSave(edit);
}

ProjectInfo ProjectSession::saveAs(const juce::File& parentFolder, const juce::String& name)
{
    auto& edit = impl->requireEdit();
    requirePlainName(name);
    requireParentFolder(parentFolder);
    requirePathLength(parentFolder, name);

    const auto folder = parentFolder.getChildFile(name);
    const auto file = projectFileIn(folder, name);
    ProjectFolders folders(folder, file);
    if (edit.isSaveInhibited())
        fail(error_code::saveInhibited,
             "The project cannot be saved right now: an operation that changes it temporarily is running");

    const bool wasModified = impl->isModified();
    folders.create();

    // The new project file is the Edit's file from here on; paths and file reference change together and change back
    // together if the write fails.
    const auto pathChanges = sourcePathChangesForMove(edit, file);
    const auto oldFile = *impl->fileRef;
    *impl->fileRef = file;  // first: writing a path makes the clip resolve it against the Edit's file at once
    applySourcePaths(pathChanges, true);
    try
    {
        writeProjectFile(edit, file, impl->hook);
    }
    catch (...)
    {
        *impl->fileRef = oldFile;  // first again: a path is written while the file it is relative to is current
        applySourcePaths(pathChanges, false);
        Impl::restoreModified(edit, wasModified);
        folders.takeBack();
        throw;
    }
    return impl->finishSave(edit);
}

void ProjectSession::closeProject(bool discard)
{
    if (impl->edit == nullptr)
        return;
    if (!discard)
        impl->requireNoUnsavedChanges("close it");

    // A clean close and a confirmed discard both end the session on purpose: the autosave is of no use any more. Only an
    // offered recovery that was never decided stays, so that it is offered again.
    const auto autosave = autosaveFileOf(*impl->fileRef);
    const bool recoveryUndecided = impl->recovery.has_value();
    impl->dropEdit();
    if (!recoveryUndecided)
        autosave.deleteFile();
}

//==============================================================================
// M1-05: autosave, backups, recovery.
void ProjectSession::setClock(Clock clock)
{
    impl->clock = std::move(clock);
}

void ProjectSession::setAutosaveInterval(std::chrono::milliseconds interval)
{
    impl->autosaveInterval = interval;
    impl->restartAutosaveTimer();
}

std::chrono::milliseconds ProjectSession::autosaveInterval() const
{
    return impl->autosaveInterval;
}

void ProjectSession::setMaxBackups(int count)
{
    impl->maxBackups = count;
}

int ProjectSession::maxBackups() const
{
    return impl->maxBackups;
}

AutosaveResult ProjectSession::autosaveNow()
{
    return impl->autosave();
}

std::vector<BackupInfo> ProjectSession::listBackups() const
{
    if (impl->edit == nullptr)
        return {};
    return collectBackups(backupsFolderOf(*impl->fileRef), impl->projectName());
}

std::optional<RecoveryInfo> ProjectSession::pendingRecovery() const
{
    return impl->recovery;
}

ProjectInfo ProjectSession::restoreAutosave()
{
    impl->requireEdit();
    const auto autosave = autosaveFileOf(*impl->fileRef);
    if (!autosave.existsAsFile())
        fail(error_code::noAutosave, "There is no autosave file " + quoted(autosave) + " of the open project");

    // Everything that can fail happens before the open project is touched.
    const auto state = readState(autosave, "autosave file", {});
    auto restored = impl->makeEdit(state, impl->fileRef);
    if (restored == nullptr)
        fail(error_code::corruptProject, "The autosave file " + quoted(autosave) +
                                             " cannot be read as a Tracklab project (Tracktion could not load the "
                                             "project state); the open project was not changed");

    Impl::startClean(*restored);
    impl->install(std::move(restored), impl->fileRef, true);
    impl->recovery.reset();
    impl->autosaved = impl->snapshot.createCopy();  // the autosave file has exactly this state
    return impl->infoOf();
}

void ProjectSession::discardAutosave()
{
    impl->requireEdit();
    const auto autosave = autosaveFileOf(*impl->fileRef);
    if (!autosave.existsAsFile())
        fail(error_code::noAutosave, "There is no autosave file " + quoted(autosave) + " of the open project");
    if (!autosave.deleteFile())
        fail(error_code::saveFailed, "Cannot delete the autosave file " + quoted(autosave));
    impl->recovery.reset();
    impl->autosaved = {};  // the next autosave writes again, whatever was written before
}

ProjectInfo ProjectSession::restoreBackup(const juce::String& name)
{
    auto& edit = impl->requireEdit();

    // The name comes from outside (GUI, Claude, MCP): a plain file name of this project's Backups folder, nothing else.
    if (name.isEmpty() || name.containsAnyOf("/\\") || name.contains("..") || juce::File::isAbsolutePath(name))
        throw core::CommandFailure(core::error_code::invalidParams,
                                   "\"" + name.toStdString() + "\" is not a backup name; use a name from list_backups",
                                   "/name");
    const auto listed = collectBackups(backupsFolderOf(*impl->fileRef), impl->projectName());
    if (std::none_of(listed.begin(), listed.end(), [&name](const BackupInfo& backup) { return backup.name == name; }))
        fail(error_code::backupNotFound, "The project has no backup \"" + name.toStdString() + "\"");
    const auto backupFile = backupsFolderOf(*impl->fileRef).getChildFile(name);

    // Load first, change nothing yet: an unreadable backup must not cost the current state.
    const auto state = readState(backupFile, "backup file", {});
    auto restored = impl->makeEdit(state, impl->fileRef);
    if (restored == nullptr)
        fail(error_code::corruptProject, "The backup file " + quoted(backupFile) +
                                             " cannot be read as a Tracklab project (Tracktion could not load the "
                                             "project state); the open project was not changed");

    // The current state, unsaved changes included, becomes a backup of its own, so that restoring loses nothing. Same
    // atomic write as a save; without the test hook, which is about the project file and the autosave. If it shares its
    // name with the chosen backup (same second) it gets a counter suffix, and the rotation that follows never removes the
    // chosen backup: it stays, however old it is.
    const auto backupsFolder = backupsFolderOf(*impl->fileRef);
    if (const auto result = backupsFolder.createDirectory(); result.failed())
        fail(error_code::saveFailed,
             "Cannot create the folder " + quoted(backupsFolder) + ": " + result.getErrorMessage().toStdString());
    writeProjectFile(edit, newBackupFile(backupsFolder, impl->projectName(), impl->now()), {});
    rotateBackups(backupsFolder, impl->projectName(), impl->maxBackups, name);

    Impl::startClean(*restored);
    impl->install(std::move(restored), impl->fileRef, true);
    return impl->infoOf();
}

ProjectInfo ProjectSession::info() const
{
    return impl->infoOf();
}

}  // namespace tracklab::project
