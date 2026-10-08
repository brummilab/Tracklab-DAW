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
    Message thread only. The Edit has to outlive the Transaction. */
class Transaction
{
public:
    Transaction(tracktion::Edit& edit, const std::string& nameDe);
    ~Transaction();
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    /** Takes back everything done since construction: the Edit state is as before, no undo entry remains, and the
        redo stack is as it was before the transaction (UndoManager::undoCurrentTransactionOnly). Does nothing if
        nothing was changed. */
    void rollback();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

}  // namespace tracklab::core
