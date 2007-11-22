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

// Todo: 
// Fix up trap and barrier handling.
// May want to have different statuses to differentiate the different stall
// conditions.

#ifndef __CPU_O3_CPU_SIMPLE_RENAME_HH__
#define __CPU_O3_CPU_SIMPLE_RENAME_HH__

#include <list>

#include "base/statistics.hh"
#include "base/timebuf.hh"

// Will need rename maps for both the int reg file and fp reg file.
// Or change rename map class to handle both. (RegFile handles both.)
template<class Impl>
class SimpleRename
{
  public:
    // Typedefs from the Impl.
    typedef typename Impl::ISA ISA;
    typedef typename Impl::CPUPol CPUPol;
    typedef typename Impl::DynInstPtr DynInstPtr;
    typedef typename Impl::FullCPU FullCPU;
    typedef typename Impl::Params Params;

    typedef typename CPUPol::FetchStruct FetchStruct;
    typedef typename CPUPol::DecodeStruct DecodeStruct;
    typedef typename CPUPol::RenameStruct RenameStruct;
    typedef typename CPUPol::TimeStruct TimeStruct;

    // Typedefs from the CPUPol
    typedef typename CPUPol::FreeList FreeList;
    typedef typename CPUPol::RenameMap RenameMap;

    // Typedefs from the ISA.
    typedef typename ISA::Addr Addr;

  public:
    // Rename will block if ROB becomes full or issue queue becomes full,
    // or there are no free registers to rename to.
    // Only case where rename squashes is if IEW squashes.
    enum Status {
        Running,
        Idle,
        Squashing,
        Blocked,
        Unblocking,
        BarrierStall
    };

  private:
    Status _status;

  public:
    SimpleRename(Params &params);

    void regStats();

    void setCPU(FullCPU *cpu_ptr);

    void setTimeBuffer(TimeBuffer<TimeStruct> *tb_ptr);

    void setRenameQueue(TimeBuffer<RenameStruct> *rq_ptr);

    void setDecodeQueue(TimeBuffer<DecodeStruct> *dq_ptr);

    void setRenameMap(RenameMap *rm_ptr);

    void setFreeList(FreeList *fl_ptr);

    void dumpHistory();

    void tick();

    void rename();

    void squash();

  private:
    void block();
    
    inline void unblock();

    void doSquash();

    void removeFromHistory(InstSeqNum inst_seq_num);

    inline void renameSrcRegs(DynInstPtr &inst);

    inline void renameDestRegs(DynInstPtr &inst);

    inline int calcFreeROBEntries();

    inline int calcFreeIQEntries();

    /** Holds the previous information for each rename. 
     *  Note that often times the inst may have been deleted, so only access
     *  the pointer for the address and do not dereference it.
     */
    struct RenameHistory {
        RenameHistory(InstSeqNum _instSeqNum, RegIndex _archReg, 
                      PhysRegIndex _newPhysReg, PhysRegIndex _prevPhysReg)
            : instSeqNum(_instSeqNum), archReg(_archReg), 
              newPhysReg(_newPhysReg), prevPhysReg(_prevPhysReg),
              placeHolder(false)
        {
        }

        /** Constructor used specifically for cases where a place holder
         *  rename history entry is being made.
         */
        RenameHistory(InstSeqNum _instSeqNum)
            : instSeqNum(_instSeqNum), archReg(0), newPhysReg(0), 
              prevPhysReg(0), placeHolder(true)
        {
        }

        InstSeqNum instSeqNum;
        RegIndex archReg;
        PhysRegIndex newPhysReg;
        PhysRegIndex prevPhysReg;
        bool placeHolder;
    };

    std::list<RenameHistory> historyBuffer;

    /** CPU interface. */
    FullCPU *cpu;

    // Interfaces to objects outside of rename.
    /** Time buffer interface. */
    TimeBuffer<TimeStruct> *timeBuffer;

    /** Wire to get IEW's output from backwards time buffer. */
    typename TimeBuffer<TimeStruct>::wire fromIEW;

    /** Wire to get commit's output from backwards time buffer. */
    typename TimeBuffer<TimeStruct>::wire fromCommit;

    /** Wire to write infromation heading to previous stages. */
    // Might not be the best name as not only decode will read it.
    typename TimeBuffer<TimeStruct>::wire toDecode;

    /** Rename instruction queue. */
    TimeBuffer<RenameStruct> *renameQueue;

    /** Wire to write any information heading to IEW. */
    typename TimeBuffer<RenameStruct>::wire toIEW;

    /** Decode instruction queue interface. */
    TimeBuffer<DecodeStruct> *decodeQueue;

    /** Wire to get decode's output from decode queue. */
    typename TimeBuffer<DecodeStruct>::wire fromDecode;

    /** Skid buffer between rename and decode. */
    std::queue<DecodeStruct> skidBuffer;

    /** Rename map interface. */
    SimpleRenameMap *renameMap;

    /** Free list interface. */
    FreeList *freeList;

    /** Delay between iew and rename, in ticks. */
    int iewToRenameDelay;

    /** Delay between decode and rename, in ticks. */
    int decodeToRenameDelay;

    /** Delay between commit and rename, in ticks. */
    unsigned commitToRenameDelay;

    /** Rename width, in instructions. */
    unsigned renameWidth;

    /** Commit width, in instructions.  Used so rename knows how many
     *  instructions might have freed registers in the previous cycle.
     */
    unsigned commitWidth;

    /** The instruction that rename is currently on.  It needs to have
     *  persistent state so that when a stall occurs in the middle of a
     *  group of instructions, it can restart at the proper instruction.
     */
    unsigned numInst;

    Stats::Scalar<> renameSquashCycles;
    Stats::Scalar<> renameIdleCycles;
    Stats::Scalar<> renameBlockCycles;
    Stats::Scalar<> renameUnblockCycles;
    Stats::Scalar<> renameRenamedInsts;
    Stats::Scalar<> renameSquashedInsts;
    Stats::Scalar<> renameROBFullEvents;
    Stats::Scalar<> renameIQFullEvents;
    Stats::Scalar<> renameFullRegistersEvents;
    Stats::Scalar<> renameRenamedOperands;
    Stats::Scalar<> renameRenameLookups;
    Stats::Scalar<> renameHBPlaceHolders;
    Stats::Scalar<> renameCommittedMaps;
    Stats::Scalar<> renameUndoneMaps;
    Stats::Scalar<> renameValidUndoneMaps;
};

#endif // __CPU_O3_CPU_SIMPLE_RENAME_HH__
