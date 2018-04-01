// Copyright 2015 SuperBlockChain Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "catchup/ApplyBucketsWork.h"
#include "bucket/Bucket.h"
#include "bucket/BucketApplicator.h"
#include "bucket/BucketList.h"
#include "bucket/BucketManager.h"
#include "crypto/Hex.h"
#include "history/HistoryArchive.h"
#include "historywork/Progress.h"
#include "ledger/LedgerManager.h"
#include "main/Application.h"
#include "util/format.h"
#include "util/make_unique.h"
#include <medida/meter.h>
#include <medida/metrics_registry.h>

namespace snb
{

ApplyBucketsWork::ApplyBucketsWork(
    Application& app, WorkParent& parent,
    std::map<std::string, std::shared_ptr<Bucket>>& buckets,
    HistoryArchiveState& applyState,
    LedgerHeaderHistoryEntry const& firstVerified)
    : Work(app, parent, std::string("apply-buckets"))
    , mBuckets(buckets)
    , mApplyState(applyState)
    , mFirstVerified(firstVerified)
    , mApplying(false)
    , mLevel(BucketList::kNumLevels - 1)
    , mBucketApplyStart(app.getMetrics().NewMeter(
          {"history", "bucket-apply", "start"}, "event"))
    , mBucketApplySuccess(app.getMetrics().NewMeter(
          {"history", "bucket-apply", "success"}, "event"))
    , mBucketApplyFailure(app.getMetrics().NewMeter(
          {"history", "bucket-apply", "failure"}, "event"))
{
    // Consistency check: LCL should be in the _past_ from firstVerified,
    // since we're about to clobber a bunch of DB state with new buckets
    // held in firstVerified's state.
    auto lcl = app.getLedgerManager().getLastClosedLedgerHeader();
    if (firstVerified.header.ledgerSeq < lcl.header.ledgerSeq)
    {
        throw std::runtime_error(
            fmt::format("ApplyBucketsWork applying ledger earlier than local "
                        "LCL: {:s} < {:s}",
                        LedgerManager::ledgerAbbrev(firstVerified),
                        LedgerManager::ledgerAbbrev(lcl)));
    }
}

ApplyBucketsWork::~ApplyBucketsWork()
{
}

BucketList&
ApplyBucketsWork::getBucketList()
{
    return mApp.getBucketManager().getBucketList();
}

BucketLevel&
ApplyBucketsWork::getBucketLevel(size_t level)
{
    return getBucketList().getLevel(level);
}

std::shared_ptr<Bucket>
ApplyBucketsWork::getBucket(std::string const& hash)
{
    std::shared_ptr<Bucket> b;
    if (hash.find_first_not_of('0') == std::string::npos)
    {
        b = std::make_shared<Bucket>();
    }
    else
    {
        auto i = mBuckets.find(hash);
        if (i != mBuckets.end())
        {
            b = i->second;
        }
        else
        {
            b = mApp.getBucketManager().getBucketByHash(hexToBin256(hash));
        }
    }
    assert(b);
    return b;
}

void
ApplyBucketsWork::onReset()
{
    mLevel = BucketList::kNumLevels - 1;
    mApplying = false;
    mSnapBucket.reset();
    mCurrBucket.reset();
    mSnapApplicator.reset();
    mCurrApplicator.reset();
}

void
ApplyBucketsWork::onStart()
{
    auto& level = getBucketLevel(mLevel);
    HistoryStateBucket& i = mApplyState.currentBuckets.at(mLevel);
    if (mApplying || i.snap != binToHex(level.getSnap()->getHash()))
    {
        mSnapBucket = getBucket(i.snap);
        mSnapApplicator =
            make_unique<BucketApplicator>(mApp.getDatabase(), mSnapBucket);
        CLOG(DEBUG, "History") << "ApplyBuckets : starting level[" << mLevel
                               << "].snap = " << i.snap;
        mApplying = true;
        mBucketApplyStart.Mark();
    }
    if (mApplying || i.curr != binToHex(level.getCurr()->getHash()))
    {
        mCurrBucket = getBucket(i.curr);
        mCurrApplicator =
            make_unique<BucketApplicator>(mApp.getDatabase(), mCurrBucket);
        CLOG(DEBUG, "History") << "ApplyBuckets : starting level[" << mLevel
                               << "].curr = " << i.curr;
        mApplying = true;
        mBucketApplyStart.Mark();
    }
}

void
ApplyBucketsWork::onRun()
{
    if (mSnapApplicator && *mSnapApplicator)
    {
        mSnapApplicator->advance();
    }
    else if (mCurrApplicator && *mCurrApplicator)
    {
        mCurrApplicator->advance();
    }
    scheduleSuccess();
}

Work::State
ApplyBucketsWork::onSuccess()
{
    mApp.getCatchupManager().logAndUpdateCatchupStatus(true);

    if ((mSnapApplicator && *mSnapApplicator) ||
        (mCurrApplicator && *mCurrApplicator))
    {
        return WORK_RUNNING;
    }

    auto& level = getBucketLevel(mLevel);
    if (mSnapBucket)
    {
        level.setSnap(mSnapBucket);
        mBucketApplySuccess.Mark();
    }
    if (mCurrBucket)
    {
        level.setCurr(mCurrBucket);
        mBucketApplySuccess.Mark();
    }
    mSnapBucket.reset();
    mCurrBucket.reset();
    mSnapApplicator.reset();
    mCurrApplicator.reset();

    HistoryStateBucket& i = mApplyState.currentBuckets.at(mLevel);
    level.setNext(i.next);

    if (mLevel != 0)
    {
        --mLevel;
        CLOG(DEBUG, "History") << "ApplyBuckets : starting next level: "
                               << mLevel;
        return WORK_PENDING;
    }

    CLOG(DEBUG, "History") << "ApplyBuckets : done, restarting merges";
    getBucketList().restartMerges(mApp, mFirstVerified.header.ledgerSeq);
    return WORK_SUCCESS;
}

void
ApplyBucketsWork::onFailureRetry()
{
    mBucketApplyFailure.Mark();
    Work::onFailureRetry();
}

void
ApplyBucketsWork::onFailureRaise()
{
    mBucketApplyFailure.Mark();
    Work::onFailureRaise();
}
}
