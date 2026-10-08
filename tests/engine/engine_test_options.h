// Test helper: engine options that keep every folder of the engine out of the real user folders.
#pragma once

#include "engine/engine_factory.h"

#include "test_support.h"

namespace tracklab_test
{

/** Defaults of the factory (headless, in memory, private cache), with the private caches in the folder of the test
    run. Settings and persistent cache folders are set by the test itself when it needs them. */
inline tracklab::engine::EngineOptions testOptions()
{
    tracklab::engine::EngineOptions options;
    options.tempDirectory = engineTempDirectory();
    return options;
}

}  // namespace tracklab_test
