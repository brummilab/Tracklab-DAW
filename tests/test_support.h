// Shared helpers of the Tracklab tests: doctest include, skip handling, temporary folders.
#pragma once

#include <juce_core/juce_core.h>

#include <doctest.h>

namespace tracklab_test
{

/** Registers a skipped test: prints the reason; the process then exits with code 77 (CTest "skipped", never
    "passed") unless something failed. */
void skipTest(const juce::String& reason);

/** Number of skipTest() calls so far. */
int skippedCount();

/** A fresh, empty temporary folder, deleted (recursively, also if it was made read-only) on destruction. */
class ScopedTempDir
{
public:
    ScopedTempDir();
    ~ScopedTempDir();

    ScopedTempDir(const ScopedTempDir&) = delete;
    ScopedTempDir& operator=(const ScopedTempDir&) = delete;

    const juce::File& dir() const noexcept { return folder; }

private:
    juce::File folder;
};

/** A description of the files below `dir` (relative path, size, modification time) for before/after comparisons.
    A missing folder yields "<missing>". */
juce::String snapshotOf(const juce::File& dir);

}  // namespace tracklab_test
