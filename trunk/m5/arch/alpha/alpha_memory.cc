/*
 * Copyright (c) 2001, 2002, 2003, 2004, 2005
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

#include <sstream>
#include <string>
#include <vector>

#include "arch/alpha/alpha_memory.hh"
#include "base/inifile.hh"
#include "base/str.hh"
#include "base/trace.hh"
#include "config/alpha_tlaser.hh"
#include "cpu/exec_context.hh"
#include "sim/builder.hh"

using namespace std;
using namespace EV5;

///////////////////////////////////////////////////////////////////////
//
//  Alpha TLB
//
#ifdef DEBUG
bool uncacheBit39 = false;
bool uncacheBit40 = false;
#endif

#define MODE2MASK(X)			(1 << (X))

AlphaTLB::AlphaTLB(const string &name, int s)
    : SimObject(name), size(s), nlu(0)
{
    table = new AlphaISA::PTE[size];
    memset(table, 0, sizeof(AlphaISA::PTE[size]));
}

AlphaTLB::~AlphaTLB()
{
    if (table)
	delete [] table;
}

// look up an entry in the TLB
AlphaISA::PTE *
AlphaTLB::lookup(Addr vpn, uint8_t asn) const
{
    // assume not found...
    AlphaISA::PTE *retval = NULL;

    PageTable::const_iterator i = lookupTable.find(vpn);
    if (i != lookupTable.end()) {
	while (i->first == vpn) {
	    int index = i->second;
	    AlphaISA::PTE *pte = &table[index];
	    assert(pte->valid);
	    if (vpn == pte->tag && (pte->asma || pte->asn == asn)) {
		retval = pte;
		break;
	    }

	    ++i;
	}
    }

    DPRINTF(TLB, "lookup %#x, asn %#x -> %s ppn %#x\n", vpn, (int)asn,
	    retval ? "hit" : "miss", retval ? retval->ppn : 0);
    return retval;
}


void
AlphaTLB::checkCacheability(MemReqPtr &req)
{
    // in Alpha, cacheability is controlled by upper-level bits of the
    // physical address

    /*
     * We support having the uncacheable bit in either bit 39 or bit 40.
     * The Turbolaser platform (and EV5) support having the bit in 39, but
     * Tsunami (which Linux assumes uses an EV6) generates accesses with
     * the bit in 40.  So we must check for both, but we have debug flags
     * to catch a weird case where both are used, which shouldn't happen.
     */
   

#if ALPHA_TLASER
    if (req->paddr & PAddrUncachedBit39) {
#else
    if (req->paddr & PAddrUncachedBit43) {
#endif
        // IPR memory space not implemented
        if (PAddrIprSpace(req->paddr)) {
            if (!req->xc->misspeculating()) {
                switch (req->paddr) {
                  case ULL(0xFFFFF00188):
                    req->data = 0;
                    break;
                 
                  default:
                    panic("IPR memory space not implemented! PA=%x\n",
                          req->paddr);
                }
            }
        } else {
            // mark request as uncacheable
            req->flags |= UNCACHEABLE;

#if !ALPHA_TLASER
            // Clear bits 42:35 of the physical address (10-2 in Tsunami manual)
            req->paddr &= PAddrUncachedMask;
#endif
        }
    }
}
  

// insert a new TLB entry
void
AlphaTLB::insert(Addr addr, AlphaISA::PTE &pte)
{
    AlphaISA::VAddr vaddr = addr;
    if (table[nlu].valid) {
	Addr oldvpn = table[nlu].tag;
	PageTable::iterator i = lookupTable.find(oldvpn);

	if (i == lookupTable.end())
	    panic("TLB entry not found in lookupTable");

	int index;
	while ((index = i->second) != nlu) {
	    if (table[index].tag != oldvpn)
		panic("TLB entry not found in lookupTable");

	    ++i;
	}

	DPRINTF(TLB, "remove @%d: %#x -> %#x\n", nlu, oldvpn, table[nlu].ppn);

	lookupTable.erase(i);
    }

    DPRINTF(TLB, "insert @%d: %#x -> %#x\n", nlu, vaddr.vpn(), pte.ppn);

    table[nlu] = pte;
    table[nlu].tag = vaddr.vpn();
    table[nlu].valid = true;

    lookupTable.insert(make_pair(vaddr.vpn(), nlu));
    nextnlu();
}

void
AlphaTLB::flushAll()
{
    DPRINTF(TLB, "flushAll\n");
    memset(table, 0, sizeof(AlphaISA::PTE[size]));
    lookupTable.clear();
    nlu = 0;
}

void
AlphaTLB::flushProcesses()
{
    PageTable::iterator i = lookupTable.begin();
    PageTable::iterator end = lookupTable.end();
    while (i != end) {
	int index = i->second;
	AlphaISA::PTE *pte = &table[index];
    	assert(pte->valid);

	// we can't increment i after we erase it, so save a copy and
	// increment it to get the next entry now
	PageTable::iterator cur = i;
	++i;

	if (!pte->asma) {
	    DPRINTF(TLB, "flush @%d: %#x -> %#x\n", index, pte->tag, pte->ppn);
	    pte->valid = false;
	    lookupTable.erase(cur);
	}
    }
}

void
AlphaTLB::flushAddr(Addr addr, uint8_t asn)
{
    AlphaISA::VAddr vaddr = addr;

    PageTable::iterator i = lookupTable.find(vaddr.vpn());
    if (i == lookupTable.end())
	return;

    while (i->first == vaddr.vpn()) {
	int index = i->second;
	AlphaISA::PTE *pte = &table[index];
	assert(pte->valid);

	if (vaddr.vpn() == pte->tag && (pte->asma || pte->asn == asn)) {
	    DPRINTF(TLB, "flushaddr @%d: %#x -> %#x\n", index, vaddr.vpn(),
		    pte->ppn);

	    // invalidate this entry
	    pte->valid = false;

	    lookupTable.erase(i);
	}

	++i;
    }
}


void
AlphaTLB::serialize(ostream &os)
{
    SERIALIZE_SCALAR(size);
    SERIALIZE_SCALAR(nlu);

    for (int i = 0; i < size; i++) {
	nameOut(os, csprintf("%s.PTE%d", name(), i));
	table[i].serialize(os);
    }
}

void
AlphaTLB::unserialize(Checkpoint *cp, const string &section)
{
    UNSERIALIZE_SCALAR(size);
    UNSERIALIZE_SCALAR(nlu);

    for (int i = 0; i < size; i++) {
	table[i].unserialize(cp, csprintf("%s.PTE%d", section, i));
	if (table[i].valid) {
	    lookupTable.insert(make_pair(table[i].tag, i));
	}
    }
}


///////////////////////////////////////////////////////////////////////
//
//  Alpha ITB
//
AlphaITB::AlphaITB(const std::string &name, int size)
    : AlphaTLB(name, size)
{}


void
AlphaITB::regStats()
{
    hits
	.name(name() + ".hits")
	.desc("ITB hits");
    misses
	.name(name() + ".misses")
	.desc("ITB misses");
    acv
	.name(name() + ".acv")
	.desc("ITB acv");
    accesses
	.name(name() + ".accesses")
	.desc("ITB accesses");

    accesses = hits + misses;
}

void
AlphaITB::fault(Addr pc, ExecContext *xc) const
{
    uint64_t *ipr = xc->regs.ipr;

    if (!xc->misspeculating()) {
	ipr[AlphaISA::IPR_ITB_TAG] = pc;
	ipr[AlphaISA::IPR_IFAULT_VA_FORM] =
	    ipr[AlphaISA::IPR_IVPTBR] | (AlphaISA::VAddr(pc).vpn() << 3);
    }
}


Fault
AlphaITB::translate(MemReqPtr &req) const
{
    InternalProcReg *ipr = req->xc->regs.ipr;

    if (AlphaISA::PcPAL(req->vaddr)) {
	// strip off PAL PC marker (lsb is 1)
	req->paddr = (req->vaddr & ~3) & PAddrImplMask;
	hits++;
	return No_Fault;
    }

    if (req->flags & PHYSICAL) {
	req->paddr = req->vaddr;
    } else {
        // verify that this is a good virtual address
        if (!validVirtualAddress(req->vaddr)) {
            fault(req->vaddr, req->xc);
            acv++;
            return ITB_Acv_Fault;
        }


        // VA<42:41> == 2, VA<39:13> maps directly to PA<39:13> for EV5
        // VA<47:41> == 0x7e, VA<40:13> maps directly to PA<40:13> for EV6
#if ALPHA_TLASER
        if ((MCSR_SP(ipr[AlphaISA::IPR_MCSR]) & 2) &&
	    VAddrSpaceEV5(req->vaddr) == 2) {
#else
        if (VAddrSpaceEV6(req->vaddr) == 0x7e) {
#endif
            // only valid in kernel mode
            if (ICM_CM(ipr[AlphaISA::IPR_ICM]) !=
		AlphaISA::mode_kernel) {
                fault(req->vaddr, req->xc);
                acv++;
                return ITB_Acv_Fault;
            }

            req->paddr = req->vaddr & PAddrImplMask;

#if !ALPHA_TLASER
            // sign extend the physical address properly
            if (req->paddr & PAddrUncachedBit40)
                req->paddr |= ULL(0xf0000000000);
            else
                req->paddr &= ULL(0xffffffffff);
#endif

        } else {
            // not a physical address: need to look up pte
            AlphaISA::PTE *pte = lookup(AlphaISA::VAddr(req->vaddr).vpn(),
					DTB_ASN_ASN(ipr[AlphaISA::IPR_DTB_ASN]));

            if (!pte) {
                fault(req->vaddr, req->xc);
                misses++;
                return ITB_Fault_Fault;
            }

            req->paddr = (pte->ppn << AlphaISA::PageShift) +
		(AlphaISA::VAddr(req->vaddr).offset() & ~3);
            
            // check permissions for this access
            if (!(pte->xre & (1 << ICM_CM(ipr[AlphaISA::IPR_ICM])))) {
                // instruction access fault
                fault(req->vaddr, req->xc);
                acv++;
                return ITB_Acv_Fault;
            }

            hits++;
        }
    }

    // check that the physical address is ok (catch bad physical addresses)
    if (req->paddr & ~PAddrImplMask)
        return Machine_Check_Fault;

    checkCacheability(req);

    return No_Fault;
}

///////////////////////////////////////////////////////////////////////
//
//  Alpha DTB
//
AlphaDTB::AlphaDTB(const std::string &name, int size)
    : AlphaTLB(name, size)
{}

void
AlphaDTB::regStats()
{
    read_hits
	.name(name() + ".read_hits")
	.desc("DTB read hits")
	;

    read_misses
	.name(name() + ".read_misses")
	.desc("DTB read misses")
	;

    read_acv
	.name(name() + ".read_acv")
	.desc("DTB read access violations")
	;

    read_accesses
	.name(name() + ".read_accesses")
	.desc("DTB read accesses")
	;

    write_hits
	.name(name() + ".write_hits")
	.desc("DTB write hits")
	;

    write_misses
	.name(name() + ".write_misses")
	.desc("DTB write misses")
	;

    write_acv
	.name(name() + ".write_acv")
	.desc("DTB write access violations")
	;

    write_accesses
	.name(name() + ".write_accesses")
	.desc("DTB write accesses")
	;

    hits
	.name(name() + ".hits")
	.desc("DTB hits")
	;

    misses
	.name(name() + ".misses")
	.desc("DTB misses")
	;

    acv
	.name(name() + ".acv")
	.desc("DTB access violations")
	;

    accesses
	.name(name() + ".accesses")
	.desc("DTB accesses")
	;

    hits = read_hits + write_hits;
    misses = read_misses + write_misses;
    acv = read_acv + write_acv;
    accesses = read_accesses + write_accesses;
}

void
AlphaDTB::fault(MemReqPtr &req, uint64_t flags) const
{
    ExecContext *xc = req->xc;
    AlphaISA::VAddr vaddr = req->vaddr;
    uint64_t *ipr = xc->regs.ipr;

    // Set fault address and flags.  Even though we're modeling an
    // EV5, we use the EV6 technique of not latching fault registers
    // on VPTE loads (instead of locking the registers until IPR_VA is
    // read, like the EV5).  The EV6 approach is cleaner and seems to
    // work with EV5 PAL code, but not the other way around.
    if (!xc->misspeculating()
	&& !(req->flags & VPTE) && !(req->flags & NO_FAULT)) {
	// set VA register with faulting address
	ipr[AlphaISA::IPR_VA] = req->vaddr;

	// set MM_STAT register flags
	ipr[AlphaISA::IPR_MM_STAT] =
	    (((Opcode(xc->getInst()) & 0x3f) << 11)
	     | ((Ra(xc->getInst()) & 0x1f) << 6)
	     | (flags & 0x3f));

	// set VA_FORM register with faulting formatted address
	ipr[AlphaISA::IPR_VA_FORM] =
	    ipr[AlphaISA::IPR_MVPTBR] | (vaddr.vpn() << 3);
    }
}

Fault
AlphaDTB::translate(MemReqPtr &req, bool write) const
{
    RegFile *regs = &req->xc->regs;
    Addr pc = regs->pc;
    InternalProcReg *ipr = regs->ipr;

    AlphaISA::mode_type mode =
	(AlphaISA::mode_type)DTB_CM_CM(ipr[AlphaISA::IPR_DTB_CM]);


    /**
     * Check for alignment faults
     */
    if (req->vaddr & (req->size - 1)) {
        fault(req, write ? MM_STAT_WR_MASK : 0);
        DPRINTF(TLB, "Alignment Fault on %#x, size = %d", req->vaddr, 
                req->size); 
        return Alignment_Fault;
    }
    
    if (pc & 0x1) {
	mode = (req->flags & ALTMODE) ?
	    (AlphaISA::mode_type)ALT_MODE_AM(ipr[AlphaISA::IPR_ALT_MODE])
	    : AlphaISA::mode_kernel;
    }

    if (req->flags & PHYSICAL) {
	req->paddr = req->vaddr;
    } else {
        // verify that this is a good virtual address
        if (!validVirtualAddress(req->vaddr)) {
            fault(req, (write ? MM_STAT_WR_MASK : 0) |
		  MM_STAT_BAD_VA_MASK |
		  MM_STAT_ACV_MASK);
            
            if (write) { write_acv++; } else { read_acv++; }
            return DTB_Fault_Fault;
        }

        // Check for "superpage" mapping
#if ALPHA_TLASER
        if ((MCSR_SP(ipr[AlphaISA::IPR_MCSR]) & 2) &&
	    VAddrSpaceEV5(req->vaddr) == 2) {
#else
        if (VAddrSpaceEV6(req->vaddr) == 0x7e) {
#endif

            // only valid in kernel mode
            if (DTB_CM_CM(ipr[AlphaISA::IPR_DTB_CM]) != 
                AlphaISA::mode_kernel) {
                fault(req, ((write ? MM_STAT_WR_MASK : 0) |
			    MM_STAT_ACV_MASK));
                if (write) { write_acv++; } else { read_acv++; }
                return DTB_Acv_Fault;
            }

            req->paddr = req->vaddr & PAddrImplMask;

#if !ALPHA_TLASER
            // sign extend the physical address properly
            if (req->paddr & PAddrUncachedBit40)
                req->paddr |= ULL(0xf0000000000);
            else
                req->paddr &= ULL(0xffffffffff);
#endif

        } else {
            if (write)
                write_accesses++;
            else
                read_accesses++;

            // not a physical address: need to look up pte
            AlphaISA::PTE *pte = lookup(AlphaISA::VAddr(req->vaddr).vpn(),
					DTB_ASN_ASN(ipr[AlphaISA::IPR_DTB_ASN]));

            if (!pte) {
                // page fault
                fault(req, (write ? MM_STAT_WR_MASK : 0) |
		      MM_STAT_DTB_MISS_MASK);
                if (write) { write_misses++; } else { read_misses++; }
                return (req->flags & VPTE) ? Pdtb_Miss_Fault : Ndtb_Miss_Fault;
            }
            
            req->paddr = (pte->ppn << AlphaISA::PageShift) +
		AlphaISA::VAddr(req->vaddr).offset();

            if (write) {
                if (!(pte->xwe & MODE2MASK(mode))) {
                    // declare the instruction access fault
                    fault(req, MM_STAT_WR_MASK |
			  MM_STAT_ACV_MASK |
			  (pte->fonw ? MM_STAT_FONW_MASK : 0));
                    write_acv++;
                    return DTB_Fault_Fault;
                }
                if (pte->fonw) {
                    fault(req, MM_STAT_WR_MASK |
			  MM_STAT_FONW_MASK);
                    write_acv++;
                    return DTB_Fault_Fault;
                }
            } else {
                if (!(pte->xre & MODE2MASK(mode))) {
                    fault(req, MM_STAT_ACV_MASK | 
			  (pte->fonr ? MM_STAT_FONR_MASK : 0));
                    read_acv++;
                    return DTB_Acv_Fault;
                }
                if (pte->fonr) {
                    fault(req, MM_STAT_FONR_MASK);
                    read_acv++;
                    return DTB_Fault_Fault;
                }
            }
        }

        if (write)
            write_hits++;
        else
            read_hits++;
    }

    // check that the physical address is ok (catch bad physical addresses)
    if (req->paddr & ~PAddrImplMask)
        return Machine_Check_Fault;

    checkCacheability(req);

    return No_Fault;
}

AlphaISA::PTE &
AlphaTLB::index(bool advance)
{
    AlphaISA::PTE *pte = &table[nlu];

    if (advance)
        nextnlu();

    return *pte;
}

DEFINE_SIM_OBJECT_CLASS_NAME("AlphaTLB", AlphaTLB)

BEGIN_DECLARE_SIM_OBJECT_PARAMS(AlphaITB)

    Param<int> size;

END_DECLARE_SIM_OBJECT_PARAMS(AlphaITB)

BEGIN_INIT_SIM_OBJECT_PARAMS(AlphaITB)

    INIT_PARAM_DFLT(size, "TLB size", 48)

END_INIT_SIM_OBJECT_PARAMS(AlphaITB)


CREATE_SIM_OBJECT(AlphaITB)
{
    return new AlphaITB(getInstanceName(), size);
}

REGISTER_SIM_OBJECT("AlphaITB", AlphaITB)

BEGIN_DECLARE_SIM_OBJECT_PARAMS(AlphaDTB)

    Param<int> size;

END_DECLARE_SIM_OBJECT_PARAMS(AlphaDTB)

BEGIN_INIT_SIM_OBJECT_PARAMS(AlphaDTB)

    INIT_PARAM_DFLT(size, "TLB size", 64)

END_INIT_SIM_OBJECT_PARAMS(AlphaDTB)


CREATE_SIM_OBJECT(AlphaDTB)
{
    return new AlphaDTB(getInstanceName(), size);
}

REGISTER_SIM_OBJECT("AlphaDTB", AlphaDTB)

