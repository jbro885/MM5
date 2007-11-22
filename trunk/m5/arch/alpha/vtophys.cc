/*
 * Copyright (c) 2002, 2003, 2004, 2005
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

#include <string>

#include "arch/alpha/vtophys.hh"
#include "base/trace.hh"
#include "cpu/exec_context.hh"
#include "mem/functional/physical.hh"

using namespace std;

AlphaISA::PageTableEntry
kernel_pte_lookup(PhysicalMemory *pmem, Addr ptbr, AlphaISA::VAddr vaddr)
{
    Addr level1_pte = ptbr + vaddr.level1();
    AlphaISA::PageTableEntry level1 = pmem->phys_read_qword(level1_pte);
    if (!level1.valid()) {
	DPRINTF(VtoPhys, "level 1 PTE not valid, va = %#\n", vaddr);
	return 0;
    }

    Addr level2_pte = level1.paddr() + vaddr.level2();
    AlphaISA::PageTableEntry level2 = pmem->phys_read_qword(level2_pte);
    if (!level2.valid()) {
	DPRINTF(VtoPhys, "level 2 PTE not valid, va = %#x\n", vaddr);
	return 0;
    }

    Addr level3_pte = level2.paddr() + vaddr.level3();
    AlphaISA::PageTableEntry level3 = pmem->phys_read_qword(level3_pte);
    if (!level3.valid()) {
	DPRINTF(VtoPhys, "level 3 PTE not valid, va = %#x\n", vaddr);
	return 0;
    }
    return level3;
}

Addr
vtophys(PhysicalMemory *xc, Addr vaddr)
{
    Addr paddr = 0;
    if (AlphaISA::IsUSeg(vaddr))
	DPRINTF(VtoPhys, "vtophys: invalid vaddr %#x", vaddr);
    else if (AlphaISA::IsK0Seg(vaddr))
	paddr = AlphaISA::K0Seg2Phys(vaddr);
    else
	panic("vtophys: ptbr is not set on virtual lookup");

    DPRINTF(VtoPhys, "vtophys(%#x) -> %#x\n", vaddr, paddr);

    return paddr;
}

Addr
vtophys(ExecContext *xc, Addr addr)
{
    AlphaISA::VAddr vaddr = addr;
    Addr ptbr = xc->regs.ipr[AlphaISA::IPR_PALtemp20];
    Addr paddr = 0;
    //@todo Andrew couldn't remember why he commented some of this code
    //so I put it back in. Perhaps something to do with gdb debugging?
    if (AlphaISA::PcPAL(vaddr) && (vaddr < EV5::PalMax)) {
        paddr = vaddr & ~ULL(1);
    } else {
        if (AlphaISA::IsK0Seg(vaddr)) {
            paddr = AlphaISA::K0Seg2Phys(vaddr);
        } else if (!ptbr) {
            paddr = vaddr;
        } else {
            AlphaISA::PageTableEntry pte =
		kernel_pte_lookup(xc->physmem, ptbr, vaddr);
            if (pte.valid())
                paddr = pte.paddr() | vaddr.offset();
        }
    }

    
    DPRINTF(VtoPhys, "vtophys(%#x) -> %#x\n", vaddr, paddr);

    return paddr;
}

uint8_t *
ptomem(ExecContext *xc, Addr paddr, size_t len)
{
    return xc->physmem->dma_addr(paddr, len);
}

uint8_t *
vtomem(ExecContext *xc, Addr vaddr, size_t len)
{
    Addr paddr = vtophys(xc, vaddr);
    return xc->physmem->dma_addr(paddr, len);
}

void
CopyOut(ExecContext *xc, void *dest, Addr src, size_t cplen)
{
    Addr paddr;
    char *dmaaddr;
    char *dst = (char *)dest;
    int len;

    paddr = vtophys(xc, src);
    len = min((int)(AlphaISA::PageBytes - (paddr & AlphaISA::PageOffset)),
	      (int)cplen);
    dmaaddr = (char *)xc->physmem->dma_addr(paddr, len);
    assert(dmaaddr);

    memcpy(dst, dmaaddr, len);
    if (len == cplen)
	return;

    cplen -= len;
    dst += len;
    src += len;

    while (cplen > AlphaISA::PageBytes) {
	paddr = vtophys(xc, src);
	dmaaddr = (char *)xc->physmem->dma_addr(paddr, AlphaISA::PageBytes);
	assert(dmaaddr);

	memcpy(dst, dmaaddr, AlphaISA::PageBytes);
	cplen -= AlphaISA::PageBytes;
	dst += AlphaISA::PageBytes;
	src += AlphaISA::PageBytes;
    }

    if (cplen > 0) {
	paddr = vtophys(xc, src);
	dmaaddr = (char *)xc->physmem->dma_addr(paddr, cplen);
	assert(dmaaddr);

	memcpy(dst, dmaaddr, cplen);
    }
}

void
CopyIn(ExecContext *xc, Addr dest, void *source, size_t cplen)
{
    Addr paddr;
    char *dmaaddr;
    char *src = (char *)source;
    int len;

    paddr = vtophys(xc, dest);
    len = min((int)(AlphaISA::PageBytes - (paddr & AlphaISA::PageOffset)),
	      (int)cplen);
    dmaaddr = (char *)xc->physmem->dma_addr(paddr, len);
    assert(dmaaddr);

    memcpy(dmaaddr, src, len);
    if (len == cplen)
	return;

    cplen -= len;
    src += len;
    dest += len;

    while (cplen > AlphaISA::PageBytes) {
	paddr = vtophys(xc, dest);
	dmaaddr = (char *)xc->physmem->dma_addr(paddr, AlphaISA::PageBytes);
	assert(dmaaddr);

	memcpy(dmaaddr, src, AlphaISA::PageBytes);
	cplen -= AlphaISA::PageBytes;
	src += AlphaISA::PageBytes;
	dest += AlphaISA::PageBytes;
    }

    if (cplen > 0) {
	paddr = vtophys(xc, dest);
	dmaaddr = (char *)xc->physmem->dma_addr(paddr, cplen);
	assert(dmaaddr);

	memcpy(dmaaddr, src, cplen);
    }
}

void
CopyString(ExecContext *xc, char *dst, Addr vaddr, size_t maxlen)
{
    Addr paddr;
    char *dmaaddr;
    int len;

    paddr = vtophys(xc, vaddr);
    len = min((int)(AlphaISA::PageBytes - (paddr & AlphaISA::PageOffset)),
	      (int)maxlen);
    dmaaddr = (char *)xc->physmem->dma_addr(paddr, len);
    assert(dmaaddr);

    char *term = (char *)memchr(dmaaddr, 0, len);
    if (term)
	len = term - dmaaddr + 1;

    memcpy(dst, dmaaddr, len);

    if (term || len == maxlen)
	return;

    maxlen -= len;
    dst += len;
    vaddr += len;

    while (maxlen > AlphaISA::PageBytes) {
	paddr = vtophys(xc, vaddr);
	dmaaddr = (char *)xc->physmem->dma_addr(paddr, AlphaISA::PageBytes);
	assert(dmaaddr);

	char *term = (char *)memchr(dmaaddr, 0, AlphaISA::PageBytes);
	len = term ? (term - dmaaddr + 1) : AlphaISA::PageBytes;

	memcpy(dst, dmaaddr, len);
	if (term)
	    return;

	maxlen -= AlphaISA::PageBytes;
	dst += AlphaISA::PageBytes;
	vaddr += AlphaISA::PageBytes;
    }

    if (maxlen > 0) {
	paddr = vtophys(xc, vaddr);
	dmaaddr = (char *)xc->physmem->dma_addr(paddr, maxlen);
	assert(dmaaddr);

	char *term = (char *)memchr(dmaaddr, 0, maxlen);
	len = term ? (term - dmaaddr + 1) : maxlen;

	memcpy(dst, dmaaddr, len);

	maxlen -= len;
    }

    if (maxlen == 0)
	dst[maxlen] = '\0';
}
