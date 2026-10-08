// Test helper: an environment variable set for a scope (JUCE reads $HOME and the cache paths from the environment).
#pragma once

#include <juce_core/juce_core.h>

#include <cstdlib>
#include <string>

namespace tracklab_test
{

/** Sets an environment variable for the life of the object (nullptr = unset) and restores it afterwards. */
class ScopedEnv
{
public:
    ScopedEnv(const char* variable, const char* value) : name(variable)
    {
        if (const char* old = std::getenv(name))
        {
            hadValue = true;
            oldValue = old;
        }
        apply(value);
    }

    ~ScopedEnv() { apply(hadValue ? oldValue.c_str() : nullptr); }

    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;

private:
    const char* name;
    bool hadValue = false;
    std::string oldValue;

    void apply(const char* value) const
    {
#if JUCE_WINDOWS
        _putenv_s(name, value != nullptr ? value : "");
#else
        if (value != nullptr)
            setenv(name, value, 1);
        else
            unsetenv(name);
#endif
    }
};

}  // namespace tracklab_test
