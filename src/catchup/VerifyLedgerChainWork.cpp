// Copyright 2015 SuperBlockChain Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "catchup/VerifyLedgerChainWork.h"
#include "history/FileTransferInfo.h"
#include "historywork/Progress.h"
#include "ledger/LedgerHeaderFrame.h"
#include "ledger/LedgerManager.h"
#include "main/Application.h"
#include "util/XDRStream.h"
#include <medida/meter.h>
#include <medida/metrics_registry.h>

namespace snb
{

static HistoryManager::VerifyHashStatus
verifyLedgerHistoryEntry(LedgerHeaderHistoryEntry const& hhe)
{
    LedgerHeaderFrame lFrame(hhe.header);
    Hash calculated = lFrame.getHash();
    if (calculated != hhe.hash)
    {
        CLOG(ERROR, "History")
            << "Bad ledger-header history entry: claimed ledger "
            << LedgerManager::ledgerAbbrev(hhe) << " actually hashes to "
            << hexAbbrev(calculated);
        return HistoryManager::VERIFY_HASH_BAD;
    }
    return HistoryManager::VERIFY_HASH_OK;
}

static HistoryManager::VerifyHashStatus
verifyLedgerHistoryLink(Hash const& prev, LedgerHeaderHistoryEntry const& curr)
{
    if (verifyLedgerHistoryEntry(curr) != HistoryManager::VERIFY_HASH_OK)
    {
        return HistoryManager::VERIFY_HASH_BAD;
    }
    if (prev != curr.header.previousLedgerHash)
    {
        CLOG(ERROR, "History")
            << "Bad hash-chain: " << LedgerManager::ledgerAbbrev(curr)
            << " wants prev hash " << hexAbbrev(curr.header.previousLedgerHash)
            << " but actual prev hash is " << hexAbbrev(prev);
        return HistoryManager::VERIFY_HASH_BAD;
    }
    return HistoryManager::VERIFY_HASH_OK;
}

VerifyLedgerChainWork::VerifyLedgerChainWork(
    Application& app, WorkParent& parent, TmpDir const& downloadDir,
    CheckpointRange range, bool verifyWithBufferedLedgers,
    LedgerHeaderHistoryEntry& firstVerified,
    LedgerHeaderHistoryEntry& lastVerified)
    : Work(app, parent, "verify-ledger-chain")
    , mDownloadDir(downloadDir)
    , mRange(range)
    , mCurrSeq(
          mApp.getHistoryManager().nextCheckpointLedger(mRange.first() + 1) - 1)
    , mVerifyWithBufferedLedgers(verifyWithBufferedLedgers)
    , mFirstVerified(firstVerified)
    , mLastVerified(lastVerified)
    , mVerifyLedgerSuccessOld(app.getMetrics().NewMeter(
          {"history", "verify-ledger", "success-old"}, "event"))
    , mVerifyLedgerSuccess(app.getMetrics().NewMeter(
          {"history", "verify-ledger", "success"}, "event"))
    , mVerifyLedgerFailureOvershot(app.getMetrics().NewMeter(
          {"history", "verify-ledger", "failure-overshot"}, "event"))
    , mVerifyLedgerFailureLink(app.getMetrics().NewMeter(
          {"history", "verify-ledger", "failure-link"}, "event"))
    , mVerifyLedgerChainSuccess(app.getMetrics().NewMeter(
          {"history", "verify-ledger-chain", "success"}, "event"))
    , mVerifyLedgerChainFailure(app.getMetrics().NewMeter(
          {"history", "verify-ledger-chain", "failure"}, "event"))
    , mVerifyLedgerChainFailureEnd(app.getMetrics().NewMeter(
          {"history", "verify-ledger-chain", "failure-end"}, "event"))
{
}

std::string
VerifyLedgerChainWork::getStatus() const
{
    if (mState == WORK_RUNNING)
    {
        std::string task = "verifying checkpoint";
        return fmtProgress(mApp, task, mRange.first(), mRange.last(), mCurrSeq);
    }
    return Work::getStatus();
}

void
VerifyLedgerChainWork::onReset()
{
    auto setLedger = mApp.getLedgerManager().getLastClosedLedgerHeader();
    if (setLedger.header.ledgerSeq < 2)
    {
        setLedger = {};
    }
    if (mFirstVerified.header.ledgerSeq != 0)
    {
        mFirstVerified = setLedger;
    }
    if (mLastVerified.header.ledgerSeq != 0)
    {
        mLastVerified = setLedger;
    }
    mCurrSeq =
        mApp.getHistoryManager().nextCheckpointLedger(mRange.first() + 1) - 1;
}

HistoryManager::VerifyHashStatus
VerifyLedgerChainWork::verifyHistoryOfSingleCheckpoint()
{
    FileTransferInfo ft(mDownloadDir, HISTORY_FILE_TYPE_LEDGER, mCurrSeq);
    XDRInputFileStream hdrIn;
    hdrIn.open(ft.localPath_nogz());

    LedgerHeaderHistoryEntry prev = mLastVerified;
    LedgerHeaderHistoryEntry curr;

    CLOG(DEBUG, "History") << "Verifying ledger headers from "
                           << ft.localPath_nogz() << " starting from ledger "
                           << LedgerManager::ledgerAbbrev(prev);

    while (hdrIn && hdrIn.readOne(curr))
    {
        if (prev.header.ledgerSeq == 0)
        {
            // When we have no previous state to connect up with
            // (eg. starting somewhere mid-chain like in CATCHUP_MINIMAL)
            // we just accept the first chain entry we see. We will
            // verify the chain continuously from here, and against the
            // live network.
            prev = curr;
            mVerifyLedgerSuccess.Mark();
            continue;
        }

        uint32_t expectedSeq = prev.header.ledgerSeq + 1;
        if (curr.header.ledgerSeq < expectedSeq)
        {
            // Harmless prehistory
            mVerifyLedgerSuccessOld.Mark();
            continue;
        }
        else if (curr.header.ledgerSeq > expectedSeq)
        {
            CLOG(ERROR, "History")
                << "History chain overshot expected ledger seq " << expectedSeq
                << ", got " << curr.header.ledgerSeq << " instead";
            mVerifyLedgerFailureOvershot.Mark();
            return HistoryManager::VERIFY_HASH_BAD;
        }
        if (verifyLedgerHistoryLink(prev.hash, curr) !=
            HistoryManager::VERIFY_HASH_OK)
        {
            mVerifyLedgerFailureLink.Mark();
            return HistoryManager::VERIFY_HASH_BAD;
        }
        mVerifyLedgerSuccess.Mark();
        prev = curr;
    }

    if (curr.header.ledgerSeq != mCurrSeq)
    {
        CLOG(ERROR, "History") << "History chain did not end with " << mCurrSeq;
        mVerifyLedgerChainFailureEnd.Mark();
        return HistoryManager::VERIFY_HASH_BAD;
    }

    auto status = HistoryManager::VERIFY_HASH_OK;
    if (mCurrSeq == mRange.last())
    {
        CLOG(INFO, "History") << "Verifying catchup candidate " << mCurrSeq
                              << " with LedgerManager";
        status = mApp.getLedgerManager().verifyCatchupCandidate(curr);
        if ((status == HistoryManager::VERIFY_HASH_UNKNOWN_RECOVERABLE ||
             status == HistoryManager::VERIFY_HASH_UNKNOWN_UNRECOVERABLE) &&
            !mVerifyWithBufferedLedgers)
        {
            CLOG(WARNING, "History")
                << "Accepting unknown-hash ledger due to manual catchup";
            status = HistoryManager::VERIFY_HASH_OK;
        }
    }

    if (status == HistoryManager::VERIFY_HASH_OK)
    {
        mVerifyLedgerChainSuccess.Mark();
        if (mCurrSeq == mRange.first())
        {
            mFirstVerified = curr;
        }
        mLastVerified = curr;
    }
    else
    {
        mVerifyLedgerChainFailure.Mark();
    }

    return status;
}

Work::State
VerifyLedgerChainWork::onSuccess()
{
    mApp.getCatchupManager().logAndUpdateCatchupStatus(true);

    if (mCurrSeq > mRange.last())
    {
        throw std::runtime_error("Verification overshot target ledger");
    }

    // This is in onSuccess rather than onRun, so we can force a FAILURE_RAISE.
    switch (verifyHistoryOfSingleCheckpoint())
    {
    case HistoryManager::VERIFY_HASH_OK:
        if (mCurrSeq == mRange.last())
        {
            CLOG(INFO, "History") << "History chain [" << mRange.first() << ","
                                  << mRange.last() << "] verified";
            return WORK_SUCCESS;
        }

        mCurrSeq += mApp.getHistoryManager().getCheckpointFrequency();
        return WORK_RUNNING;
    case HistoryManager::VERIFY_HASH_UNKNOWN_RECOVERABLE:
        CLOG(WARNING, "History")
            << "Catchup material verification inconclusive, retrying";
        return WORK_FAILURE_RETRY;
    case HistoryManager::VERIFY_HASH_BAD:
    case HistoryManager::VERIFY_HASH_UNKNOWN_UNRECOVERABLE:
        CLOG(ERROR, "History")
            << "Catchup material failed verification, propagating failure";
        return WORK_FAILURE_FATAL;
    default:
        assert(false);
        throw std::runtime_error("unexpected VerifyLedgerChainWork state");
    }
}
}
