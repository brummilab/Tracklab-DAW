#include "engine/edit_factory.h"

namespace tracklab::engine
{

// STUB (test-writer, M1-03): creates the Edit but keeps Tracktion's default undo depth (30); `undoLevels` is applied
// with the card.
std::unique_ptr<te::Edit> createEdit(te::Engine& engine, const EditOptions&)
{
    auto state = te::createEmptyEdit(engine);
    auto projectId = te::ProjectItemID::fromProperty(state, te::IDs::projectID);
    return te::Edit::createEdit(te::Edit::Options{engine, state, projectId});
}

}  // namespace tracklab::engine
