#pragma once
#include "xdr/SuperBlockChain-ledger-entries.h"
#include "xdr/SuperBlockChain-ledger.h"
#include "xdr/SuperBlockChain-overlay.h"
#include "xdr/SuperBlockChain-transaction.h"
#include "xdr/SuperBlockChain-types.h"

namespace snb
{

std::string xdr_printer(const PublicKey& pk);
}
