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
        redoAtBegin = edit.getUndoManager().getRedoDescriptions();
        edit.getUndoManager().beginNewTransaction(juce::String::fromUTF8(nameDe.c_str()));
        open.push_back(&edit);
    }

    ~Impl()
    {
        if (!inhibitor)
            return;
        auto& open = openEdits();
        open.erase(std::remove(open.begin(), open.end(), &edit), open.end());
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    tracktion::Edit& edit;
    std::optional<tracktion::Edit::UndoTransactionInhibitor> inhibitor;  // empty = joined an outer Transaction
    juce::StringArray redoAtBegin;  // names of the redo steps when the Transaction began (rollback check)
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
    auto& manager = impl->edit.getUndoManager();
    // Only the current transaction: earlier steps stay as they were. False = nothing was changed, nothing to do.
    if (!manager.undoCurrentTransactionOnly())
        return;

    // JUCE 9.0.3 puts its "stash" of former redo steps back on the redo stack here. The stash is replaced only when a
    // step is performed while redo steps exist, otherwise it keeps steps that were discarded long ago (or that
    // clearUndoHistory() dropped), and they would come back as a redo that no longer fits the state. There is no
    // public call that removes redo steps alone. So: if the redo stack is not exactly what it was when the
    // Transaction began (right stash, or none needed), the stash was stale and the only safe way out is to drop the
    // whole history. The Edit state is already restored at this point.
    if (manager.getRedoDescriptions() != impl->redoAtBegin)
    {
        manager.clearUndoHistory();
        manager.beginNewTransaction();
    }
}

}  // namespace tracklab::core
