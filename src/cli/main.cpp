// tracklab-cli: thin main() around tracklab::cli::runCli (cli.h).
#include "cli/cli.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
// shellapi.h needs windows.h first.
#include <shellapi.h>
#endif

namespace
{

/** The arguments after the program name, as UTF-8. Windows hands argv[] to main() in the ANSI code page, which
    mangles every non-ASCII character (a device name with an umlaut), so there the wide command line is converted
    instead. This works on every Windows version and needs no manifest. */
std::vector<std::string> utf8Arguments(int argc, char** argv)
{
    std::vector<std::string> args;
#ifdef _WIN32
    (void)argc;
    (void)argv;
    int count = 0;
    if (LPWSTR* wide = CommandLineToArgvW(GetCommandLineW(), &count))
    {
        for (int i = 1; i < count; ++i)
            args.emplace_back(juce::String(wide[i]).toStdString());
        LocalFree(wide);
        return args;
    }
#endif
    for (int i = 1; i < argc; ++i)
        args.emplace_back(argv[i]);
    return args;
}

}  // namespace

int main(int argc, char** argv)
{
#ifdef _WIN32
    // The output is UTF-8 (JSON); without this the console shows it in the OEM code page.
    SetConsoleOutputCP(CP_UTF8);
#endif
    // The engine expects a message thread (MessageManager); this thread is it for the lifetime of the command.
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    return tracklab::cli::runCli(utf8Arguments(argc, argv), std::cout, std::cerr);
}
