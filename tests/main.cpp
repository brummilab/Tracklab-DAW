// Test runner of Tracklab: doctest (from Tracktion's modules/3rd_party, no new dependency) plus JUCE message-thread
// setup. One binary, one doctest suite per module (--test-suite=<module>), one CTest entry per suite.
//
// Exit code: the doctest result if anything failed; 77 if nothing failed but a test was skipped (CTest reports
// this as "skipped" via SKIP_RETURN_CODE, never as "passed"); 0 otherwise.
#define DOCTEST_CONFIG_IMPLEMENT
#include "test_support.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>

namespace
{

/** The folders of the real user that the engine would create by default. A test run must not create any of them
    (tests point the engine at temporary folders); main() fails the run if one appeared. Checked as seen at start, so
    a user who has these folders from real use is not affected. */
juce::Array<juce::File> userFoldersOfTheEngine()
{
    juce::Array<juce::File> folders;
#if JUCE_LINUX
    const auto cacheHome = juce::SystemStats::getEnvironmentVariable("XDG_CACHE_HOME", {});
    folders.add(cacheHome.isNotEmpty() && juce::File::isAbsolutePath(cacheHome)
                    ? juce::File(cacheHome).getChildFile("Tracklab")
                    : juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                          .getChildFile(".cache")
                          .getChildFile("Tracklab"));
    const auto runtime = juce::SystemStats::getEnvironmentVariable("XDG_RUNTIME_DIR", {});
    if (runtime.isNotEmpty() && juce::File::isAbsolutePath(runtime))
        folders.add(juce::File(runtime).getChildFile("tracklab"));
#elif JUCE_WINDOWS
    const auto local = juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA", {});
    if (local.isNotEmpty())
        folders.add(juce::File(local).getChildFile("Tracklab"));
#endif
    return folders;
}

}  // namespace

int main(int argc, char** argv)
{
    // The engine is created, used and destroyed on the message thread = this thread.
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto userFolders = userFoldersOfTheEngine();
    juce::Array<bool> existedBefore;
    for (const auto& folder : userFolders)
        existedBefore.add(folder.exists());

    const tracklab_test::ScopedTempDir engineTemp;
    tracklab_test::setEngineTempDirectory(engineTemp.dir());

    doctest::Context context;
    context.applyCommandLine(argc, argv);
    const int result = context.run();
    if (context.shouldExit())
        return result;

    for (int i = 0; i < userFolders.size(); ++i)
    {
        if (!existedBefore[i] && userFolders[i].exists())
        {
            std::fprintf(stderr, "FAILED: the tests created %s in the real user folders\n",
                         userFolders[i].getFullPathName().toRawUTF8());
            return result != 0 ? result : 1;
        }
    }

    if (result != 0)
        return result;
    return tracklab_test::skippedCount() > 0 ? 77 : 0;
}
