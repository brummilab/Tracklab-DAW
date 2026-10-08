// Test runner of Tracklab: doctest (from Tracktion's modules/3rd_party, no new dependency) plus JUCE message-thread
// setup. One binary, one doctest suite per module (--test-suite=<module>), one CTest entry per suite.
//
// Exit code: the doctest result if anything failed; 77 if nothing failed but a test was skipped (CTest reports
// this as "skipped" via SKIP_RETURN_CODE, never as "passed"); 0 otherwise.
#define DOCTEST_CONFIG_IMPLEMENT
#include "test_support.h"

#include <juce_gui_basics/juce_gui_basics.h>

int main(int argc, char** argv)
{
    // The engine is created, used and destroyed on the message thread = this thread.
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    doctest::Context context;
    context.applyCommandLine(argc, argv);
    const int result = context.run();
    if (context.shouldExit())
        return result;

    if (result != 0)
        return result;
    return tracklab_test::skippedCount() > 0 ? 77 : 0;
}
