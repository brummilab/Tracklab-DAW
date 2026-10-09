// Undo depth (M1-03): the Tracklab edit factory sets Edit::Options::numUndoLevelsToStore to 200 (Tracktion: 30).
//
// Note on the numbers: juce::UndoManager keeps at least `numUndoLevelsToStore` transactions and drops the oldest ones
// only when the stored actions exceed 1000 units per level. A transaction of a single property change is far below
// that, so the tests below fill every transaction with 100 changes; then the limit is exactly the number of levels.
#include "core/undo_fixture.h"

namespace
{

using namespace tracklab_test::undo;

constexpr int changesPerTransaction = 100;

/** `count` transactions, each with `changesPerTransaction` property changes (never to the value a property has). */
void fillHistory(UndoFixture& f, int count)
{
    auto& um = f.undoManager();
    auto node = testNode(*f.edit);
    for (int step = 0; step < count; ++step)
    {
        um.beginNewTransaction("Schritt " + juce::String(step));
        for (int k = 0; k < changesPerTransaction; ++k)
            node.setProperty(juce::Identifier("p" + juce::String(k)), step + 1, &um);
    }
}

/** Undoes until nothing is left; returns the number of steps that could be undone. */
int countUndoSteps(UndoFixture& f)
{
    int steps = 0;
    while (f.undoManager().canUndo())
    {
        REQUIRE(f.undoManager().undo());
        ++steps;
        REQUIRE(steps <= 100000);
    }
    return steps;
}

}  // namespace

TEST_SUITE("core")
{
    TEST_CASE("the default undo depth of Tracklab is 200 levels")
    {
        CHECK(tracklab::engine::defaultUndoLevels == 200);
        CHECK(tracklab::engine::EditOptions{}.undoLevels == 200);
    }

    TEST_CASE("an Edit from the factory keeps 200 undo steps, not Tracktion's 30")
    {
        UndoFixture f;  // default EditOptions
        fillHistory(f, 250);
        settle(*f.edit);

        CHECK(countUndoSteps(f) == 200);
    }

    TEST_CASE("the undo depth is adjustable through EditOptions")
    {
        for (const int levels : {5, 50, 300})
        {
            CAPTURE(levels);
            UndoFixture f(levels);
            fillHistory(f, levels + 20);
            settle(*f.edit);

            CHECK(countUndoSteps(f) == levels);
        }
    }

    TEST_CASE("fewer steps than the depth are all kept")
    {
        UndoFixture f;
        fillHistory(f, 40);
        settle(*f.edit);

        CHECK(countUndoSteps(f) == 40);
    }

    TEST_CASE("a fresh Edit from the factory has an empty undo history")
    {
        auto engine = tracklab::engine::createEngine(tracklab_test::testOptions());
        auto edit = tracklab::engine::createEdit(*engine);
        REQUIRE(edit != nullptr);
        pumpMessageLoop(30);
        settle(*edit);

        CHECK_FALSE(edit->getUndoManager().canUndo());
        CHECK_FALSE(edit->getUndoManager().canRedo());
    }
}
