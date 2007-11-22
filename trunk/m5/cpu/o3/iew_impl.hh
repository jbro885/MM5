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

// @todo: Fix the instantaneous communication among all the stages within
// iew.  There's a clear delay between issue and execute, yet backwards
// communication happens simultaneously.
// Update the statuses for each stage.

#include <queue>

#include "base/timebuf.hh"
#include "cpu/o3/iew.hh"

template<class Impl>
SimpleIEW<Impl>::WritebackEvent::WritebackEvent(DynInstPtr &_inst,
                                                SimpleIEW<Impl> *_iew)
    : Event(&mainEventQueue, CPU_Tick_Pri), inst(_inst), iewStage(_iew)
{
    this->setFlags(Event::AutoDelete);
}

template<class Impl>
void
SimpleIEW<Impl>::WritebackEvent::process()
{
    DPRINTF(IEW, "IEW: WRITEBACK EVENT!!!!\n");

    // Need to insert instruction into queue to commit
    iewStage->instToCommit(inst);
    // Need to execute second half of the instruction, do actual writing to
    // registers and such
    inst->execute();
}

template<class Impl>
const char *
SimpleIEW<Impl>::WritebackEvent::description()
{
    return "LSQ writeback event";
}

template<class Impl>
SimpleIEW<Impl>::SimpleIEW(Params &params)
    : // Just make this time buffer really big for now
      issueToExecQueue(5, 5),
      instQueue(params),
      ldstQueue(params),
      commitToIEWDelay(params.commitToIEWDelay),
      renameToIEWDelay(params.renameToIEWDelay),
      issueToExecuteDelay(params.issueToExecuteDelay),
      issueReadWidth(params.issueWidth),
      issueWidth(params.issueWidth),
      executeWidth(params.executeWidth)
{
    DPRINTF(IEW, "IEW: executeIntWidth: %i.\n", params.executeIntWidth);
    _status = Idle;
    _issueStatus = Idle;
    _exeStatus = Idle;
    _wbStatus = Idle;

    // Setup wire to read instructions coming from issue.
    fromIssue = issueToExecQueue.getWire(-issueToExecuteDelay);
    
    // Instruction queue needs the queue between issue and execute.
    instQueue.setIssueToExecuteQueue(&issueToExecQueue);

    ldstQueue.setIEW(this);
}

template <class Impl>
void
SimpleIEW<Impl>::regStats()
{
    instQueue.regStats();

    iewIdleCycles
        .name(name() + ".iewIdleCycles")
        .desc("Number of cycles IEW is idle");

    iewSquashCycles
        .name(name() + ".iewSquashCycles")
        .desc("Number of cycles IEW is squashing");

    iewBlockCycles
        .name(name() + ".iewBlockCycles")
        .desc("Number of cycles IEW is blocking");

    iewUnblockCycles
        .name(name() + ".iewUnblockCycles")
        .desc("Number of cycles IEW is unblocking");

//    iewWBInsts;

    iewDispatchedInsts
        .name(name() + ".iewDispatchedInsts")
        .desc("Number of instructions dispatched to IQ");

    iewDispSquashedInsts
        .name(name() + ".iewDispSquashedInsts")
        .desc("Number of squashed instructions skipped by dispatch");

    iewDispLoadInsts
        .name(name() + ".iewDispLoadInsts")
        .desc("Number of dispatched load instructions");

    iewDispStoreInsts
        .name(name() + ".iewDispStoreInsts")
        .desc("Number of dispatched store instructions");

    iewDispNonSpecInsts
        .name(name() + ".iewDispNonSpecInsts")
        .desc("Number of dispatched non-speculative instructions");

    iewIQFullEvents
        .name(name() + ".iewIQFullEvents")
        .desc("Number of times the IQ has become full, causing a stall");

    iewExecutedInsts
        .name(name() + ".iewExecutedInsts")
        .desc("Number of executed instructions");

    iewExecLoadInsts
        .name(name() + ".iewExecLoadInsts")
        .desc("Number of load instructions executed");

    iewExecStoreInsts
        .name(name() + ".iewExecStoreInsts")
        .desc("Number of store instructions executed");

    iewExecSquashedInsts
        .name(name() + ".iewExecSquashedInsts")
        .desc("Number of squashed instructions skipped in execute");

    memOrderViolationEvents
        .name(name() + ".memOrderViolationEvents")
        .desc("Number of memory order violations");

    predictedTakenIncorrect
        .name(name() + ".predictedTakenIncorrect")
        .desc("Number of branches that were predicted taken incorrectly");
}

template<class Impl>
void
SimpleIEW<Impl>::setCPU(FullCPU *cpu_ptr)
{
    DPRINTF(IEW, "IEW: Setting CPU pointer.\n");
    cpu = cpu_ptr;

    instQueue.setCPU(cpu_ptr);
    ldstQueue.setCPU(cpu_ptr);
}

template<class Impl>
void
SimpleIEW<Impl>::setTimeBuffer(TimeBuffer<TimeStruct> *tb_ptr)
{
    DPRINTF(IEW, "IEW: Setting time buffer pointer.\n");
    timeBuffer = tb_ptr;

    // Setup wire to read information from time buffer, from commit.
    fromCommit = timeBuffer->getWire(-commitToIEWDelay);

    // Setup wire to write information back to previous stages.
    toRename = timeBuffer->getWire(0);

    // Instruction queue also needs main time buffer.
    instQueue.setTimeBuffer(tb_ptr);
}

template<class Impl>
void
SimpleIEW<Impl>::setRenameQueue(TimeBuffer<RenameStruct> *rq_ptr)
{
    DPRINTF(IEW, "IEW: Setting rename queue pointer.\n");
    renameQueue = rq_ptr;

    // Setup wire to read information from rename queue.
    fromRename = renameQueue->getWire(-renameToIEWDelay);
}

template<class Impl>
void
SimpleIEW<Impl>::setIEWQueue(TimeBuffer<IEWStruct> *iq_ptr)
{
    DPRINTF(IEW, "IEW: Setting IEW queue pointer.\n");
    iewQueue = iq_ptr;

    // Setup wire to write instructions to commit.
    toCommit = iewQueue->getWire(0);
}

template<class Impl>
void
SimpleIEW<Impl>::setRenameMap(RenameMap *rm_ptr)
{
    DPRINTF(IEW, "IEW: Setting rename map pointer.\n");
    renameMap = rm_ptr;
}

template<class Impl>
void
SimpleIEW<Impl>::squash()
{
    DPRINTF(IEW, "IEW: Squashing all instructions.\n");
    _status = Squashing;

    // Tell the IQ to start squashing.
    instQueue.squash();

    // Tell the LDSTQ to start squashing.
    ldstQueue.squash(fromCommit->commitInfo.doneSeqNum);
}

template<class Impl>
void
SimpleIEW<Impl>::squashDueToBranch(DynInstPtr &inst)
{
    DPRINTF(IEW, "IEW: Squashing from a specific instruction, PC: %#x.\n",
            inst->PC);
    // Perhaps leave the squashing up to the ROB stage to tell it when to
    // squash?
    _status = Squashing;

    // Tell rename to squash through the time buffer.
    toCommit->squash = true;
    // Also send PC update information back to prior stages.
    toCommit->squashedSeqNum = inst->seqNum;
    toCommit->mispredPC = inst->readPC();
    toCommit->nextPC = inst->readNextPC();
    toCommit->branchMispredict = true;
    // Prediction was incorrect, so send back inverse.
    toCommit->branchTaken = inst->readNextPC() != 
        (inst->readPC() + sizeof(MachInst));
}

template<class Impl>
void
SimpleIEW<Impl>::squashDueToMem(DynInstPtr &inst)
{
    DPRINTF(IEW, "IEW: Squashing from a specific instruction, PC: %#x.\n",
            inst->PC);
    // Perhaps leave the squashing up to the ROB stage to tell it when to
    // squash?
    _status = Squashing;

    // Tell rename to squash through the time buffer.
    toCommit->squash = true;
    // Also send PC update information back to prior stages.
    toCommit->squashedSeqNum = inst->seqNum;
    toCommit->nextPC = inst->readNextPC();
}

template<class Impl>
void
SimpleIEW<Impl>::block()
{
    DPRINTF(IEW, "IEW: Blocking.\n");
    // Set the status to Blocked.
    _status = Blocked;

    // Add the current inputs to the skid buffer so they can be 
    // reprocessed when this stage unblocks.
    skidBuffer.push(*fromRename);
    
    // Note that this stage only signals previous stages to stall when
    // it is the cause of the stall originates at this stage.  Otherwise
    // the previous stages are expected to check all possible stall signals.
}

template<class Impl>
inline void
SimpleIEW<Impl>::unblock()
{
    // Check if there's information in the skid buffer.  If there is, then
    // set status to unblocking, otherwise set it directly to running.
    DPRINTF(IEW, "IEW: Reading instructions out of the skid "
            "buffer.\n");
    // Remove the now processed instructions from the skid buffer.
    skidBuffer.pop();
    
    // If there's still information in the skid buffer, then 
    // continue to tell previous stages to stall.  They will be 
    // able to restart once the skid buffer is empty.   
    if (!skidBuffer.empty()) {
        toRename->iewInfo.stall = true;
    } else {
        DPRINTF(IEW, "IEW: Stage is done unblocking.\n");
        _status = Running;
    }
}

template<class Impl>
void
SimpleIEW<Impl>::wakeDependents(DynInstPtr &inst)
{
    instQueue.wakeDependents(inst);
}


template<class Impl>
void
SimpleIEW<Impl>::instToCommit(DynInstPtr &inst)
{
    
}

template <class Impl>
void
SimpleIEW<Impl>::dispatchInsts()
{
    ////////////////////////////////////////
    // DISPATCH/ISSUE stage
    ////////////////////////////////////////

    //Put into its own function?
    //Add instructions to IQ if there are any instructions there

    // Check if there are any instructions coming from rename, and we're.
    // not squashing.
    if (fromRename->size > 0) {
        int insts_to_add = fromRename->size;

        // Loop through the instructions, putting them in the instruction
        // queue.
        for (int inst_num = 0; inst_num < insts_to_add; ++inst_num)
        {
            DynInstPtr inst = fromRename->insts[inst_num];

            // Make sure there's a valid instruction there.
            assert(inst);

            DPRINTF(IEW, "IEW: Issue: Adding PC %#x to IQ.\n", 
                    inst->readPC());

            // Be sure to mark these instructions as ready so that the
            // commit stage can go ahead and execute them, and mark
            // them as issued so the IQ doesn't reprocess them.
            if (inst->isSquashed()) {
                ++iewDispSquashedInsts;
                continue;
            } else if (instQueue.isFull()) {
                DPRINTF(IEW, "IEW: Issue: IQ has become full.\n");
                // Call function to start blocking.
                block();
                // Tell previous stage to stall.
                toRename->iewInfo.stall = true;

                ++iewIQFullEvents;
                break;
            } else if (inst->isLoad()) {
                DPRINTF(IEW, "IEW: Issue: Memory instruction "
                        "encountered, adding to LDSTQ.\n");

                // Reserve a spot in the load store queue for this
                // memory access.
                ldstQueue.insertLoad(inst);

                ++iewDispLoadInsts;
            } else if (inst->isStore()) {
                ldstQueue.insertStore(inst);

                ++iewDispStoreInsts;
            } else if (inst->isNonSpeculative()) {
                DPRINTF(IEW, "IEW: Issue: Nonspeculative instruction "
                        "encountered, skipping.\n");

                // Same hack as with stores.
                inst->setCanCommit();
                
                // Specificall insert it as nonspeculative.
                instQueue.insertNonSpec(inst);

                ++iewDispNonSpecInsts;

                continue;
            } else if (inst->isNop()) {
                DPRINTF(IEW, "IEW: Issue: Nop instruction encountered "
                        ", skipping.\n");

                inst->setIssued();
                inst->setExecuted();
                inst->setCanCommit();

                instQueue.advanceTail(inst);

                continue;
            } else if (inst->isExecuted()) {
                assert(0 && "Instruction shouldn't be executed.\n");
                DPRINTF(IEW, "IEW: Issue: Executed branch encountered, "
                        "skipping.\n");

                inst->setIssued();
                inst->setCanCommit();

                instQueue.advanceTail(inst);

                continue;
            }

            // If the instruction queue is not full, then add the 
            // instruction.
            instQueue.insert(fromRename->insts[inst_num]);

            ++iewDispatchedInsts;
        }
    }
}

template <class Impl>
void
SimpleIEW<Impl>::executeInsts()
{
    ////////////////////////////////////////
    //EXECUTE/WRITEBACK stage
    ////////////////////////////////////////

    //Put into its own function?
    //Similarly should probably have separate execution for int vs FP.
    // Above comment is handled by the issue queue only issuing a valid
    // mix of int/fp instructions.
    //Actually okay to just have one execution, buuuuuut will need
    //somewhere that defines the execution latency of all instructions.
    // @todo: Move to the FU pool used in the current full cpu.

    int fu_usage = 0;
    bool fetch_redirect = false;
    int inst_slot = 0;
    int time_slot = 0;

    // Execute/writeback any instructions that are available.
    for (int inst_num = 0; 
         fu_usage < executeWidth && /* Haven't exceeded available FU's. */
             inst_num < issueWidth &&
             fromIssue->insts[inst_num]; 
         ++inst_num) {

        DPRINTF(IEW, "IEW: Execute: Executing instructions from IQ.\n");

        // Get instruction from issue's queue.
        DynInstPtr inst = fromIssue->insts[inst_num];

        DPRINTF(IEW, "IEW: Execute: Processing PC %#x.\n", inst->readPC());

        // Check if the instruction is squashed; if so then skip it
        // and don't count it towards the FU usage.
        if (inst->isSquashed()) {
            DPRINTF(IEW, "IEW: Execute: Instruction was squashed.\n");

            // Consider this instruction executed so that commit can go
            // ahead and retire the instruction.
            inst->setExecuted();

            toCommit->insts[inst_num] = inst;

            ++iewExecSquashedInsts;

            continue;
        } 

        inst->setExecuted();

        // If an instruction is executed, then count it towards FU usage.
        ++fu_usage;

        // Execute instruction.
        // Note that if the instruction faults, it will be handled
        // at the commit stage.
        if (inst->isMemRef()) {
            DPRINTF(IEW, "IEW: Execute: Calculating address for memory "
                    "reference.\n");

            // Tell the LDSTQ to execute this instruction (if it is a load).
            if (inst->isLoad()) {
                ldstQueue.executeLoad(inst);

                ++iewExecLoadInsts;
            } else if (inst->isStore()) {
                ldstQueue.executeStore(inst);

                ++iewExecStoreInsts;
            } else {
                panic("IEW: Unexpected memory type!\n");
            }

        } else {
            inst->execute();

            ++iewExecutedInsts;
        }

        // First check the time slot that this instruction will write
        // to.  If there are free write ports at the time, then go ahead
        // and write the instruction to that time.  If there are not,
        // keep looking back to see where's the first time there's a
        // free slot.  What happens if you run out of free spaces?
        // For now naively assume that all instructions take one cycle.
        // Otherwise would have to look into the time buffer based on the
        // latency of the instruction.
        (*iewQueue)[time_slot].insts[inst_slot];
        while ((*iewQueue)[time_slot].insts[inst_slot]) {
            if (inst_slot < issueWidth) {
                ++inst_slot;
            } else {
                ++time_slot;
                inst_slot = 0;
            }

            assert(time_slot < 5);
        }

        // May actually have to work this out, especially with loads and stores

        // Add finished instruction to queue to commit.
        (*iewQueue)[time_slot].insts[inst_slot] = inst;
        (*iewQueue)[time_slot].size++;

        // Check if branch was correct.  This check happens after the
        // instruction is added to the queue because even if the branch
        // is mispredicted, the branch instruction itself is still valid.
        // Only handle this if there hasn't already been something that 
        // redirects fetch in this group of instructions.
        if (!fetch_redirect) {
            if (inst->mispredicted()) {
                fetch_redirect = true;
                
                DPRINTF(IEW, "IEW: Execute: Branch mispredict detected.\n");
                DPRINTF(IEW, "IEW: Execute: Redirecting fetch to PC: %#x.\n",
                        inst->nextPC);
                
                // If incorrect, then signal the ROB that it must be squashed.
                squashDueToBranch(inst);

                if (inst->predTaken()) {
                    predictedTakenIncorrect++;
                }
            } else if (ldstQueue.violation()) {
                fetch_redirect = true;

                // Get the DynInst that caused the violation.
                DynInstPtr violator = ldstQueue.getMemDepViolator();

                DPRINTF(IEW, "IEW: LDSTQ detected a violation.  Violator PC: "
                        "%#x, inst PC: %#x.  Addr is: %#x.\n",
                        violator->readPC(), inst->readPC(), inst->physEffAddr);

                // Tell the instruction queue that a violation has occured.
                instQueue.violation(inst, violator);
                
                // Squash.
                squashDueToMem(inst);

                ++memOrderViolationEvents;
            }
        }
    }
}

template<class Impl>
void 
SimpleIEW<Impl>::tick()
{
    // Considering putting all the state-determining stuff in this section.
    
    // Try to fill up issue queue with as many instructions as bandwidth
    // allows.
    // Decode should try to execute as many instructions as its bandwidth
    // will allow, as long as it is not currently blocked.

    // Check if the stage is in a running status.
    if (_status != Blocked && _status != Squashing) {
        DPRINTF(IEW, "IEW: Status is not blocked, attempting to run "
                     "stage.\n");
        iew();
        
        // If it's currently unblocking, check to see if it should switch
        // to running.
        if (_status == Unblocking) {
            unblock();

            ++iewUnblockCycles;
        }
    } else if (_status == Squashing) {

        DPRINTF(IEW, "IEW: Still squashing.\n");

        // Check if stage should remain squashing.  Stop squashing if the 
        // squash signal clears.
        if (!fromCommit->commitInfo.squash &&
            !fromCommit->commitInfo.robSquashing) {
            DPRINTF(IEW, "IEW: Done squashing, changing status to "
                    "running.\n");

            _status = Running;
            instQueue.stopSquash();
        } else {
            instQueue.doSquash();
        }

        ++iewSquashCycles;
    } else if (_status == Blocked) {
        // Continue to tell previous stage to stall.
        toRename->iewInfo.stall = true;

        // Check if possible stall conditions have cleared.
        if (!fromCommit->commitInfo.stall && 
            !instQueue.isFull()) {
            DPRINTF(IEW, "IEW: Stall signals cleared, going to unblock.\n");
            _status = Unblocking;
        } 

        // If there's still instructions coming from rename, continue to
        // put them on the skid buffer.
        if (fromRename->size == 0) {
            block();
        }

        if (fromCommit->commitInfo.squash ||
            fromCommit->commitInfo.robSquashing) {
            squash();
        }

        ++iewBlockCycles;
    }
    
    // @todo: Maybe put these at the beginning, so if it's idle it can
    // return early.
    // Write back number of free IQ entries here.
    toRename->iewInfo.freeIQEntries = instQueue.numFreeEntries();

    ldstQueue.writebackStores();

    // Check the committed load/store signals to see if there's a load
    // or store to commit.  Also check if it's being told to execute a
    // nonspeculative instruction.
    // This is pretty inefficient...
    if (!fromCommit->commitInfo.squash &&
        !fromCommit->commitInfo.robSquashing) {
        ldstQueue.commitStores(fromCommit->commitInfo.doneSeqNum);
        ldstQueue.commitLoads(fromCommit->commitInfo.doneSeqNum);
    }

    if (fromCommit->commitInfo.nonSpecSeqNum != 0) {
        instQueue.scheduleNonSpec(fromCommit->commitInfo.nonSpecSeqNum);
    }

    DPRINTF(IEW, "IEW: IQ has %i free entries.\n", 
            instQueue.numFreeEntries());
}

template<class Impl>
void
SimpleIEW<Impl>::iew()
{
    // Might want to put all state checks in the tick() function.
    // Check if being told to stall from commit.
    if (fromCommit->commitInfo.stall) {
        block();
        return;
    } else if (fromCommit->commitInfo.squash ||
               fromCommit->commitInfo.robSquashing) {
        // Also check if commit is telling this stage to squash.
        squash();
        return;
    }

    dispatchInsts();

    // Have the instruction queue try to schedule any ready instructions.
    instQueue.scheduleReadyInsts();

    executeInsts();

    // Loop through the head of the time buffer and wake any dependents.
    // These instructions are about to write back.  In the simple model
    // this loop can really happen within the previous loop, but when
    // instructions have actual latencies, this loop must be separate.
    // Also mark scoreboard that this instruction is finally complete.
    // Either have IEW have direct access to rename map, or have this as
    // part of backwards communication.
    for (int inst_num = 0; inst_num < issueWidth &&
             toCommit->insts[inst_num]; inst_num++)
    {
        DynInstPtr inst = toCommit->insts[inst_num];

        DPRINTF(IEW, "IEW: Sending instructions to commit, PC %#x.\n",
                inst->readPC());

        if(!inst->isSquashed()) {
            instQueue.wakeDependents(inst);
    
            for (int i = 0; i < inst->numDestRegs(); i++)
            {
                renameMap->markAsReady(inst->renamedDestRegIdx(i));
            }
        }
    }

    // Also should advance its own time buffers if the stage ran.
    // Not the best place for it, but this works (hopefully).
    issueToExecQueue.advance();
}

#if !FULL_SYSTEM
template<class Impl>
void
SimpleIEW<Impl>::lsqWriteback()
{
    ldstQueue.writebackAllInsts();
}
#endif
