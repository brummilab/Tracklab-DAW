#include "engine/edit_factory.h"

namespace tracklab::engine
{

std::unique_ptr<te::Edit> createEdit(te::Engine& engine, const EditOptions& options)
{
    auto state = te::createEmptyEdit(engine);
    auto projectId = te::ProjectItemID::fromProperty(state, te::IDs::projectID);
    // Tracktion's default of 30 steps is too shallow for a session; the depth is the only option we change.
    return te::Edit::createEdit(te::Edit::Options{.engine = engine,
                                                  .editState = state,
                                                  .editProjectItemID = projectId,
                                                  .numUndoLevelsToStore = options.undoLevels});
}

}  // namespace tracklab::engine
