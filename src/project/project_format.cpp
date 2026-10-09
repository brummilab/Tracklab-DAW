#include "project/project_format.h"

#include <exception>
#include <string>
#include <utility>

namespace tracklab::project
{

int formatVersionOf(const juce::ValueTree& edit)
{
    return static_cast<int>(edit.getProperty(formatVersionProperty, 0));
}

const std::vector<MigrationStep>& builtInMigrationSteps()
{
    // v0 -> v1: files written before the version attribute existed (e.g. by the spikes) have exactly the content of
    // v1, only the attribute is missing; the framework sets it. Later format changes append their step here and raise
    // currentFormatVersion.
    static const std::vector<MigrationStep> steps = {MigrationStep{0, [](juce::ValueTree&) {}}};
    return steps;
}

namespace
{

MigrationOutcome failure(std::string_view code, std::string message, int from)
{
    return MigrationOutcome{
        .ok = false, .code = std::string(code), .message = std::move(message), .fromVersion = from, .toVersion = from};
}

const MigrationStep* findStep(const std::vector<MigrationStep>& steps, int fromVersion)
{
    for (const auto& step : steps)
        if (step.fromVersion == fromVersion)
            return &step;
    return nullptr;
}

}  // namespace

MigrationOutcome migrateState(juce::ValueTree& edit, const std::vector<MigrationStep>& steps, int targetVersion)
{
    const int from = formatVersionOf(edit);
    if (from > targetVersion)
        return failure(error_code::projectTooNew,
                       "The project has format version " + std::to_string(from) +
                           ", but this Tracklab only understands up to version " + std::to_string(targetVersion) +
                           ". Update Tracklab to open it.",
                       from);

    if (from == targetVersion)
        return MigrationOutcome{.ok = true, .code = {}, .message = {}, .fromVersion = from, .toVersion = from};

    // All or nothing: the steps run on a copy, `edit` is only touched when every step went through.
    auto work = edit.createCopy();
    for (int version = from; version < targetVersion; ++version)
    {
        const auto* step = findStep(steps, version);
        if (step == nullptr || !step->apply)
            return failure(error_code::migrationFailed,
                           "No migration step from format version " + std::to_string(version) + " to " +
                               std::to_string(version + 1) + " (project version " + std::to_string(from) + ").",
                           from);
        try
        {
            step->apply(work);
        }
        catch (const std::exception& error)
        {
            return failure(error_code::migrationFailed,
                           "Migration from format version " + std::to_string(version) + " to " +
                               std::to_string(version + 1) + " failed: " + error.what(),
                           from);
        }
        catch (...)
        {
            return failure(error_code::migrationFailed,
                           "Migration from format version " + std::to_string(version) + " to " +
                               std::to_string(version + 1) + " failed.",
                           from);
        }
        work.setProperty(formatVersionProperty, version + 1, nullptr);
    }

    // In place, so that handles to the EDIT node stay valid.
    edit.copyPropertiesAndChildrenFrom(work, nullptr);
    return MigrationOutcome{.ok = true, .code = {}, .message = {}, .fromVersion = from, .toVersion = targetVersion};
}

}  // namespace tracklab::project
