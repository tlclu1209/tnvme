/*
 * Copyright (c) 2011, Intel Corporation.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <boost/format.hpp>
#include <vector>
#include "kernelAPI.h"
#include "globals.h"
#include "io.h"


IO::IO()
{
}


IO::~IO()
{
}


void
IO::SendAndReapCmdFail(string grpName, string testName, uint16_t ms,
    SharedSQPtr sq, SharedCQPtr cq, SharedCmdPtr cmd, string qualify,
    bool verbose)
{
    uint32_t numCE;
    uint32_t isrCount;
    string work;
    uint16_t uniqueId;

    if ((numCE = cq->ReapInquiry(isrCount, true)) != 0) {
        cq->Dump(
            FileSystem::PrepDumpFile(grpName, testName, "cq",
            "notEmpty"), "Test assumption have not been met");
        throw FrmwkEx(HERE, "Require 0 CE's within CQ %d, not upheld, found %d",
            cq->GetQId(), numCE);
    }

    LOG_NRM("Send the cmd to hdw via SQ %d", sq->GetQId());
    sq->Send(cmd, uniqueId);
    if (verbose) {
        work = str(boost::format(
            "Just B4 ringing SQ %d doorbell, dump entire SQ") % sq->GetQId());
        sq->Dump(FileSystem::PrepDumpFile(grpName, testName,
            "sq." + cmd->GetName(), qualify), work);
    }
    sq->Ring();

    LOG_NRM("Wait for the CE to arrive in CQ %d", cq->GetQId());
    if (cq->ReapInquiryWaitSpecifyQ(ms, 1, numCE, isrCount) == true) {
        /* Found a CE...reap it then throw so that it gets cleared from CQ */
        if (verbose) {
            work = str(boost::format("Just B4 reaping CQ %d, dump entire CQ") %
                cq->GetQId());
            cq->Dump(FileSystem::PrepDumpFile(grpName, testName,
                "cq." + cmd->GetName(), qualify), work);
        }

        // throws if an error occurs
        ReapCEIgnore(cq, numCE, isrCount, grpName, testName, qualify);
        if (verbose) {
            cmd->Dump(FileSystem::PrepDumpFile(grpName, testName,
                cmd->GetName(), qualify), "A cmd's contents dumped");
        }

        throw FrmwkEx(HERE, "Command was successfully processed and its "
            "completion was reaped from the CQ");
    }
}

CEStat
IO::SendAndReapCmdIgnore(string grpName, string testName, uint16_t ms,
    SharedSQPtr sq, SharedCQPtr cq, SharedCmdPtr cmd, string qualify,
    bool verbose)
{
    std::vector<CEStat> localStatus;
    return SendAndReapCmd(grpName, testName, ms, sq, cq, cmd, qualify, verbose,
        localStatus, ReapCEIgnore);
}

CEStat
IO::SendAndReapCmdNot(string grpName, string testName, uint16_t ms,
    SharedSQPtr sq, SharedCQPtr cq, SharedCmdPtr cmd, string qualify,
    bool verbose, CEStat status)
{
    std::vector<CEStat> localStatus;
    localStatus.push_back(status);
    return SendAndReapCmdNot(grpName, testName, ms, sq, cq, cmd, qualify,
        verbose, localStatus);
}

CEStat
IO::SendAndReapCmdNot(string grpName, string testName, uint16_t ms,
    SharedSQPtr sq, SharedCQPtr cq, SharedCmdPtr cmd, string qualify,
    bool verbose, std::vector<CEStat> &status)
{
    return SendAndReapCmd(grpName, testName, ms, sq, cq, cmd, qualify, verbose,
        status, ReapCENot);
}

CEStat
IO::SendAndReapCmd(string grpName, string testName, uint16_t ms,
    SharedSQPtr sq, SharedCQPtr cq, SharedCmdPtr cmd, string qualify,
    bool verbose, CEStat status)
{
    std::vector<CEStat> localStatus;
    localStatus.push_back(status);
    return SendAndReapCmd(grpName, testName, ms, sq, cq, cmd, qualify,
        verbose, localStatus);
}


CEStat
IO::SendAndReapCmd(string grpName, string testName, uint16_t ms,
    SharedSQPtr sq, SharedCQPtr cq, SharedCmdPtr cmd, string qualify,
    bool verbose, std::vector<CEStat> &status)
{
    return SendAndReapCmd(grpName, testName, ms, sq, cq, cmd, qualify, verbose,
        status, ReapCE);
}

CEStat
IO::SendAndReapCmd(string grpName, string testName, uint16_t ms,
    SharedSQPtr sq, SharedCQPtr cq, SharedCmdPtr cmd, string qualify,
    bool verbose, std::vector<CEStat> &status,
    CEStat (*Reap)(SharedCQPtr, uint32_t, uint32_t &, string, string,
            string, std::vector<CEStat> &))
{
    uint32_t numCE;
    uint32_t isrCount;
    string work;
    uint16_t uniqueId;

    if ((numCE = cq->ReapInquiry(isrCount, true)) != 0) {
        cq->Dump(
            FileSystem::PrepDumpFile(grpName, testName, "cq",
            "notEmpty"), "Test assumption have not been met");
        throw FrmwkEx(HERE, "Require 0 CE's within CQ %d, not upheld, found %d",
            cq->GetQId(), numCE);
    }

    LOG_NRM("Send the cmd to hdw via SQ %d", sq->GetQId());
    sq->Send(cmd, uniqueId);
    if (verbose) {
        work = str(boost::format(
            "Just B4 ringing SQ %d doorbell, dump entire SQ") % sq->GetQId());
        sq->Dump(FileSystem::PrepDumpFile(grpName, testName,
            "sq." + cmd->GetName(), qualify), work);
    }
    sq->Ring();

    LOG_NRM("Wait for the CE to arrive in CQ %d", cq->GetQId());
    if (cq->ReapInquiryWaitSpecify(ms, 1, numCE, isrCount) == false) {
        work = str(boost::format(
            "Unable to see any CE's in CQ %d, dump entire CQ") % cq->GetQId());
        cq->Dump(
            FileSystem::PrepDumpFile(grpName, testName, "cq." + cmd->GetName(),
            qualify), work);
        throw FrmwkEx(HERE, work);
    } else if (numCE != 1) {
        work = str(boost::format(
            "Unable to see any CE's in CQ %d, dump entire CQ") % cq->GetQId());
        cq->Dump(
            FileSystem::PrepDumpFile(grpName, testName, "cq." + cmd->GetName(),
            qualify), work);
        throw FrmwkEx(HERE, "1 cmd caused %d CE's to arrive in CQ %d",
            numCE, cq->GetQId());
    }
    if (verbose) {
        work = str(boost::format("Just B4 reaping CQ %d, dump entire CQ") %
            cq->GetQId());
        cq->Dump(FileSystem::PrepDumpFile(grpName, testName,
            "cq." + cmd->GetName(), qualify), work);
    }

    // throws if an error occurs
    CEStat retStat =
        Reap(cq, numCE, isrCount, grpName, testName, qualify, status);
    if (verbose) {
        cmd->Dump(FileSystem::PrepDumpFile(grpName, testName,
            cmd->GetName(), qualify), "A cmd's contents dumped");
    }
    return retStat;
}


CEStat
IO::ReapCEIgnore(SharedCQPtr cq, uint32_t numCE, uint32_t &isrCount,
    string grpName, string testName, string qualify)
{
    return ProcessCE::GetCEStat(
        RetrieveCE(cq, numCE, isrCount, grpName, testName, qualify));
}


CEStat
IO::ReapCEIgnore(SharedCQPtr cq, uint32_t numCE, uint32_t &isrCount,
    string grpName, string testName, string qualify,
    std::vector<CEStat> &status)
{
    status = status;    // Suppress compiler error/warning
    return ReapCEIgnore(cq, numCE, isrCount, grpName, testName, qualify);
}


CEStat
IO::ReapCENot(SharedCQPtr cq, uint32_t numCE, uint32_t &isrCount,
    string grpName, string testName, string qualify, CEStat status)
{
    std::vector<CEStat> localStatus;
    localStatus.push_back(status);
    return ReapCENot(cq, numCE, isrCount, grpName, testName, qualify,
        localStatus);
}


CEStat
IO::ReapCENot(SharedCQPtr cq, uint32_t numCE, uint32_t &isrCount,
        string grpName, string testName, string qualify,
        std::vector<CEStat> &status)
{
    union CE ce = RetrieveCE(cq, numCE, isrCount, grpName, testName, qualify);

    if (status.empty()) {
        throw FrmwkEx(HERE,
            "Internal Programming Error; Must supply >= 1 status");
    }

    // Search for 1 matching CEStat to match the device status
    for (size_t sIdx = 0; sIdx < status.size(); sIdx++) {
        if (ProcessCE::InvalidatePeek(ce, status[sIdx]) == false)
            throw FrmwkEx(HERE,
                "%d CeStat's compared to device status, and match found",
                status.size());
    }

    return ProcessCE::GetCEStat(ce);
}


CEStat
IO::ReapCE(SharedCQPtr cq, uint32_t numCE, uint32_t &isrCount,
    string grpName, string testName, string qualify, CEStat status)
{
    std::vector<CEStat> localStatus;
    localStatus.push_back(status);
    return ReapCE(cq, numCE, isrCount, grpName, testName, qualify, localStatus);
}


CEStat
IO::ReapCE(SharedCQPtr cq, uint32_t numCE, uint32_t &isrCount,
    string grpName, string testName, string qualify,
    std::vector<CEStat> &status)
{
    union CE ce = RetrieveCE(cq, numCE, isrCount, grpName, testName, qualify);

    if (status.empty()) {
        throw FrmwkEx(HERE,
            "Internal Programming Error; Must supply >= 1 status");
    }

    // Search for 1 matching CEStat to match the device status
    for (size_t sIdx = 0; sIdx < status.size(); sIdx++) {
        if (ProcessCE::ValidatePeek(ce, status[sIdx]) == true)
            return status[sIdx];
    }

    throw FrmwkEx(HERE,
        "%d CeStat's compared to device status, but no match found",
        status.size());
}


union CE
IO::RetrieveCE(SharedCQPtr cq, uint32_t numCE, uint32_t &isrCount,
            string grpName, string testName, string qualify)
{
    struct nvme_gen_cq cqMetrics = cq->GetQMetrics();
    uint32_t numReaped = AttemptRetrieveCE(cq, numCE, isrCount, &cqMetrics);
    string work;

    if (numReaped != 1) {
        work = str( boost::format("Verified CE's exist, desired %d, reaped %d")
                % numCE % numReaped);
        cq->Dump(FileSystem::PrepDumpFile(grpName, testName, "cq.error",
            qualify), work);
        throw FrmwkEx(HERE, work);
    }
    return cq->PeekCE(cqMetrics.head_ptr);
}


uint32_t
IO::AttemptRetrieveCE(SharedCQPtr cq, uint32_t numCE, uint32_t &isrCount,
    struct nvme_gen_cq *cqMetrics)
{
    uint32_t ceRemain;
    string work;

    LOG_NRM("The CQ's metrics before reaping holds head_ptr");
    KernelAPI::LogCQMetrics(*cqMetrics);

    LOG_NRM("Reaping CE from CQ %d, requires memory to hold CE", cq->GetQId());
    SharedMemBufferPtr ceMem = SharedMemBufferPtr(new MemBuffer());

    return cq->Reap(ceRemain, ceMem, isrCount, numCE, true);
}
