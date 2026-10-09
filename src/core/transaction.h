// Undo transaction (M1-03, DESIGN Rev 3 section 3 "Undo"): one gesture / macro / Claude turn = one undo step.
#pragma once

#include "core/edit_context.h"

#include <memory>
#include <string>

namespace tracklab::core
{

/** RAII: holds an Edit::UndoTransactionInhibitor (Tracktion's 350 ms timer must not cut the transaction) and calls
    UndoManager::beginNewTransaction(nameDe) on construction. Every change that reaches the Edit's UndoManager while
    the object lives belongs to ONE undo step named `nameDe` (UTF-8). A transaction without any change leaves no entry.
    A Transaction created while another one on the same Edit is alive (on this thread) joins it: no new step, no
    name of its own, and its rollback() does nothing: only the outer Transaction can take the changes back.
    Message thread only. The Edit has to outlive the Transaction. */
class Transaction
{
public:
    Transaction(tracktion::Edit& edit, const std::string& nameDe);
    ~Transaction();
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    /** Takes back everything done since construction: the Edit state is as before and no undo entry remains. The redo
        steps that existed when the Transaction began are back too (UndoManager::undoCurrentTransactionOnly), with one
        exception forced by JUCE: its stash of former redo steps can be stale, and then the undo history is cleared
        completely (see transaction.cpp) rather than let a redo come back that no longer fits the state. A redo that
        existed at the beginning is never lost by a rollback, one that did not exist is never created.
        Does nothing if nothing was changed, and nothing for a Transaction that joined an outer one.
        It ends the transaction: later changes before the destructor are not part of it and not rolled back, and a
        second rollback() does nothing. */
    void rollback();

    /** True while a Transaction (not a joined one) is alive on `edit` on this thread. edit.undo/edit.redo check this:
        UndoManager::undo() starts a new transaction and would cut the open one in two. */
    static bool isOpen(const tracktion::Edit& edit);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

}  // namespace tracklab::core
