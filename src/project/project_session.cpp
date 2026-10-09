#include "project/project_session.h"

#include "core/undo_levels.h"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace tracklab::project
{

namespace
{

/** How long after a save / open the "modified" flag is looked at once more, see ProjectSession::Impl::SettleTimer.
    Tracktion's PluginChangeTimer marks the Edit changed 500 ms after the last plugin change; 650 leaves some slack. */
constexpr int settleDelayMs = 650;

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

juce::File projectFileIn(const juce::File& projectFolder, const juce::String& name)
{
    return projectFolder.getChildFile(name + fileExtension);
}

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
    removed by juce::TemporaryFile's destructor in every case. */
void writeProjectFile(te::Edit& edit, const juce::File& target, const ProjectSession::BeforeReplaceHook& hook)
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

    if (!temporary.overwriteTargetFileWithTemporary())
        fail(error_code::saveFailed, "Cannot replace " + quoted(target) + " by the saved project");
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
        alignFlagWithContent();
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
        return settling ? alignFlagWithContent() : edit->hasChangedSinceSaved();
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
        settling = false;
        context.setEdit(nullptr);
        edit.reset();  // before the engine goes away
        fileRef = std::make_shared<juce::File>();
    }

    /** Makes `fresh` the open project. The previous Edit is destroyed after the context points to the new one. */
    void install(std::unique_ptr<te::Edit> fresh, std::shared_ptr<juce::File> file)
    {
        settleTimer.stopTimer();
        settling = false;
        context.setEdit(fresh.get());
        auto previous = std::move(edit);
        edit = std::move(fresh);
        fileRef = std::move(file);
        previous.reset();
        beginSettling();
    }

    ProjectInfo finishSave(te::Edit& e)
    {
        e.resetChangedStatus();
        beginSettling();
        return infoOf();
    }

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
    SettleTimer settleTimer{*this};
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
    return impl->infoOf();
}

ProjectInfo ProjectSession::openProject(const juce::File& file)
{
    impl->requireNoUnsavedChanges("open another project");
    if (!file.existsAsFile())
        fail(error_code::projectNotFound, "The project file " + quoted(file) + " does not exist");

    const auto corrupt = [&file](const std::string& reason)
    {
        fail(error_code::corruptProject, "The project file " + quoted(file) +
                                             " cannot be read as a Tracklab project (" + reason +
                                             "); the file was not changed");
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
        fail(outcome.code, "The project file " + quoted(file) + ": " + outcome.message);
    state.setProperty(te::IDs::alwaysUseRelativePaths, true, nullptr);

    auto fileRef = std::make_shared<juce::File>(file);
    auto opened = impl->makeEdit(state, fileRef);
    if (opened == nullptr)
        corrupt("Tracktion could not load the project state");

    Impl::startClean(*opened);
    impl->install(std::move(opened), std::move(fileRef));
    return impl->infoOf();
}

ProjectInfo ProjectSession::save()
{
    auto& edit = impl->requireEdit();
    if (edit.isSaveInhibited())
        fail(error_code::saveInhibited,
             "The project cannot be saved right now: an operation that changes it temporarily is running");

    const bool wasModified = impl->isModified();
    try
    {
        writeProjectFile(edit, *impl->fileRef, impl->hook);
    }
    catch (...)
    {
        Impl::restoreModified(edit, wasModified);
        throw;
    }
    return impl->finishSave(edit);
}

ProjectInfo ProjectSession::saveAs(const juce::File& parentFolder, const juce::String& name)
{
    auto& edit = impl->requireEdit();
    requirePlainName(name);
    requireParentFolder(parentFolder);

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
    impl->dropEdit();
}

ProjectInfo ProjectSession::info() const
{
    return impl->infoOf();
}

}  // namespace tracklab::project
