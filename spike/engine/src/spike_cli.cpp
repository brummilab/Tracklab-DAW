// spike_cli: command line front end of the engine spike. All logic lives in spike_core (spike_common.h).
#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>
#include <string>
#include <vector>

#include "spike_common.h"

int main(int argc, char** argv)
{
    // The engine expects a message thread (MessageManager); this thread is it for the lifetime of the command.
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i)
        args.emplace_back(argv[i]);

    return spike::runCli(args, std::cout, std::cerr);
}
