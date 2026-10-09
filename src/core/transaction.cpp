#include "core/transaction.h"

#include <tracktion_engine/tracktion_engine.h>

#include <algorithm>
#include <optional>
#include <vector>

namespace tracklab::core
{

namespace
{

// Edits that have an open (outermost) Transaction on this thread. Transactions live on the message thread only, so
// thread_local is not a synchronisation device, it just keeps this bookkeeping out of shared global state.
std::vector<const tracktion::Edit*>& openEdits()
{
    thread_local std::vector<const tracktion::Edit*> edits;
    return edits;
}

}  // namespace

struct Transaction::Impl
{
    Impl(tracktion::Edit& e, const std::string& nameDe) : edit(e)
    {
        // Undo state belongs to the message thread; a Transaction from anywhere else would race the UndoManager.
        jassert(juce::MessageManager::existsAndIsCurrentThread());

        auto& open = openEdits();
        if (std::find(open.begin(), open.end(), &edit) != open.end())
            return;  // joined: the outer Transaction owns inhibitor, name and rollback

        // The inhibitor first: from here on Tracktion's 350 ms timer must not start a new transaction.
        inhibitor.emplace(edit);
#if JUCE_DEBUG
        redoAtBegin = edit.getUndoManager().getRedoDescriptions();
#endif
        edit.getUndoManager().beginNewTransaction(juce::String::fromUTF8(nameDe.c_str()));
        open.push_back(&edit);
    }

    ~Impl()
    {
        if (!inhibitor)
            return;
#if JUCE_DEBUG
        assertRedoStackUntouched();
#endif
        auto& open = openEdits();
        open.erase(std::remove(open.begin(), open.end(), &edit), open.end());
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

#if JUCE_DEBUG
    /** Rollback hands the redo steps back through JUCE's stash, which only works if the redo stack is changed by the
        first write of this Transaction alone: JUCE then moves the redo steps that existed at the beginning into its
        stash, so the stack is either still what it was (nothing written yet) or empty (stashed). Anything else means
        an undo()/redo() or a second manager user cut into the open transaction (edit.undo/edit.redo refuse that via
        isOpen). Debug builds only (the whole check, also the copy of the names at the beginning, is compiled out in a
        release build): it costs a string array per Transaction, and in the destructor it reads the UndoManager, so
        it is the one place that would trip over an Edit destroyed before its Transaction. That breaks the documented
        contract (transaction.h) anyway, and a debug build should say so here, not crash later. */
    void assertRedoStackUntouched() const
    {
        const auto now = edit.getUndoManager().getRedoDescriptions();
        jassert(now.isEmpty() || now == redoAtBegin);
        juce::ignoreUnused(now);
    }
#endif

    tracktion::Edit& edit;
    std::optional<tracktion::Edit::UndoTransactionInhibitor> inhibitor;  // empty = joined an outer Transaction
#if JUCE_DEBUG
    juce::StringArray redoAtBegin;  // names of the redo steps when the Transaction began (debug check only)
#endif
};

Transaction::Transaction(tracktion::Edit& edit, const std::string& nameDe) : impl(std::make_unique<Impl>(edit, nameDe))
{
}

Transaction::~Transaction() = default;

bool Transaction::isOpen(const tracktion::Edit& edit)
{
    const auto& open = openEdits();
    return std::find(open.begin(), open.end(), &edit) != open.end();
}

void Transaction::rollback()
{
    // A joined Transaction cannot roll back on its own: its changes belong to the outer one.
    if (!impl->inhibitor)
        return;
#if JUCE_DEBUG
    impl->assertRedoStackUntouched();
#endif
    // Only the current transaction: earlier steps stay as they were. The redo steps that existed at the beginning come
    // back from JUCE's stash; that this stash is not stale depends on the Tracklab patch in
    // third_party/patches/juce-undomanager-stale-stash.patch (applied by cmake/TracklabDeps.cmake, E49). The return
    // value is false if nothing was changed, then there is nothing to do.
    impl->edit.getUndoManager().undoCurrentTransactionOnly();
}

}  // namespace tracklab::core
