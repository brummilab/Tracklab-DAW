// tracklab-cli: thin main() around tracklab::cli::runCli (cli.h).
#include "cli/cli.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    // The engine expects a message thread (MessageManager); this thread is it for the lifetime of the command.
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i)
        args.emplace_back(argv[i]);

    return tracklab::cli::runCli(args, std::cout, std::cerr);
}
