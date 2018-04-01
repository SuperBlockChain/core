#pragma once

// Copyright 2017 SuperBlockChain Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "invariant/InvariantManager.h"
#include "util/make_unique.h"
#include <map>
#include <vector>

namespace snb
{

class InvariantManagerImpl : public InvariantManager
{
    std::map<std::string, std::shared_ptr<Invariant>> mInvariants;
    std::vector<std::shared_ptr<Invariant>> mEnabled;

  public:
    InvariantManagerImpl();

    virtual void checkOnLedgerClose(TxSetFramePtr const& txSet,
                                    LedgerDelta const& delta) override;

    virtual void
    registerInvariant(std::shared_ptr<Invariant> invariant) override;
    virtual void enableInvariant(std::string const& name) override;
};
}
