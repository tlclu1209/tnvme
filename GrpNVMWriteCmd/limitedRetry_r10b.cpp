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

#include "limitedRetry_r10b.h"
#include "globals.h"
#include "grpDefs.h"
#include "../Queues/iocq.h"
#include "../Queues/iosq.h"
#include "../Utils/io.h"

namespace GrpNVMWriteCmd {


LimitedRetry_r10b::LimitedRetry_r10b(int fd, string mGrpName,
    string mTestName, ErrorRegs errRegs) :
    Test(fd, mGrpName, mTestName, SPECREV_10b, errRegs)
{
    // 63 chars allowed:     xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    mTestDesc.SetCompliance("revision 1.0b, section 6");
    mTestDesc.SetShort(     "Verify limited retry ");
    // No string size limit for the long description
    mTestDesc.SetLong(
        "Search for 1 of the following namspcs to run test. Find 1st bare "
        "namspc, or find 1st meta namspc, or find 1st E2E namspc. Issue 2 "
        "write cmds, one with DW12.LR set and the other with it reset, expect "
        "success for all.  Issue each cmd sending 1 block and approp "
        "supporting meta/E2E if necessary to the selected namspc at LBA 0.");
}


LimitedRetry_r10b::~LimitedRetry_r10b()
{
    ///////////////////////////////////////////////////////////////////////////
    // Allocations taken from the heap and not under the control of the
    // RsrcMngr need to be freed/deleted here.
    ///////////////////////////////////////////////////////////////////////////
}


LimitedRetry_r10b::
LimitedRetry_r10b(const LimitedRetry_r10b &other) : Test(other)
{
    ///////////////////////////////////////////////////////////////////////////
    // All pointers in this object must be NULL, never allow shallow or deep
    // copies, see Test::Clone() header comment.
    ///////////////////////////////////////////////////////////////////////////
}


LimitedRetry_r10b &
LimitedRetry_r10b::operator=(const LimitedRetry_r10b &other)
{
    ///////////////////////////////////////////////////////////////////////////
    // All pointers in this object must be NULL, never allow shallow or deep
    // copies, see Test::Clone() header comment.
    ///////////////////////////////////////////////////////////////////////////
    Test::operator=(other);
    return *this;
}


void
LimitedRetry_r10b::RunCoreTest()
{
    /** \verbatim
     * Assumptions:
     * 1) Test CreateResources_r10b has run prior.
     * \endverbatim
     */

    // Lookup objs which were created in a prior test within group
    SharedIOSQPtr iosq = CAST_TO_IOSQ(gRsrcMngr->GetObj(IOSQ_GROUP_ID));
    SharedIOCQPtr iocq = CAST_TO_IOCQ(gRsrcMngr->GetObj(IOCQ_GROUP_ID));

    SharedWritePtr writeCmd = CreateCmd();
    IO::SendCmdToHdw(mGrpName, mTestName, DEFAULT_CMD_WAIT_ms, iosq, iocq,
        writeCmd, "none.set", true);

    writeCmd->SetLR(true);
    IO::SendCmdToHdw(mGrpName, mTestName, DEFAULT_CMD_WAIT_ms, iosq, iocq,
        writeCmd, "all.set", true);
}


SharedWritePtr
LimitedRetry_r10b::CreateCmd()
{
    Informative::Namspc namspcData = gInformative->Get1stBareMetaE2E();
    if (namspcData.type != Informative::NS_BARE) {
        LBAFormat lbaFormat = namspcData.idCmdNamspc->GetLBAFormat();
        if (gRsrcMngr->SetMetaAllocSize(lbaFormat.MS) == false)
            throw FrmwkEx();
    }
    LOG_NRM("Processing write cmd using namspc id %d", namspcData.id);

    ConstSharedIdentifyPtr namSpcPtr = namspcData.idCmdNamspc;
    uint64_t lbaDataSize = namSpcPtr->GetLBADataSize();;
    SharedMemBufferPtr dataPat = SharedMemBufferPtr(new MemBuffer());
    dataPat->Init(lbaDataSize);

    SharedWritePtr writeCmd = SharedWritePtr(new Write());
    send_64b_bitmask prpBitmask = (send_64b_bitmask)(MASK_PRP1_PAGE
        | MASK_PRP2_PAGE | MASK_PRP2_LIST);

    if (namspcData.type == Informative::NS_META) {
        writeCmd->AllocMetaBuffer();
    } else if (namspcData.type == Informative::NS_E2E) {
        writeCmd->AllocMetaBuffer();
        LOG_ERR("Deferring E2E namspc work to the future");
        throw FrmwkEx("Need to add CRC's to correlate to buf pattern");
    }

    writeCmd->SetPrpBuffer(prpBitmask, dataPat);
    writeCmd->SetNSID(namspcData.id);
    writeCmd->SetNLB(0);
    return writeCmd;
}


}   // namespace
