#pragma once

// Copyright 2017 SuperBlockChain Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "xdr/SuperBlockChain-ledger-entries.h"

#include <cstdint>

namespace snb
{

class Database;

struct NumberOfSubentries
{
    uint64_t inAccountsTable;
    uint64_t calculated;
};

int64_t sumOfBalances(Database& db);

NumberOfSubentries numberOfSubentries(AccountID const& accountID, Database& db);
}
