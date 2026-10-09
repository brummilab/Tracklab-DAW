// Fixture and helpers of the project tests (M1-04): a headless engine, an EditContext, a command registry with the
// edit.* and project.* commands, a ProjectSession, and a temporary folder that holds every project of the test.
// No audio device, no real user folder (testOptions()), no network. Audio files are generated (synthetic sine).
#pragma once

#include "core/command_registry.h"
#include "core/edit_commands.h"
#include "core/edit_context.h"
#include "engine/engine_factory.h"
#include "project/project_commands.h"
#include "project/project_format.h"
#include "project/project_session.h"

#include "engine/engine_test_options.h"
#include "test_support.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <tracktion_engine/tracktion_engine.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace tracklab_test::project
{

namespace te = tracktion;
using tracklab::core::CommandResult;
using tracklab::core::EditContext;
using tracklab::core::Json;

//==============================================================================
// Small helpers

inline std::string utf8(const juce::File& file)
{
    return file.getFullPathName().toStdString();
}

inline juce::File fileFromUtf8(const std::string& path)
{
    return juce::File(juce::String::fromUTF8(path.c_str()));
}

/** The whole file as bytes (binary-exact, so that "untouched" can be compared). */
inline juce::MemoryBlock bytesOf(const juce::File& file)
{
    juce::MemoryBlock data;
    file.loadFileAsData(data);
    return data;
}

inline bool sameBytes(const juce::MemoryBlock& a, const juce::MemoryBlock& b)
{
    return a.getSize() == b.getSize() && a.matches(b.getData(), b.getSize());
}

inline void pumpMessageLoop(int ms)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}

/** Lets pending messages through (Edit updates, the UndoManager's change messages that set "modified"). */
inline void settle(te::Edit& edit)
{
    edit.dispatchPendingUpdatesSynchronously();
    edit.getUndoManager().dispatchPendingMessages();
}

/** The names of the plain files directly in `folder`, sorted (sub folders are not listed). */
inline std::vector<std::string> fileNamesIn(const juce::File& folder)
{
    std::vector<std::string> names;
    for (const auto& file : folder.findChildFiles(juce::File::findFiles, false))
        names.push_back(file.getFileName().toStdString());
    std::sort(names.begin(), names.end());
    return names;
}

/** The root element of an XML file, null if it cannot be parsed. */
inline std::unique_ptr<juce::XmlElement> parseXml(const juce::File& file)
{
    return juce::XmlDocument::parse(file);
}

/** Parses `file`, lets `change` modify the root element, writes it back (for building old, new and foreign files). */
template <typename Change> void rewriteXml(const juce::File& file, Change&& change)
{
    auto xml = parseXml(file);
    REQUIRE(xml != nullptr);
    change(*xml);
    REQUIRE(xml->writeTo(file));
}

inline void writeText(const juce::File& file, const juce::String& text)
{
    file.deleteFile();
    REQUIRE(file.replaceWithText(text));
}

/** A mono 16 bit WAV with a 440 Hz sine, generated, nothing recorded: the only kind of audio the tests use. */
inline void writeSineWav(const juce::File& file, double seconds = 0.5, double sampleRate = 44100.0)
{
    REQUIRE(file.getParentDirectory().createDirectory().wasOk());
    file.deleteFile();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
    REQUIRE(stream != nullptr);
    auto writer = format.createWriterFor(
        stream, juce::AudioFormatWriterOptions().withSampleRate(sampleRate).withNumChannels(1).withBitsPerSample(16));
    REQUIRE(writer != nullptr);  // the writer owns the stream now

    const int numSamples = juce::roundToInt(seconds * sampleRate);
    juce::AudioBuffer<float> buffer(1, numSamples);
    for (int i = 0; i < numSamples; ++i)
        buffer.setSample(0, i, 0.25f * static_cast<float>(std::sin(2.0 * 3.14159265358979 * 440.0 * i / sampleRate)));
    REQUIRE(writer->writeFromAudioSampleBuffer(buffer, 0, numSamples));
}

//==============================================================================
// Helpers of the autosave / backup / recovery tests (M1-05)

/** `<project file>.autosave`, spelled out here on purpose (the name is part of the format, E41). */
inline juce::File autosaveFileOf(const juce::File& projectFile)
{
    return projectFile.getSiblingFile(projectFile.getFileName() + ".autosave");
}

inline juce::File backupsFolderOf(const juce::File& projectFile)
{
    return projectFile.getParentDirectory().getChildFile("Backups");
}

/** The file names in Backups/ of a project, sorted. */
inline std::vector<std::string> backupNamesOf(const juce::File& projectFile)
{
    return fileNamesIn(backupsFolderOf(projectFile));
}

/** The `name` attributes of the TRACK elements of a project / autosave / backup file, in file order. */
inline std::vector<std::string> trackNamesOfFile(const juce::File& file)
{
    std::vector<std::string> names;
    const auto xml = parseXml(file);
    REQUIRE(xml != nullptr);
    for (const auto* track : xml->getChildWithTagNameIterator("TRACK"))
        names.push_back(track->getStringAttribute("name").toStdString());
    return names;
}

/** The injected clock of the tests: 9 October 2026, 12:34:56 + `seconds`, local time (the month argument of juce::Time
    is zero based). Backups are named after it. */
inline juce::Time clockTime(int seconds = 0)
{
    return juce::Time(2026, 9, 9, 12, 34, 56, 0, true) + juce::RelativeTime::seconds(seconds);
}

/** Backup file name of project `name` for the clock time `seconds` seconds after clockTime(). */
inline std::string backupNameAt(const std::string& name, int seconds = 0)
{
    return name + "." + clockTime(seconds).formatted("%Y%m%d-%H%M%S").toStdString() + ".tracklab";
}

struct ProjectFixture;

/** The writes of the atomic save as the BeforeReplaceHook sees them: how often a file named `<target>` was about to be
    replaced. Never refuses a write unless `refuse` is set for it. */
struct WriteCounter
{
    explicit WriteCounter(ProjectFixture& fixture);

    int autosaveWrites = 0;  ///< targets that end in ".autosave"
    int projectWrites = 0;   ///< all others
    bool refuseAutosave = false;
    juce::File lastTemporary;
    juce::File lastTarget;
};

//==============================================================================
/** Engine + context + registry + session. Member order = construction order; the session (and with it the Edit) dies
    first, the engine last. */
struct ProjectFixture
{
    ProjectFixture()
    {
        engine = tracklab::engine::createEngine(testOptions());
        REQUIRE(engine != nullptr);
        registry.setEditContext(&context);
        REQUIRE(tracklab::core::registerEditCommands(registry, context).ok);
        session = std::make_unique<tracklab::project::ProjectSession>(*engine, context);
        const auto outcome = tracklab::project::registerProjectCommands(registry, *session);
        INFO("registerProjectCommands: " << outcome.error.code << " / " << outcome.error.message);
        REQUIRE(outcome.ok);
    }

    ProjectFixture(const ProjectFixture&) = delete;
    ProjectFixture& operator=(const ProjectFixture&) = delete;

    /** Folder that holds the projects of this test (the `folder` parameter of project.new). */
    const juce::File& root() const { return temp.dir(); }

    CommandResult tryRun(const std::string& id, const Json& params = Json::object()) const
    {
        return registry.execute(id, params);
    }

    /** Runs a command and requires success; returns the result. */
    Json run(const std::string& id, const Json& params = Json::object()) const
    {
        const auto result = registry.execute(id, params);
        INFO(id << ": " << result.error.code << " / " << result.error.message << " @ " << result.error.pointer);
        REQUIRE(result.ok);
        return result.result;
    }

    /** Runs a command that has to fail and returns the error code ("" if it succeeded). */
    std::string errorOf(const std::string& id, const Json& params = Json::object()) const
    {
        const auto result = registry.execute(id, params);
        INFO(id << " was expected to fail; ok=" << result.ok);
        return result.ok ? std::string() : result.error.code;
    }

    /** The open Edit (REQUIRE: there is one). */
    te::Edit& edit() const
    {
        REQUIRE(session->edit() != nullptr);
        return *session->edit();
    }

    /** Lets the Edit settle after it was created / opened: the change listener of the UndoManager is attached
        asynchronously, and the "modified" flag is only meaningful afterwards. */
    void settleEdit() const
    {
        pumpMessageLoop(250);
        if (session->edit() != nullptr)
            settle(*session->edit());
    }

    /** project.new `<root>/<name>/<name>.tracklab`; returns the project file. */
    juce::File newProject(const std::string& name = "Muster") const
    {
        const auto result = run("project.new", Json{{"folder", utf8(root())}, {"name", name}});
        settleEdit();
        return fileFromUtf8(result["path"].get<std::string>());
    }

    juce::File projectFolder(const std::string& name = "Muster") const
    {
        return root().getChildFile(juce::String::fromUTF8(name.c_str()));
    }

    juce::File projectFile(const std::string& name = "Muster") const
    {
        return projectFolder(name).getChildFile(juce::String::fromUTF8(name.c_str()) +
                                                tracklab::project::fileExtension);
    }

    Json info() const { return run("project.get_info"); }
    bool modified() const { return info()["modified"].get<bool>(); }

    /** Makes the project "modified" through an undoable write (like any command does). */
    void makeModified()
    {
        auto& e = edit();
        pumpMessageLoop(30);
        e.state.setProperty("sampleNote", ++counter, &e.getUndoManager());
        settle(e);
        REQUIRE(e.hasChangedSinceSaved());
    }

    /** Tracktion marks an Edit "changed" up to 500 ms after the last plugin change (Edit::PluginChangeTimer), even if
        it was saved in between. Tests that add tracks or clips therefore let those timers run out before they go on, so
        that "modified" means what the test says (see the report of M1-04, open question on the modified flag). */
    static void letPluginTimersRunOut(te::Edit& e)
    {
        pumpMessageLoop(700);
        settle(e);
    }

    /** Some project content: three tracks with names, a tempo. Everything goes through the undo manager. */
    void addSampleContent()
    {
        auto& e = edit();
        e.ensureNumberOfAudioTracks(3);
        const char* names[] = {"Gitarre", "Bass", "Gesang"};
        auto tracks = te::getAudioTracks(e);
        REQUIRE(tracks.size() >= 3);
        for (int i = 0; i < 3; ++i)
            tracks[i]->setName(names[i]);
        e.tempoSequence.getTempo(0)->setBpm(97.0);
        settle(e);
        letPluginTimersRunOut(e);
    }

    /** A wave clip of a generated sine file on the first audio track (the Tracktion API, as the import will do). */
    te::WaveAudioClip::Ptr addClip(const juce::File& audioFile, const juce::String& name = "Take 1")
    {
        auto& e = edit();
        e.ensureNumberOfAudioTracks(1);
        auto tracks = te::getAudioTracks(e);
        REQUIRE_FALSE(tracks.isEmpty());
        auto clip = te::insertWaveClip(*tracks[0], name, audioFile,
                                       te::ClipPosition{.time = te::TimeRange(te::TimePosition::fromSeconds(0.0),
                                                                              te::TimePosition::fromSeconds(0.5))},
                                       te::DeleteExistingClips::no);
        REQUIRE(clip != nullptr);
        settle(e);
        letPluginTimersRunOut(e);
        return clip;
    }

    /** Renames the first audio track (an undoable change, like any command makes it): the project turns "modified". */
    void renameFirstTrack(const juce::String& newName)
    {
        auto& e = edit();
        pumpMessageLoop(30);
        auto tracks = te::getAudioTracks(e);
        REQUIRE_FALSE(tracks.isEmpty());
        tracks[0]->setName(newName);
        settle(e);
        REQUIRE(e.hasChangedSinceSaved());
    }

    /** project.save with the clock `seconds` after clockTime(): the backup of that save is named after it. */
    void saveAt(int seconds)
    {
        clockNow = clockTime(seconds);
        run("project.save");
    }

    /** Injects the clock of the session: it reads `clockNow`. */
    void useInjectedClock()
    {
        clockNow = clockTime(0);
        session->setClock([this] { return clockNow; });
    }

    ScopedTempDir temp;
    juce::Time clockNow = clockTime(0);
    std::unique_ptr<te::Engine> engine;
    EditContext context;
    tracklab::core::CommandRegistry registry;
    std::unique_ptr<tracklab::project::ProjectSession> session;
    int counter = 0;
};

inline WriteCounter::WriteCounter(ProjectFixture& fixture)
{
    fixture.session->setBeforeReplaceHook(
        [this](const juce::File& temporary, const juce::File& target)
        {
            lastTemporary = temporary;
            lastTarget = target;
            if (target.getFileName().endsWith(".autosave"))
            {
                ++autosaveWrites;
                return !refuseAutosave;
            }
            ++projectWrites;
            return true;
        });
}

//==============================================================================
/** A project as it is on disk after a crash: saved with the track names Gitarre / Bass / Gesang, then the first track
    renamed to "Neu" (unsaved), autosaved, and the whole project folder copied (while it was open) to
    `<root>/Absturz/<name>/`. The original is closed (discarded). The copy's project file has the modification time
    `projectTime`, its autosave file `autosaveTime` (set explicitly: no waiting for the file system's resolution). */
struct CrashedProject
{
    juce::File file;      ///< the copy's project file
    juce::File autosave;  ///< the copy's autosave file
    juce::Time projectTime;
    juce::Time autosaveTime;
};

inline CrashedProject crashedProject(ProjectFixture& f, const std::string& name = "Muster",
                                     const juce::Time& projectTime = juce::Time(2026, 9, 9, 10, 0, 0, 0, true),
                                     const juce::Time& autosaveTime = juce::Time(2026, 9, 9, 10, 5, 0, 0, true))
{
    const auto original = f.newProject(name);
    f.addSampleContent();
    f.run("project.save");
    f.settleEdit();
    f.renameFirstTrack("Neu");
    REQUIRE(f.session->autosaveNow() == tracklab::project::AutosaveResult::written);
    REQUIRE(autosaveFileOf(original).existsAsFile());

    const auto copyFolder = f.root().getChildFile("Absturz").getChildFile(juce::String::fromUTF8(name.c_str()));
    REQUIRE(f.projectFolder(name).copyDirectoryTo(copyFolder));
    f.run("project.close", Json{{"discard", true}});

    CrashedProject crashed;
    crashed.file = copyFolder.getChildFile(original.getFileName());
    crashed.autosave = autosaveFileOf(crashed.file);
    crashed.projectTime = projectTime;
    crashed.autosaveTime = autosaveTime;
    REQUIRE(crashed.file.existsAsFile());
    REQUIRE(crashed.autosave.existsAsFile());
    REQUIRE(crashed.file.setLastModificationTime(projectTime));
    REQUIRE(crashed.autosave.setLastModificationTime(autosaveTime));
    return crashed;
}

}  // namespace tracklab_test::project
