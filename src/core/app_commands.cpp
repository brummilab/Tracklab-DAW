// STUB (test-writer, M1-02): replaced by the implementer.
#include "core/app_commands.h"

namespace tracklab::core
{

RegisterResult registerAppCommands(CommandRegistry&, std::string)
{
    RegisterResult result;
    result.error.code = "not_implemented";
    return result;
}

}  // namespace tracklab::core
