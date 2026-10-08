#include "test_support.h"

#include <atomic>
#include <cstdio>

namespace tracklab_test
{

namespace
{
std::atomic<int> g_skipped{0};
juce::File g_engineTemp;
}  // namespace

void skipTest(const juce::String& reason)
{
    std::fprintf(stderr, "SKIPPED: %s\n", reason.toRawUTF8());
    ++g_skipped;
}

int skippedCount()
{
    return g_skipped.load();
}

const juce::File& engineTempDirectory()
{
    return g_engineTemp;
}

void setEngineTempDirectory(const juce::File& folder)
{
    g_engineTemp = folder;
}

ScopedTempDir::ScopedTempDir() : folder(juce::File::createTempFile("tracklab-test"))
{
    folder.createDirectory();
}

ScopedTempDir::~ScopedTempDir()
{
    folder.setReadOnly(false, true);
    folder.deleteRecursively();
}

juce::String snapshotOf(const juce::File& dir)
{
    if (!dir.isDirectory())
        return "<missing>";

    juce::StringArray lines;
    for (const auto& entry : juce::RangedDirectoryIterator(dir, true, "*", juce::File::findFilesAndDirectories))
    {
        const auto file = entry.getFile();
        lines.add(file.getRelativePathFrom(dir) + "|" + juce::String(file.getSize()) + "|" +
                  juce::String(file.getLastModificationTime().toMilliseconds()));
    }
    lines.sort(false);
    return lines.joinIntoString("\n");
}

}  // namespace tracklab_test
