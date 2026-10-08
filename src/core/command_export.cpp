// STUB (test-writer, M1-02): replaced by the implementer.
#include "core/command_export.h"

namespace tracklab::core
{

std::string exportToolsJson(const CommandRegistry&)
{
    return {};
}

std::string exportCommandsMarkdown(const CommandRegistry&)
{
    return {};
}

bool writeExportedFiles(const CommandRegistry&, const std::filesystem::path&)
{
    return false;
}

ExportCheck checkExportedFiles(const CommandRegistry&, const std::filesystem::path&)
{
    ExportCheck check;
    check.problem = "not implemented";
    return check;
}

}  // namespace tracklab::core
