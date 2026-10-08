// STUB (tests first, M1-01): the implementer replaces this file with the real engine factory.
#include "engine/engine_factory.h"

namespace tracklab::engine
{

juce::File defaultSettingsDirectory()
{
    return {};
}

std::unique_ptr<te::Engine> createEngine(const EngineOptions&)
{
    return nullptr;
}

}  // namespace tracklab::engine
