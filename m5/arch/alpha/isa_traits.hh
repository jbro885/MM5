/*
 * Copyright (c) 2003, 2004, 2005
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

#ifndef __ARCH_ALPHA_ISA_TRAITS_HH__
#define __ARCH_ALPHA_ISA_TRAITS_HH__

#include "arch/alpha/faults.hh"
#include "base/misc.hh"
#include "config/full_system.hh"
#include "sim/host.hh"

class FastCPU;
class FullCPU;
class Checkpoint;

#define TARGET_ALPHA

template <class ISA> class StaticInst;
template <class ISA> class StaticInstPtr;

namespace EV5 {
int DTB_ASN_ASN(uint64_t reg);
int ITB_ASN_ASN(uint64_t reg);
}

class AlphaISA
{
  public:

    typedef uint32_t MachInst;
    typedef uint64_t Addr;
    typedef uint8_t  RegIndex;

    enum {
	MemoryEnd = 0xffffffffffffffffULL,

	NumIntRegs = 32,
	NumFloatRegs = 32,
	NumMiscRegs = 32,

	MaxRegsOfAnyType = 32,
	// Static instruction parameters
	MaxInstSrcRegs = 3,
	MaxInstDestRegs = 2,

	// semantically meaningful register indices
	ZeroReg = 31,	// architecturally meaningful
	// the rest of these depend on the ABI
	StackPointerReg = 30,
	GlobalPointerReg = 29,
	ReturnAddressReg = 26,
	ReturnValueReg = 0,
	ArgumentReg0 = 16,
	ArgumentReg1 = 17,
	ArgumentReg2 = 18,
	ArgumentReg3 = 19,
	ArgumentReg4 = 20,
	ArgumentReg5 = 21,

	LogVMPageSize = 13,	// 8K bytes
	VMPageSize = (1 << LogVMPageSize),

	BranchPredAddrShiftAmt = 2, // instructions are 4-byte aligned

	WordBytes = 4,
	HalfwordBytes = 2,
	ByteBytes = 1,
	DepNA = 0,
    };

    // These enumerate all the registers for dependence tracking.
    enum DependenceTags {
	// 0..31 are the integer regs 0..31
	// 32..63 are the FP regs 0..31, i.e. use (reg + FP_Base_DepTag)
	FP_Base_DepTag = 32,
	Ctrl_Base_DepTag = 64,
	Fpcr_DepTag = 64,		// floating point control register
	Uniq_DepTag = 65,
	IPR_Base_DepTag = 66
    };

    typedef uint64_t IntReg;
    typedef IntReg IntRegFile[NumIntRegs];

    // floating point register file entry type
    typedef union {
	uint64_t q;
	double d;
    } FloatReg;

    typedef union {
	uint64_t q[NumFloatRegs];	// integer qword view
	double d[NumFloatRegs];		// double-precision floating point view
    } FloatRegFile;

    // control register file contents
    typedef uint64_t MiscReg;
    typedef struct {
	uint64_t	fpcr;		// floating point condition codes
	uint64_t	uniq;		// process-unique register
	bool		lock_flag;	// lock flag for LL/SC
	Addr		lock_addr;	// lock address for LL/SC
    } MiscRegFile;

static const Addr PageShift = 13;
static const Addr PageBytes = ULL(1) << PageShift;
static const Addr PageMask = ~(PageBytes - 1);
static const Addr PageOffset = PageBytes - 1;

#if FULL_SYSTEM

    typedef uint64_t InternalProcReg;

#include "arch/alpha/isa_fullsys_traits.hh"

#else
    enum {
	NumInternalProcRegs = 0
    };
#endif

    enum {
	TotalNumRegs =
	NumIntRegs + NumFloatRegs + NumMiscRegs + NumInternalProcRegs
    };

    enum {
        TotalDataRegs = NumIntRegs + NumFloatRegs
    };

    typedef union {
	IntReg  intreg;
	FloatReg   fpreg;
	MiscReg ctrlreg;
    } AnyReg;

    struct RegFile {
	IntRegFile intRegFile;		// (signed) integer register file
	FloatRegFile floatRegFile;	// floating point register file
	MiscRegFile miscRegs;		// control register file
	Addr pc;			// program counter
	Addr npc;			// next-cycle program counter
#if FULL_SYSTEM
	IntReg palregs[NumIntRegs];	// PAL shadow registers
	InternalProcReg ipr[NumInternalProcRegs]; // internal processor regs
	int intrflag;			// interrupt flag
	bool pal_shadow;		// using pal_shadow registers
	inline int instAsid() { return EV5::ITB_ASN_ASN(ipr[IPR_ITB_ASN]); }
	inline int dataAsid() { return EV5::DTB_ASN_ASN(ipr[IPR_DTB_ASN]); }
#endif // FULL_SYSTEM

	void serialize(std::ostream &os);
	void unserialize(Checkpoint *cp, const std::string &section);
    };

    static StaticInstPtr<AlphaISA> decodeInst(MachInst);

    // return a no-op instruction... used for instruction fetch faults
    static const MachInst NoopMachInst;

    enum annotes {
	ANNOTE_NONE = 0,
	// An impossible number for instruction annotations
	ITOUCH_ANNOTE = 0xffffffff,
    };

    static inline bool isCallerSaveIntegerRegister(unsigned int reg) {
	panic("register classification not implemented");
	return (reg >= 1 && reg <= 8 || reg >= 22 && reg <= 25 || reg == 27);
    }

    static inline bool isCalleeSaveIntegerRegister(unsigned int reg) {
	panic("register classification not implemented");
	return (reg >= 9 && reg <= 15);
    }

    static inline bool isCallerSaveFloatRegister(unsigned int reg) {
	panic("register classification not implemented");
	return false;
    }

    static inline bool isCalleeSaveFloatRegister(unsigned int reg) {
	panic("register classification not implemented");
	return false;
    }

    static inline Addr alignAddress(const Addr &addr,
					 unsigned int nbytes) {
	return (addr & ~(nbytes - 1));
    }

    // Instruction address compression hooks
    static inline Addr realPCToFetchPC(const Addr &addr) {
	return addr;
    }

    static inline Addr fetchPCToRealPC(const Addr &addr) {
	return addr;
    }

    // the size of "fetched" instructions (not necessarily the size
    // of real instructions for PISA)
    static inline size_t fetchInstSize() {
	return sizeof(MachInst);
    }

    static inline MachInst makeRegisterCopy(int dest, int src) {
	panic("makeRegisterCopy not implemented");
	return 0;
    }

    // Machine operations

    static void saveMachineReg(AnyReg &savereg, const RegFile &reg_file,
			       int regnum);

    static void restoreMachineReg(RegFile &regs, const AnyReg &reg,
				  int regnum);

#if 0
    static void serializeSpecialRegs(const Serializable::Proxy &proxy,
				     const RegFile &regs);

    static void unserializeSpecialRegs(const IniFile *db,
				       const std::string &category,
				       ConfigNode *node,
				       RegFile &regs);
#endif

    /**
     * Function to insure ISA semantics about 0 registers.
     * @param xc The execution context.
     */
    template <class XC>
    static void zeroRegisters(XC *xc);
};


typedef AlphaISA TheISA;

typedef TheISA::MachInst MachInst;
typedef TheISA::Addr Addr;
typedef TheISA::RegIndex RegIndex;
typedef TheISA::IntReg IntReg;
typedef TheISA::IntRegFile IntRegFile;
typedef TheISA::FloatReg FloatReg;
typedef TheISA::FloatRegFile FloatRegFile;
typedef TheISA::MiscReg MiscReg;
typedef TheISA::MiscRegFile MiscRegFile;
typedef TheISA::AnyReg AnyReg;
typedef TheISA::RegFile RegFile;

const int NumIntRegs   = TheISA::NumIntRegs;
const int NumFloatRegs = TheISA::NumFloatRegs;
const int NumMiscRegs  = TheISA::NumMiscRegs;
const int TotalNumRegs = TheISA::TotalNumRegs;
const int VMPageSize   = TheISA::VMPageSize;
const int LogVMPageSize   = TheISA::LogVMPageSize;
const int ZeroReg = TheISA::ZeroReg;
const int StackPointerReg = TheISA::StackPointerReg;
const int GlobalPointerReg = TheISA::GlobalPointerReg;
const int ReturnAddressReg = TheISA::ReturnAddressReg;
const int ReturnValueReg = TheISA::ReturnValueReg;
const int ArgumentReg0 = TheISA::ArgumentReg0;
const int ArgumentReg1 = TheISA::ArgumentReg1;
const int ArgumentReg2 = TheISA::ArgumentReg2;
const int BranchPredAddrShiftAmt = TheISA::BranchPredAddrShiftAmt;
const int MaxAddr = (Addr)-1;

#if !FULL_SYSTEM
class SyscallReturn {
        public:
           template <class T>
           SyscallReturn(T v, bool s)
           {
               retval = (uint64_t)v;
               success = s;
           }

           template <class T>
           SyscallReturn(T v)
           {
               success = (v >= 0);
               retval = (uint64_t)v;
           }
           
           ~SyscallReturn() {}
           
           SyscallReturn& operator=(const SyscallReturn& s) {
               retval = s.retval;
               success = s.success;
               return *this;
           }

           bool successful() { return success; }
           uint64_t value() { return retval; } 


       private:
           uint64_t retval;
           bool success;
};

#endif


#if FULL_SYSTEM
typedef TheISA::InternalProcReg InternalProcReg;
const int NumInternalProcRegs  = TheISA::NumInternalProcRegs;
const int NumInterruptLevels = TheISA::NumInterruptLevels;

#include "arch/alpha/ev5.hh"
#endif

#endif // __ARCH_ALPHA_ISA_TRAITS_HH__
