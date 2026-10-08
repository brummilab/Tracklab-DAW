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
        auto& open = openEdits();
        if (std::find(open.begin(), open.end(), &edit) != open.end())
            return;  // joined: the outer Transaction owns inhibitor, name and rollback

        // The inhibitor first: from here on Tracktion's 350 ms timer must not start a new transaction.
        inhibitor.emplace(edit);
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
};

Transaction::Transaction(tracktion::Edit& edit, const std::string& nameDe) : impl(std::make_unique<Impl>(edit, nameDe))
{
}

Transaction::~Transaction() = default;

void Transaction::rollback()
{
    // A joined Transaction cannot roll back on its own: its changes belong to the outer one.
    if (!impl->inhibitor)
        return;
    // Only the current transaction: earlier steps and the redo stack stay as they were.
    impl->edit.getUndoManager().undoCurrentTransactionOnly();
}

}  // namespace tracklab::core
