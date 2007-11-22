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

/* @file
 * A single PCI device configuration space entry.
 */

#include <list>
#include <sstream>
#include <string>
#include <vector>

#include "base/inifile.hh"
#include "base/misc.hh"
#include "base/str.hh"	// for to_number
#include "base/trace.hh"
#include "dev/pcidev.hh"
#include "dev/pciconfigall.hh"
#include "mem/bus/bus.hh"
#include "mem/functional/memory_control.hh"
#include "sim/builder.hh"
#include "sim/param.hh"
#include "sim/root.hh"
#include "dev/tsunamireg.h"

using namespace std;

PciDev::PciDev(Params *p)
    : DmaDevice(p->name, p->plat), _params(p), plat(p->plat),
      configData(p->configData)
{
    // copy the config data from the PciConfigData object
    if (configData) {
        memcpy(config.data, configData->config.data, sizeof(config.data));
        memcpy(BARSize, configData->BARSize, sizeof(BARSize));
        memcpy(BARAddrs, configData->BARAddrs, sizeof(BARAddrs));
    } else
        panic("NULL pointer to configuration data");

    // Setup pointer in config space to point to this entry
    if (p->configSpace->deviceExists(p->deviceNum, p->functionNum)) 
        panic("Two PCI devices occuping same dev: %#x func: %#x",
	      p->deviceNum, p->functionNum); 
    else 
        p->configSpace->registerDevice(p->deviceNum, p->functionNum, this);
}

void
PciDev::readConfig(int offset, int size, uint8_t *data)
{
    if (offset >= PCI_DEVICE_SPECIFIC)
        panic("Device specific PCI config space not implemented!\n");

    switch(size) {
      case sizeof(uint8_t):
        *data = config.data[offset];
        break;
      case sizeof(uint16_t):
        *(uint16_t*)data = *(uint16_t*)&config.data[offset];
        break;
      case sizeof(uint32_t):
        *(uint32_t*)data = *(uint32_t*)&config.data[offset];
        break;
      default:
        panic("Invalid PCI configuration read size!\n");
    }

    DPRINTF(PCIDEV,
            "read device: %#x function: %#x register: %#x %d bytes: data: %#x\n",
            params()->deviceNum, params()->functionNum, offset, size,
            *(uint32_t*)data);
}

void
PciDev::writeConfig(int offset, int size, const uint8_t *data)
{
    if (offset >= PCI_DEVICE_SPECIFIC)
        panic("Device specific PCI config space not implemented!\n");

    uint8_t &data8 = *(uint8_t*)data;
    uint16_t &data16 = *(uint16_t*)data;
    uint32_t &data32 = *(uint32_t*)data;

    DPRINTF(PCIDEV, 
            "write device: %#x function: %#x reg: %#x size: %d data: %#x\n",
            params()->deviceNum, params()->functionNum, offset, size, data32);

    switch (size) {
      case sizeof(uint8_t): // 1-byte access
	switch (offset) {
	  case PCI0_INTERRUPT_LINE:
            config.interruptLine = data8;
          case PCI_CACHE_LINE_SIZE:
            config.cacheLineSize = data8;
          case PCI_LATENCY_TIMER:
	    config.latencyTimer = data8;
	    break;
          /* Do nothing for these read-only registers */
          case PCI0_INTERRUPT_PIN:
	  case PCI0_MINIMUM_GRANT:
	  case PCI0_MAXIMUM_LATENCY:
	  case PCI_CLASS_CODE:
	  case PCI_REVISION_ID:
	    break;
	  default:
	    panic("writing to a read only register");
	}
	break;

      case sizeof(uint16_t): // 2-byte access
	switch (offset) {
	  case PCI_COMMAND:
            config.command = data16;
	  case PCI_STATUS:
            config.status = data16;
          case PCI_CACHE_LINE_SIZE:
	    config.cacheLineSize = data16;
	    break;
	  default:
	    panic("writing to a read only register");
	}
	break;

      case sizeof(uint32_t): // 4-byte access
	switch (offset) {
	  case PCI0_BASE_ADDR0:
	  case PCI0_BASE_ADDR1:
	  case PCI0_BASE_ADDR2:
	  case PCI0_BASE_ADDR3:
	  case PCI0_BASE_ADDR4:
	  case PCI0_BASE_ADDR5:
            
            uint32_t barnum, bar_mask;
            Addr base_addr, base_size, space_base;

            barnum = BAR_NUMBER(offset);
            
            if (BAR_IO_SPACE(letoh(config.baseAddr[barnum]))) {
                bar_mask = BAR_IO_MASK;
                space_base = TSUNAMI_PCI0_IO;
            } else {
                bar_mask = BAR_MEM_MASK;
                space_base = TSUNAMI_PCI0_MEMORY;
            }

            // Writing 0xffffffff to a BAR tells the card to set the 
            // value of the bar to size of memory it needs
            if (letoh(data32) == 0xffffffff) {
                // This is I/O Space, bottom two bits are read only

                config.baseAddr[barnum] = letoh(
                        (~(BARSize[barnum] - 1) & ~bar_mask) |
                        (letoh(config.baseAddr[barnum]) & bar_mask));
            } else {
		MemoryController *mmu = params()->mmu;

	        config.baseAddr[barnum] = letoh(
                    (letoh(data32) & ~bar_mask) |
                    (letoh(config.baseAddr[barnum]) & bar_mask));

                if (letoh(config.baseAddr[barnum]) & ~bar_mask) {
                    base_addr = (letoh(data32) & ~bar_mask) + space_base;
                    base_size = BARSize[barnum];

                    // It's never been set
                    if (BARAddrs[barnum] == 0)
                        mmu->add_child((FunctionalMemory *)this, 
                                       RangeSize(base_addr, base_size));
                    else
                        mmu->update_child((FunctionalMemory *)this, 
                                          RangeSize(BARAddrs[barnum], base_size),
                                          RangeSize(base_addr, base_size));

                    BARAddrs[barnum] = base_addr;
                }
            }
	    break;

	  case PCI0_ROM_BASE_ADDR:
            if (letoh(data32) == 0xfffffffe)
                config.expansionROM = htole((uint32_t)0xffffffff); 
            else
                config.expansionROM = data32;
            break;

          case PCI_COMMAND:
            // This could also clear some of the error bits in the Status 
            // register. However they should never get set, so lets ignore 
            // it for now
            config.command = data16;
            break;

	  default:
            DPRINTF(PCIDEV, "Writing to a read only register");
	}
	break;

      default:
        panic("invalid access size");
    }
}

void
PciDev::serialize(ostream &os)
{
    SERIALIZE_ARRAY(BARSize, sizeof(BARSize) / sizeof(BARSize[0]));
    SERIALIZE_ARRAY(BARAddrs, sizeof(BARAddrs) / sizeof(BARAddrs[0]));
    SERIALIZE_ARRAY(config.data, sizeof(config.data) / sizeof(config.data[0]));
}

void
PciDev::unserialize(Checkpoint *cp, const std::string &section)
{
    UNSERIALIZE_ARRAY(BARSize, sizeof(BARSize) / sizeof(BARSize[0]));
    UNSERIALIZE_ARRAY(BARAddrs, sizeof(BARAddrs) / sizeof(BARAddrs[0]));
    UNSERIALIZE_ARRAY(config.data,
                      sizeof(config.data) / sizeof(config.data[0]));

    // Add the MMU mappings for the BARs
    for (int i=0; i < 6; i++) {
        if (BARAddrs[i] != 0)
            params()->mmu->add_child(this, RangeSize(BARAddrs[i], BARSize[i]));
    }
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(PciConfigData)

    Param<uint16_t> VendorID;
    Param<uint16_t> DeviceID;
    Param<uint16_t> Command;
    Param<uint16_t> Status;
    Param<uint8_t> Revision;
    Param<uint8_t> ProgIF;
    Param<uint8_t> SubClassCode;
    Param<uint8_t> ClassCode;
    Param<uint8_t> CacheLineSize;
    Param<uint8_t> LatencyTimer;
    Param<uint8_t> HeaderType;
    Param<uint8_t> BIST;
    Param<uint32_t> BAR0;
    Param<uint32_t> BAR1;
    Param<uint32_t> BAR2;
    Param<uint32_t> BAR3;
    Param<uint32_t> BAR4;
    Param<uint32_t> BAR5;
    Param<uint32_t> CardbusCIS;
    Param<uint16_t> SubsystemVendorID;
    Param<uint16_t> SubsystemID;
    Param<uint32_t> ExpansionROM;
    Param<uint8_t> InterruptLine;
    Param<uint8_t> InterruptPin;
    Param<uint8_t> MinimumGrant;
    Param<uint8_t> MaximumLatency;
    Param<uint32_t> BAR0Size;
    Param<uint32_t> BAR1Size;
    Param<uint32_t> BAR2Size;
    Param<uint32_t> BAR3Size;
    Param<uint32_t> BAR4Size;
    Param<uint32_t> BAR5Size;
    
END_DECLARE_SIM_OBJECT_PARAMS(PciConfigData)

BEGIN_INIT_SIM_OBJECT_PARAMS(PciConfigData)

    INIT_PARAM(VendorID, "Vendor ID"),
    INIT_PARAM(DeviceID, "Device ID"),
    INIT_PARAM_DFLT(Command, "Command Register", 0x00),
    INIT_PARAM_DFLT(Status, "Status Register", 0x00),
    INIT_PARAM_DFLT(Revision, "Device Revision", 0x00),
    INIT_PARAM_DFLT(ProgIF, "Programming Interface", 0x00),
    INIT_PARAM(SubClassCode, "Sub-Class Code"),
    INIT_PARAM(ClassCode, "Class Code"),
    INIT_PARAM_DFLT(CacheLineSize, "System Cacheline Size", 0x00),
    INIT_PARAM_DFLT(LatencyTimer, "PCI Latency Timer", 0x00),
    INIT_PARAM_DFLT(HeaderType, "PCI Header Type", 0x00),
    INIT_PARAM_DFLT(BIST, "Built In Self Test", 0x00),
    INIT_PARAM_DFLT(BAR0, "Base Address Register 0", 0x00),
    INIT_PARAM_DFLT(BAR1, "Base Address Register 1", 0x00),
    INIT_PARAM_DFLT(BAR2, "Base Address Register 2", 0x00),
    INIT_PARAM_DFLT(BAR3, "Base Address Register 3", 0x00),
    INIT_PARAM_DFLT(BAR4, "Base Address Register 4", 0x00),
    INIT_PARAM_DFLT(BAR5, "Base Address Register 5", 0x00),
    INIT_PARAM_DFLT(CardbusCIS, "Cardbus Card Information Structure", 0x00),
    INIT_PARAM_DFLT(SubsystemVendorID, "Subsystem Vendor ID", 0x00),
    INIT_PARAM_DFLT(SubsystemID, "Subsystem ID", 0x00),
    INIT_PARAM_DFLT(ExpansionROM, "Expansion ROM Base Address Register", 0x00),
    INIT_PARAM(InterruptLine, "Interrupt Line Register"),
    INIT_PARAM(InterruptPin, "Interrupt Pin Register"),
    INIT_PARAM_DFLT(MinimumGrant, "Minimum Grant", 0x00),
    INIT_PARAM_DFLT(MaximumLatency, "Maximum Latency", 0x00),
    INIT_PARAM_DFLT(BAR0Size, "Base Address Register 0 Size", 0x00),
    INIT_PARAM_DFLT(BAR1Size, "Base Address Register 1 Size", 0x00),
    INIT_PARAM_DFLT(BAR2Size, "Base Address Register 2 Size", 0x00),
    INIT_PARAM_DFLT(BAR3Size, "Base Address Register 3 Size", 0x00),
    INIT_PARAM_DFLT(BAR4Size, "Base Address Register 4 Size", 0x00),
    INIT_PARAM_DFLT(BAR5Size, "Base Address Register 5 Size", 0x00)

END_INIT_SIM_OBJECT_PARAMS(PciConfigData)

CREATE_SIM_OBJECT(PciConfigData)
{
    PciConfigData *data = new PciConfigData(getInstanceName());

    data->config.vendor = htole(VendorID);
    data->config.device = htole(DeviceID);
    data->config.command = htole(Command);
    data->config.status = htole(Status);
    data->config.revision = htole(Revision);
    data->config.progIF = htole(ProgIF);
    data->config.subClassCode = htole(SubClassCode);
    data->config.classCode = htole(ClassCode);
    data->config.cacheLineSize = htole(CacheLineSize);
    data->config.latencyTimer = htole(LatencyTimer);
    data->config.headerType = htole(HeaderType);
    data->config.bist = htole(BIST);

    data->config.baseAddr0 = htole(BAR0);
    data->config.baseAddr1 = htole(BAR1);
    data->config.baseAddr2 = htole(BAR2);
    data->config.baseAddr3 = htole(BAR3);
    data->config.baseAddr4 = htole(BAR4);
    data->config.baseAddr5 = htole(BAR5);
    data->config.cardbusCIS = htole(CardbusCIS);
    data->config.subsystemVendorID = htole(SubsystemVendorID);
    data->config.subsystemID = htole(SubsystemVendorID);
    data->config.expansionROM = htole(ExpansionROM);
    data->config.interruptLine = htole(InterruptLine);
    data->config.interruptPin = htole(InterruptPin);
    data->config.minimumGrant = htole(MinimumGrant);
    data->config.maximumLatency = htole(MaximumLatency);

    data->BARSize[0] = BAR0Size;
    data->BARSize[1] = BAR1Size;
    data->BARSize[2] = BAR2Size;
    data->BARSize[3] = BAR3Size;
    data->BARSize[4] = BAR4Size;
    data->BARSize[5] = BAR5Size;

    return data;
}

REGISTER_SIM_OBJECT("PciConfigData", PciConfigData)

#endif // DOXYGEN_SHOULD_SKIP_THIS
