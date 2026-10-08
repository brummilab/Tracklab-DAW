// Command id <-> API tool name (M1-02, DESIGN Rev 3 section 4).
// The Claude API allows tool names matching ^[a-zA-Z0-9_-]{1,128}$; the tool name of a command is its id with "_"
// instead of ".". The reverse direction is NOT "replace _ by ." (ids may contain "_"): the registry keeps a map.
#pragma once

#include <string>
#include <string_view>

namespace tracklab::core
{

/** Command id rules: at least two segments separated by single dots; a segment starts with a lowercase ASCII letter
    and continues with lowercase letters, digits and "_" ("track.create", "assistant.propose_plan"). The length
    limit belongs to the tool name (isValidToolName), not to this function. */
bool isValidCommandId(std::string_view id);

/** ^[a-zA-Z0-9_-]{1,128}$ */
bool isValidToolName(std::string_view toolName);

/** The id with every "." replaced by "_". Does not validate. */
std::string toolNameFromId(std::string_view id);

}  // namespace tracklab::core
