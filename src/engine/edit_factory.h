// Edit factory (M1-03): the place that decides how a project (te::Edit) is created, so far the undo depth.
#pragma once

#include "core/undo_levels.h"

#include <tracktion_engine/tracktion_engine.h>

#include <memory>

namespace tracklab::engine
{

namespace te = tracktion;

/** Undo depth of a project (DESIGN Rev 3: 200, adjustable). Tracktion's own default is 30. The value lives in core
    (core/undo_levels.h), where the project module reads it, too. */
inline constexpr int defaultUndoLevels = core::defaultUndoLevels;

struct EditOptions
{
    /** Number of undo steps (transactions) the Edit keeps at least; passed as Edit::Options::numUndoLevelsToStore
        (the UndoManager also drops old steps when their total size exceeds 1000 units per level, but never keeps
        fewer than `undoLevels` steps). */
    int undoLevels = defaultUndoLevels;
};

/** Creates an empty, editable project (te::Edit::createEdit with the state of a new empty Edit) with
    Edit::Options::numUndoLevelsToStore = options.undoLevels. The undo history of the new Edit is empty.
    Message thread only; destroy the Edit before the engine. Returns null if Tracktion cannot create it. */
std::unique_ptr<te::Edit> createEdit(te::Engine& engine, const EditOptions& options = {});

}  // namespace tracklab::engine
