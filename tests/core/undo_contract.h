// Undo/redo contract test helper (M1-03, DESIGN Rev 3 section 3 "Undo"), generic for ALL registered undoable commands:
//
//   state := normalised(Edit); run the command through the registry; undo once -> state again; redo once -> the state
//   after the command. Besides that, the command must have left exactly ONE undo step named after its titleDe.
//
// A registry must pass checkAllUndoableCommands() with a sample for every `undoable` command it holds; a command
// without a sample is a violation (so a new undoable command cannot slip past the contract). Commands of later cards
// (M1-04 ...) add their samples to the table of their test; nothing else changes.
//
// The helper returns a Report instead of failing by itself, so that the negative tests can check that a command which
// writes past the UndoManager is caught. requireContract() turns a Report into doctest failures.
#pragma once

#include "core/undo_fixture.h"

#include <algorithm>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace tracklab_test::undo
{

/** A set of params that makes the command change the project, plus an optional preparation of the Edit (not
    undoable, not part of the checked step; the undo history is cleared after it). */
struct Sample
{
    Json params = Json::object();
    std::function<void(te::Edit&)> arrange;
};

using Samples = std::map<std::string, std::vector<Sample>>;  ///< command id -> samples

/** Properties that change on their own (clocks, user name) and are not part of the project content. */
inline std::vector<juce::Identifier> volatileProperties()
{
    return {"modifiedBy", "lastSignificantChange"};
}

namespace detail
{
inline void removeProperties(juce::ValueTree& tree, const std::vector<juce::Identifier>& names)
{
    for (const auto& name : names)
        tree.removeProperty(name, nullptr);
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto child = tree.getChild(i);
        removeProperties(child, names);
    }
}
}  // namespace detail

/** The Edit state as XML text with the volatile properties removed (on a copy; the Edit is not touched). */
inline std::string normalisedState(te::Edit& edit, const std::vector<juce::Identifier>& ignored = volatileProperties())
{
    auto copy = edit.state.createCopy();
    detail::removeProperties(copy, ignored);
    return copy.toXmlString().toStdString();
}

struct Report
{
    std::vector<std::string> problems;
    bool ok() const { return problems.empty(); }

    void add(const std::string& commandId, const std::string& what) { problems.push_back(commandId + ": " + what); }
    std::string text() const
    {
        std::ostringstream out;
        for (const auto& p : problems)
            out << "\n  - " << p;
        return out.str();
    }
};

namespace detail
{
/** The first line that differs, for the message. */
inline std::string firstDifference(const std::string& expected, const std::string& actual)
{
    std::istringstream a(expected), b(actual);
    std::string la, lb;
    while (true)
    {
        const bool ga = static_cast<bool>(std::getline(a, la));
        const bool gb = static_cast<bool>(std::getline(b, lb));
        if (!ga && !gb)
            return "(no difference found)";
        if (!ga || !gb || la != lb)
            return "expected line `" + (ga ? la : std::string("<end>")) + "`, got `" +
                   (gb ? lb : std::string("<end>")) + "`";
    }
}
}  // namespace detail

/** The contract of one command with one sample, on `edit` (whose UndoManager is cleared first). Leaves the Edit in the
    state before the command and the command on the redo stack. */
inline void checkCommand(Report& report, const CommandRegistry& registry, te::Edit& edit, const std::string& id,
                         const Sample& sample)
{
    const Command* command = registry.find(id);
    if (command == nullptr)
    {
        report.add(id, "not registered");
        return;
    }

    auto& um = edit.getUndoManager();
    if (sample.arrange)
        sample.arrange(edit);
    settle(edit);
    um.clearUndoHistory();
    um.beginNewTransaction();
    const std::string before = normalisedState(edit);

    const auto result = registry.execute(id, sample.params);
    if (!result.ok)
    {
        report.add(id, "the command failed with " + result.error.code + ": " + result.error.message);
        return;
    }
    settle(edit);
    const std::string afterCommand = normalisedState(edit);
    if (afterCommand == before)
    {
        report.add(id, "the sample changed nothing; give the command params that change the project");
        return;
    }

    const auto names = toStd(um.getUndoDescriptions());
    if (names.size() != 1)
        report.add(id, "expected exactly one undo step, found " + std::to_string(names.size()));
    else if (names.front() != command->titleDe)
        report.add(id,
                   "the undo step is named `" + names.front() + "`, expected the titleDe `" + command->titleDe + "`");

    if (!um.undo())
    {
        report.add(id, "undo is not possible after the command (nothing was recorded in the UndoManager)");
        return;
    }
    settle(edit);
    if (const auto restored = normalisedState(edit); restored != before)
    {
        report.add(id, "undo did not restore the state: " + detail::firstDifference(before, restored));
        return;
    }

    if (!um.redo())
    {
        report.add(id, "redo is not possible after undo");
        return;
    }
    settle(edit);
    if (const auto redone = normalisedState(edit); redone != afterCommand)
    {
        report.add(id, "redo did not repeat the command: " + detail::firstDifference(afterCommand, redone));
        return;
    }

    // Back to the start, so that the next sample begins from a known state.
    um.undo();
    settle(edit);
}

/** The contract for every `undoable` command of the registry, with every sample of the table. A command without a
    sample is reported. Samples of ids that are not registered are reported too (stale table). */
inline Report checkAllUndoableCommands(const CommandRegistry& registry, te::Edit& edit, const Samples& samples)
{
    Report report;
    for (const Command* command : registry.list())
    {
        if (!command->flags.undoable)
            continue;
        const auto it = samples.find(command->id);
        if (it == samples.end() || it->second.empty())
        {
            report.add(command->id, "undoable command without a contract sample");
            continue;
        }
        for (const auto& sample : it->second)
            checkCommand(report, registry, edit, command->id, sample);
    }
    for (const auto& [id, list] : samples)
        if (!registry.contains(id))
            report.add(id, "sample for a command that is not registered");
    return report;
}

/** Fails the current doctest test case for every problem of the report. */
inline void requireContract(const Report& report)
{
    INFO("undo/redo contract violations:" << report.text());
    CHECK(report.ok());
}

//==============================================================================
/** Samples for the well-behaved test commands of undo_fixture.h. */
inline Samples goodTestSamples()
{
    Samples samples;
    samples["test.set_property"] = {Sample{Json{{"name", "alpha"}, {"value", 7}}, {}},
                                    // the property exists already: undo has to bring back the old value
                                    Sample{Json{{"name", "alpha"}, {"value", 8}},
                                           [](te::Edit& e) { testNode(e).setProperty("alpha", 3, nullptr); }}};
    samples["test.add_child"] = {Sample{Json{{"type", "CHILD"}}, {}}};
    samples["test.set_many"] = {Sample{Json{{"count", 5}, {"round", 1}}, {}}};
    samples["test.set_two_with_pause"] = {Sample{Json::object(), {}}};
    samples["test.macro"] = {Sample{Json::object(), {}}};
    return samples;
}

}  // namespace tracklab_test::undo
