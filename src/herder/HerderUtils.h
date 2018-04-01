#pragma once

// Copyright 2017 SuperBlockChain Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "xdr/SuperBlockChain-types.h"
#include <vector>

namespace snb
{

struct SCPEnvelope;
struct SCPStatement;
struct SuperBlockChainValue;

std::vector<Hash> getTxSetHashes(SCPEnvelope const& envelope);
std::vector<SuperBlockChainValue> getSuperBlockChainValues(SCPStatement const& envelope);
}
