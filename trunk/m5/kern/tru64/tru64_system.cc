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

#include "base/loader/symtab.hh"
#include "base/trace.hh"
#include "cpu/base.hh"
#include "cpu/exec_context.hh"
#include "kern/tru64/tru64_events.hh"
#include "kern/tru64/tru64_system.hh"
#include "kern/system_events.hh"
#include "mem/functional/memory_control.hh"
#include "mem/functional/physical.hh"
#include "sim/builder.hh"
#include "targetarch/isa_traits.hh"
#include "targetarch/vtophys.hh"

using namespace std;

Tru64System::Tru64System(Tru64System::Params *p)
    : System(p)
{
    Addr addr = 0;
    if (kernelSymtab->findAddress("enable_async_printf", addr)) {
	Addr paddr = vtophys(physmem, addr);
	uint8_t *enable_async_printf =
	    physmem->dma_addr(paddr, sizeof(uint32_t));

	if (enable_async_printf)
	    *(uint32_t *)enable_async_printf = 0;
    }

#ifdef DEBUG
    kernelPanicEvent = addKernelFuncEvent<BreakPCEvent>("panic");
    if (!kernelPanicEvent)
	panic("could not find kernel symbol \'panic\'");
#endif

    badaddrEvent = addKernelFuncEvent<BadAddrEvent>("badaddr");
    if (!badaddrEvent)
	panic("could not find kernel symbol \'badaddr\'");

    skipPowerStateEvent =
	addKernelFuncEvent<SkipFuncEvent>("tl_v48_capture_power_state");
    skipScavengeBootEvent =
	addKernelFuncEvent<SkipFuncEvent>("pmap_scavenge_boot");

#if TRACING_ON
    printfEvent = addKernelFuncEvent<PrintfEvent>("printf");
    debugPrintfEvent = addKernelFuncEvent<DebugPrintfEvent>("m5printf");
    debugPrintfrEvent = addKernelFuncEvent<DebugPrintfrEvent>("m5printfr");
    dumpMbufEvent = addKernelFuncEvent<DumpMbufEvent>("m5_dump_mbuf");
#endif
}

Tru64System::~Tru64System()
{
#ifdef DEBUG
    delete kernelPanicEvent;
#endif
    delete badaddrEvent;
    delete skipPowerStateEvent;
    delete skipScavengeBootEvent;
#if TRACING_ON
    delete printfEvent;
    delete debugPrintfEvent;
    delete debugPrintfrEvent;
    delete dumpMbufEvent;
#endif
}

BEGIN_DECLARE_SIM_OBJECT_PARAMS(Tru64System)

    Param<Tick> boot_cpu_frequency;
    SimObjectParam<MemoryController *> memctrl;
    SimObjectParam<PhysicalMemory *> physmem;

    Param<string> kernel;
    Param<string> console;
    Param<string> pal;

    Param<string> boot_osflags;
    Param<string> readfile;
    Param<unsigned int> init_param;

    Param<uint64_t> system_type;
    Param<uint64_t> system_rev;

    Param<bool> bin;
    VectorParam<string> binned_fns;

END_DECLARE_SIM_OBJECT_PARAMS(Tru64System)

BEGIN_INIT_SIM_OBJECT_PARAMS(Tru64System)

    INIT_PARAM(boot_cpu_frequency, "frequency of the boot cpu"),
    INIT_PARAM(memctrl, "memory controller"),
    INIT_PARAM(physmem, "phsyical memory"),
    INIT_PARAM(kernel, "file that contains the kernel code"),
    INIT_PARAM(console, "file that contains the console code"),
    INIT_PARAM(pal, "file that contains palcode"),
    INIT_PARAM_DFLT(boot_osflags, "flags to pass to the kernel during boot",
		    "a"),
    INIT_PARAM_DFLT(readfile, "file to read startup script from", ""),
    INIT_PARAM_DFLT(init_param, "numerical value to pass into simulator", 0),
    INIT_PARAM_DFLT(system_type, "Type of system we are emulating", 12),
    INIT_PARAM_DFLT(system_rev, "Revision of system we are emulating", 2<<1),
    INIT_PARAM_DFLT(bin, "is this system to be binned", false),
    INIT_PARAM(binned_fns, "functions to be broken down and binned")

END_INIT_SIM_OBJECT_PARAMS(Tru64System)

CREATE_SIM_OBJECT(Tru64System)
{
    System::Params *p = new System::Params;
    p->name = getInstanceName();
    p->boot_cpu_frequency = boot_cpu_frequency;
    p->memctrl = memctrl;
    p->physmem = physmem;
    p->kernel_path = kernel;
    p->console_path = console;
    p->palcode = pal;
    p->boot_osflags = boot_osflags;
    p->init_param = init_param;
    p->readfile = readfile;
    p->system_type = system_type;
    p->system_rev = system_rev;
    p->bin = bin;
    p->binned_fns = binned_fns;
    p->bin_int = false;
    
    return new Tru64System(p);
}

REGISTER_SIM_OBJECT("Tru64System", Tru64System)
