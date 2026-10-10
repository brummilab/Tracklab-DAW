// Failure type of the CLI (M1-07): what a subcommand throws, runCli turns it into the one JSON error line and the exit code.
#pragma once

#include "cli/cli.h"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace tracklab::cli
{

class CliFailure : public std::runtime_error
{
public:
    CliFailure(int exit, std::string errorCode, const std::string& text, std::string jsonPointer = {})
        : std::runtime_error(text), exitCodeValue(exit), codeValue(std::move(errorCode)),
          pointerValue(std::move(jsonPointer))
    {
    }

    int exitCode() const noexcept { return exitCodeValue; }
    const std::string& code() const noexcept { return codeValue; }
    const std::string& pointer() const noexcept { return pointerValue; }

    /** run-commands: index of the failing step. */
    std::optional<std::size_t> failedIndex;

private:
    int exitCodeValue;
    std::string codeValue;
    std::string pointerValue;
};

/** A usage error (exit code 2, code "usage"). */
inline CliFailure usageError(const std::string& text)
{
    return CliFailure(exitUsage, "usage", text);
}

/** An operation that failed (exit code 1). */
inline CliFailure operationFailed(std::string code, const std::string& text)
{
    return CliFailure(exitFailed, std::move(code), text);
}

}  // namespace tracklab::cli
