// Undo depth of a project (M1-03, moved here in M1-04): in core because every module that creates an Edit (engine,
// project) needs the same number, and the modules do not include each other.
#pragma once

namespace tracklab::core
{

/** Undo depth of a project (DESIGN Rev 3: 200, adjustable). Tracktion's own default is 30. */
inline constexpr int defaultUndoLevels = 200;

}  // namespace tracklab::core
