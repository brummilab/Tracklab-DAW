#include "core/tool_names.h"

#include <algorithm>

namespace tracklab::core
{

namespace
{

bool isLowerAscii(char c)
{
    return c >= 'a' && c <= 'z';
}

bool isUpperAscii(char c)
{
    return c >= 'A' && c <= 'Z';
}

bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

/** [a-z][a-z0-9_]* */
bool isValidSegment(std::string_view segment)
{
    if (segment.empty() || !isLowerAscii(segment.front()))
        return false;
    return std::ranges::all_of(segment, [](char c) { return isLowerAscii(c) || isDigit(c) || c == '_'; });
}

}  // namespace

bool isValidCommandId(std::string_view id)
{
    // Hand-written instead of std::regex: no locale surprises (non-ASCII, "$" matching before a trailing newline).
    int segments = 0;
    while (true)
    {
        const auto dot = id.find('.');
        if (!isValidSegment(id.substr(0, dot)))
            return false;
        ++segments;
        if (dot == std::string_view::npos)
            break;
        id.remove_prefix(dot + 1);
    }
    return segments >= 2;
}

bool isValidToolName(std::string_view toolName)
{
    if (toolName.empty() || toolName.size() > 128)
        return false;
    return std::ranges::all_of(toolName, [](char c)
                               { return isLowerAscii(c) || isUpperAscii(c) || isDigit(c) || c == '_' || c == '-'; });
}

std::string toolNameFromId(std::string_view id)
{
    std::string name(id);
    std::ranges::replace(name, '.', '_');
    return name;
}

}  // namespace tracklab::core
