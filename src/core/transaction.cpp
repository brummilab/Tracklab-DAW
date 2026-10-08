#include "core/transaction.h"

#include <tracktion_engine/tracktion_engine.h>

namespace tracklab::core
{

struct Transaction::Impl
{
    tracktion::Edit& edit;
};

// STUB (test-writer, M1-03): the behaviour is implemented with the card.
Transaction::Transaction(tracktion::Edit& edit, const std::string&) : impl(std::make_unique<Impl>(Impl{edit})) {}

Transaction::~Transaction() = default;

void Transaction::rollback() {}

}  // namespace tracklab::core
