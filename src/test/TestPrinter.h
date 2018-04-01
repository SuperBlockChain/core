#pragma once

// Copyright 2017 SuperBlockChain Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "catchup/CatchupWork.h"
#include "lib/catch.hpp"
#include "xdr/SuperBlockChain-transaction.h"
#include "xdrpp/printer.h"

namespace snb
{

struct OfferState;
}

namespace Catch
{

template <> std::string toString(snb::ClaimOfferAtom const& coa);

template <> std::string toString(snb::Hash const& tr);

template <> std::string toString(snb::OfferEntry const& oe);

template <> std::string toString(snb::OfferState const& os);

template <> std::string toString(snb::TransactionResult const& tr);

template <> std::string toString(snb::CatchupRange const& cr);
}
