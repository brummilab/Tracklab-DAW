#include "project/project_format.h"

// STUB (test-writer, M1-04): the implementer replaces this file. The functions only exist so that the tests compile.
namespace tracklab::project
{

int formatVersionOf(const juce::ValueTree&)
{
    return 0;
}

const std::vector<MigrationStep>& builtInMigrationSteps()
{
    static const std::vector<MigrationStep> none;
    return none;
}

MigrationOutcome migrateState(juce::ValueTree&, const std::vector<MigrationStep>&, int)
{
    return MigrationOutcome{.ok = false, .code = "not_implemented", .message = "stub"};
}

}  // namespace tracklab::project
