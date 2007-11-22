/*
 * Copyright (c) 2004, 2005
 * The Regents of The University of Michigan
 * All Rights Reserved
 *
 * This code is part of the M5 simulator, developed by Nathan Binkert,
 * Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
 * from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi, 
 * and Andrew Schultz.
 *
 * Permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any
 * purpose, so long as the copyright notice above, this grant of
 * permission, and the disclaimer below appear in all copies made; and
 * so long as the name of The University of Michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
 * UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
 * WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
 * LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGES.
 */

#include "cpu/o3/decode.hh"

template<class Impl>
SimpleDecode<Impl>::SimpleDecode(Params &params)
    : renameToDecodeDelay(params.renameToDecodeDelay),
      iewToDecodeDelay(params.iewToDecodeDelay),
      commitToDecodeDelay(params.commitToDecodeDelay),
      fetchToDecodeDelay(params.fetchToDecodeDelay),
      decodeWidth(params.decodeWidth),
      numInst(0)
{
    DPRINTF(Decode, "Decode: decodeWidth=%i.\n", decodeWidth);
    _status = Idle;
}

template <class Impl>
void
SimpleDecode<Impl>::regStats()
{
    decodeIdleCycles
        .name(name() + ".decodeIdleCycles")
        .desc("Number of cycles decode is idle")
        .prereq(decodeIdleCycles);
    decodeBlockedCycles
        .name(name() + ".decodeBlockedCycles")
        .desc("Number of cycles decode is blocked")
        .prereq(decodeBlockedCycles);
    decodeUnblockCycles
        .name(name() + ".decodeUnblockCycles")
        .desc("Number of cycles decode is unblocking")
        .prereq(decodeUnblockCycles);
    decodeSquashCycles
        .name(name() + ".decodeSquashCycles")
        .desc("Number of cycles decode is squashing")
        .prereq(decodeSquashCycles);
    decodeBranchMispred
        .name(name() + ".decodeBranchMispred")
        .desc("Number of times decode detected a branch misprediction")
        .prereq(decodeBranchMispred);
    decodeControlMispred
        .name(name() + ".decodeControlMispred")
        .desc("Number of times decode detected an instruction incorrectly"
              " predicted as a control")
        .prereq(decodeControlMispred);
    decodeDecodedInsts
        .name(name() + ".decodeDecodedInsts")
        .desc("Number of instructions handled by decode")
        .prereq(decodeDecodedInsts);
    decodeSquashedInsts
        .name(name() + ".decodeSquashedInsts")
        .desc("Number of squashed instructions handled by decode")
        .prereq(decodeSquashedInsts);
}

template<class Impl>
void
SimpleDecode<Impl>::setCPU(FullCPU *cpu_ptr)
{
    DPRINTF(Decode, "Decode: Setting CPU pointer.\n");
    cpu = cpu_ptr;
}

template<class Impl>
void
SimpleDecode<Impl>::setTimeBuffer(TimeBuffer<TimeStruct> *tb_ptr)
{
    DPRINTF(Decode, "Decode: Setting time buffer pointer.\n");
    timeBuffer = tb_ptr;

    // Setup wire to write information back to fetch.
    toFetch = timeBuffer->getWire(0);
    
    // Create wires to get information from proper places in time buffer.
    fromRename = timeBuffer->getWire(-renameToDecodeDelay);
    fromIEW = timeBuffer->getWire(-iewToDecodeDelay);
    fromCommit = timeBuffer->getWire(-commitToDecodeDelay);
}

template<class Impl>
void
SimpleDecode<Impl>::setDecodeQueue(TimeBuffer<DecodeStruct> *dq_ptr)
{
    DPRINTF(Decode, "Decode: Setting decode queue pointer.\n");
    decodeQueue = dq_ptr;

    // Setup wire to write information to proper place in decode queue.
    toRename = decodeQueue->getWire(0);
}

template<class Impl>
void
SimpleDecode<Impl>::setFetchQueue(TimeBuffer<FetchStruct> *fq_ptr)
{
    DPRINTF(Decode, "Decode: Setting fetch queue pointer.\n");
    fetchQueue = fq_ptr;

    // Setup wire to read information from fetch queue.
    fromFetch = fetchQueue->getWire(-fetchToDecodeDelay);
}

template<class Impl>
inline bool
SimpleDecode<Impl>::fetchInstsValid()
{
    return fromFetch->size > 0;
}

template<class Impl>
void
SimpleDecode<Impl>::block()
{
    DPRINTF(Decode, "Decode: Blocking.\n");

    // Set the status to Blocked.
    _status = Blocked;

    // Add the current inputs to the skid buffer so they can be 
    // reprocessed when this stage unblocks.
    skidBuffer.push(*fromFetch);
    
    // Note that this stage only signals previous stages to stall when
    // it is the cause of the stall originates at this stage.  Otherwise
    // the previous stages are expected to check all possible stall signals.
}

template<class Impl>
inline void
SimpleDecode<Impl>::unblock()
{
    DPRINTF(Decode, "Decode: Unblocking, going to remove "
            "instructions from skid buffer.\n");
    // Remove the now processed instructions from the skid buffer.
    skidBuffer.pop();
    
    // If there's still information in the skid buffer, then 
    // continue to tell previous stages to stall.  They will be 
    // able to restart once the skid buffer is empty.   
    if (!skidBuffer.empty()) {
        toFetch->decodeInfo.stall = true;
    } else {
        DPRINTF(Decode, "Decode: Finished unblocking.\n");
        _status = Running;
    }
}

// This squash is specifically for when Decode detects a PC-relative branch
// was predicted incorrectly.
template<class Impl>
void
SimpleDecode<Impl>::squash(DynInstPtr &inst)
{
    DPRINTF(Decode, "Decode: Squashing due to incorrect branch prediction "
                    "detected at decode.\n");
    Addr new_PC = inst->readNextPC();
    
    toFetch->decodeInfo.branchMispredict = true;
    toFetch->decodeInfo.doneSeqNum = inst->seqNum;
    toFetch->decodeInfo.predIncorrect = true;
    toFetch->decodeInfo.squash = true;
    toFetch->decodeInfo.nextPC = new_PC;
    toFetch->decodeInfo.branchTaken = true;
    
    // Set status to squashing.
    _status = Squashing;

    // Clear the skid buffer in case it has any data in it.
    while (!skidBuffer.empty()) {
        skidBuffer.pop();
    }

    // Squash instructions up until this one
    // Slightly unrealistic!
    cpu->removeInstsUntil(inst->seqNum);
}

template<class Impl>
void
SimpleDecode<Impl>::squash()
{
    DPRINTF(Decode, "Decode: Squashing.\n");
    // Set status to squashing.
    _status = Squashing;

    // Maybe advance the time buffer?  Not sure what to do in the normal 
    // case.
    
    // Clear the skid buffer in case it has any data in it.
    while (!skidBuffer.empty())
    {
        skidBuffer.pop();
    }
}

template<class Impl>
void
SimpleDecode<Impl>::tick()
{
    // Decode should try to execute as many instructions as its bandwidth
    // will allow, as long as it is not currently blocked.
    if (_status != Blocked && _status != Squashing) {
        DPRINTF(Decode, "Decode: Not blocked, so attempting to run "
                        "stage.\n");
        // Make sure that the skid buffer has something in it if the
        // status is unblocking.
        assert(_status == Unblocking ? !skidBuffer.empty() : 1);

        decode();
        
        // If the status was unblocking, then instructions from the skid
        // buffer were used.  Remove those instructions and handle
        // the rest of unblocking.        
        if (_status == Unblocking) {
            ++decodeUnblockCycles;

            if (fetchInstsValid()) {
                // Add the current inputs to the skid buffer so they can be 
                // reprocessed when this stage unblocks.
                skidBuffer.push(*fromFetch);
            } 

            unblock();
        }
    } else if (_status == Blocked) {
        ++decodeBlockedCycles;

        if (fetchInstsValid()) {
            block();
        } 

        if (!fromRename->renameInfo.stall &&
            !fromIEW->iewInfo.stall &&
            !fromCommit->commitInfo.stall) {
            DPRINTF(Decode, "Decode: Stall signals cleared, going to "
                    "unblock.\n");
            _status = Unblocking;

            // Continue to tell previous stage to block until this
            // stage is done unblocking.
            toFetch->decodeInfo.stall = true;
        } else {
            DPRINTF(Decode, "Decode: Still blocked.\n");
            toFetch->decodeInfo.stall = true;
        }

        if (fromCommit->commitInfo.squash ||
            fromCommit->commitInfo.robSquashing) {
            squash();
        }
    } else if (_status == Squashing) {
        if (!fromCommit->commitInfo.squash &&
            !fromCommit->commitInfo.robSquashing) {
            _status = Running;
        } else if (fromCommit->commitInfo.squash) {
            ++decodeSquashCycles;

            squash();
        }
    }
}

template<class Impl>
void
SimpleDecode<Impl>::decode()
{
    // Check time buffer if being told to squash.
    if (fromCommit->commitInfo.squash) {
        squash();
        return;
    }

    // Check time buffer if being told to stall.
    if (fromRename->renameInfo.stall || 
        fromIEW->iewInfo.stall ||
        fromCommit->commitInfo.stall) {
        block();
        return;
    }

    // Check fetch queue to see if instructions are available.
    // If no available instructions, do nothing, unless this stage is
    // currently unblocking.
    if (!fetchInstsValid() && _status != Unblocking) {
        DPRINTF(Decode, "Decode: Nothing to do, breaking out early.\n");
        // Should I change the status to idle?
        ++decodeIdleCycles;
        return;
    }

    // Might be better to use a base DynInst * instead?    
    DynInstPtr inst;

    unsigned to_rename_index = 0;

    int insts_available = _status == Unblocking ? 
        skidBuffer.front().size - numInst :
        fromFetch->size;

    // Debug block...
#if 0
    if (insts_available) {
        DPRINTF(Decode, "Decode: Instructions available.\n");
    } else {
        if (_status == Unblocking && skidBuffer.empty()) {
            DPRINTF(Decode, "Decode: No instructions available, skid buffer "
                    "empty.\n");
        } else if (_status != Unblocking && 
                   !fromFetch->insts[0]) {
            DPRINTF(Decode, "Decode: No instructions available, fetch queue "
                    "empty.\n");
        } else {
            panic("Decode: No instructions available, unexpected condition!"
                  "\n");
        }            
    }
#endif

    while (insts_available > 0)
    {
        DPRINTF(Decode, "Decode: Sending instruction to rename.\n");

        inst = _status == Unblocking ? skidBuffer.front().insts[numInst] : 
               fromFetch->insts[numInst];

        DPRINTF(Decode, "Decode: Processing instruction %i with PC %#x\n",
                inst->seqNum, inst->readPC());

        if (inst->isSquashed()) {
            DPRINTF(Decode, "Decode: Instruction %i with PC %#x is "
                    "squashed, skipping.\n",
                    inst->seqNum, inst->readPC());

            ++decodeSquashedInsts;

            ++numInst;
            --insts_available;

            continue;
        }


        // Also check if instructions have no source registers.  Mark
        // them as ready to issue at any time.  Not sure if this check
        // should exist here or at a later stage; however it doesn't matter 
        // too much for function correctness.
        // Isn't this handled by the inst queue?
        if (inst->numSrcRegs() == 0) {
            inst->setCanIssue();
        }

        // This current instruction is valid, so add it into the decode
        // queue.  The next instruction may not be valid, so check to
        // see if branches were predicted correctly.
        toRename->insts[to_rename_index] = inst;

        ++(toRename->size);
        
        // Ensure that if it was predicted as a branch, it really is a
        // branch.
        if (inst->predTaken() && !inst->isControl()) {
            panic("Instruction predicted as a branch!");

            ++decodeControlMispred;
            // Might want to set some sort of boolean and just do
            // a check at the end
            squash(inst);
            break;
        }

        // Go ahead and compute any PC-relative branches.

        if (inst->isDirectCtrl() && inst->isUncondCtrl()) {

            inst->setNextPC(inst->branchTarget());

            if (inst->mispredicted()) {
                ++decodeBranchMispred;
                // Might want to set some sort of boolean and just do
                // a check at the end
                squash(inst);
                break;
            }
        }

        // Normally can check if a direct branch has the right target
        // addr (either the immediate, or the branch PC + 4) and redirect
        // fetch if it's incorrect.

        // Increment which instruction we're looking at.
        ++numInst;
        ++to_rename_index;
        ++decodeDecodedInsts;

        --insts_available;
    }

     numInst = 0;
}
