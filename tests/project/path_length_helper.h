// Helpers of the tests that depend on the length of the paths (M1-05): the folder of a test is somewhere under the
// system's temporary folder, so the longest project name that fits into 259 characters is computed from it, never
// assumed. Written out here on purpose, not taken from project_format.h: the formula is part of the contract.
#pragma once

#include "project/project_fixture.h"

#include <algorithm>
#include <string>

namespace tracklab_test::project
{

/** Windows' MAX_PATH counts UTF-16 code units. */
inline size_t utf16Units(const juce::String& text)
{
    size_t units = 0;
    for (const auto character : text)
        units += character > 0xFFFF ? 2 : 1;
    return units;
}

/** Length of the longest file of a project whose name has `nameUnits` UTF-16 units in `parent`: the temporary file of a
    backup that got the highest counter. Spelled out as a path, so that nothing but the layout is assumed. */
inline size_t worstCasePathUnits(const juce::File& parent, size_t nameUnits)
{
    const auto separator = juce::String(juce::File::getSeparatorString());
    const juce::String name(std::string(nameUnits, 'a'));
    const auto path = parent.getFullPathName() + separator + name + separator + "Backups" + separator + name +
                      ".20261009-123456-99_temp12345678.tracklab";
    return utf16Units(path);
}

/** The longest ASCII name (at most `cap` characters) that still fits into 259 characters in `parent`; 0 if none. */
inline size_t longestNameFitting(const juce::File& parent, size_t cap = 120)
{
    size_t best = 0;
    for (size_t length = 1; length <= cap && worstCasePathUnits(parent, length) <= 259; ++length)
        best = length;
    return best;
}

/** A folder below the fixture's root in which exactly a name of 60 characters is the longest that fits (when the root is
    not longer than that already), so that the boundary can be tested with names that also obey the 120 byte rule, even
    as 4-byte characters. */
inline juce::File deepParent(const ProjectFixture& f)
{
    auto parent = f.root();
    const auto current = worstCasePathUnits(parent, 60);
    if (current + 1 < 259)
    {
        // A folder name of `room - 1` characters adds `room` units (with its separator): the 60 character name then ends
        // exactly at 259.
        const auto room = 259 - current;
        parent = parent.getChildFile(juce::String(std::string(room - 1, 'x')));
        REQUIRE(parent.createDirectory().wasOk());
    }
    return parent;
}

}  // namespace tracklab_test::project
