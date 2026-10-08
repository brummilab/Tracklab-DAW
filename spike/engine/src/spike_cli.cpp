// spike_cli: command line front end of the engine spike. All logic lives in spike_core (spike_common.h).
#include "spike_common.h"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    // The implementer wraps this in a juce::ScopedJuceInitialiser_GUI once the engine code needs it.
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i)
        args.emplace_back(argv[i]);

    return spike::runCli(args, std::cout, std::cerr);
}
