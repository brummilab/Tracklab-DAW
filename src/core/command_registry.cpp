// STUB (test-writer, M1-02): replaced by the implementer.
#include "core/command_registry.h"

namespace tracklab::core
{

RegisterResult CommandRegistry::registerCommand(Command)
{
    RegisterResult result;
    result.error.code = "not_implemented";
    return result;
}

std::size_t CommandRegistry::size() const noexcept
{
    return 0;
}

bool CommandRegistry::contains(std::string_view) const
{
    return false;
}

const Command* CommandRegistry::find(std::string_view) const
{
    return nullptr;
}

std::vector<const Command*> CommandRegistry::list() const
{
    return {};
}

std::optional<std::string> CommandRegistry::toolNameForId(std::string_view) const
{
    return std::nullopt;
}

std::optional<std::string> CommandRegistry::idForToolName(std::string_view) const
{
    return std::nullopt;
}

CommandResult CommandRegistry::execute(std::string_view, const Json&) const
{
    CommandResult result;
    result.error.code = "not_implemented";
    return result;
}

}  // namespace tracklab::core
