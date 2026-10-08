#include "core/command.h"

namespace tracklab::core
{

Json CommandError::toJson() const
{
    return Json{{"code", code}, {"message", message}, {"pointer", pointer}};
}

Json CommandResult::toJson() const
{
    if (ok)
        return Json{{"ok", true}, {"result", result}};
    return Json{{"ok", false}, {"error", error.toJson()}};
}

}  // namespace tracklab::core
