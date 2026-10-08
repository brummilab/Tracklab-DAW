// Engine factory of Tracklab (M1-01): the only place that constructs a Tracktion Engine.
//
// Every caller (app, CLI, tests) creates its engine here and destroys it before the process ends
// (TRACKTION_ENABLE_SINGLETONS=0), so several engines may follow one another in one process.
#pragma once

#include <tracktion_engine/tracktion_engine.h>

#include <cstdint>
#include <memory>

namespace tracklab::engine
{

namespace te = tracktion;

/** Whether the engine opens audio devices on its own. */
enum class DeviceMode : std::uint8_t
{
    none,      ///< CLI and tests: no system audio device is opened or scanned (headless CI).
    automatic  ///< App: the engine initialises the device manager and the system device types.
};

/** Where the engine keeps its settings (te::PropertyStorage). */
enum class SettingsStorage : std::uint8_t
{
    inMemory,  ///< Nothing is written to the user's settings folders. Prefs/cache folders are private temporary
               ///< folders that are deleted together with the engine. `settingsDirectory` is ignored.
    file       ///< `<settingsDirectory>/settings.xml`, written atomically (see EngineOptions).
};

struct EngineOptions
{
    DeviceMode devices = DeviceMode::none;
    SettingsStorage storage = SettingsStorage::inMemory;

    /** Folder of `settings.xml` for SettingsStorage::file. Empty = defaultSettingsDirectory().

        Contract of the file variant:
        - `PropertyStorage::flushSettingsToDisk()` and the destruction of the engine write the whole file.
        - Writing is atomic: the content goes to a juce::TemporaryFile in the same folder as the target, is flushed,
          and replaces the target with overwriteTargetFileWithTemporary(). If that fails, the existing target stays
          unchanged and nothing is thrown. No temporary file is left behind after a flush.
        - A missing settings folder is created when the engine is created or when the file is written.
        - An existing, readable settings.xml is read when the engine is created (values survive a restart).
        - A missing or unreadable (e.g. truncated) file means default settings; creation must not fail. */
    juce::File settingsDirectory;
};

/** `userApplicationDataDirectory/Tracklab`: folder of settings.xml when EngineOptions::settingsDirectory is empty.
    Does not create the folder. */
juce::File defaultSettingsDirectory();

/** Creates a Tracktion Engine with Tracklab's behaviours. Never returns null.

    - PropertyStorage::getUserName() is "Tracklab" (never the login/full name of the system user: the name ends up
      in saved projects).
    - PropertyStorage::getApplicationVersion() is the CMake project version (PROJECT_VERSION).
    - UIBehaviour is headless and never blocks or opens a dialog: runTaskWithProgressBar() runs the job on a worker
      thread while the calling thread dispatches messages; alerts and messages are swallowed; asynchronous
      confirmation requests are never answered with "yes"/"OK".
    - Has to be called, and the engine destroyed, on the JUCE message thread. */
std::unique_ptr<te::Engine> createEngine(const EngineOptions& options = {});

}  // namespace tracklab::engine
