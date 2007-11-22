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

/** @file
 * Simple PCI IDE controller with bus mastering capability and UDMA
 * modeled after controller in the Intel PIIX4 chip
 */

#ifndef __IDE_CTRL_HH__
#define __IDE_CTRL_HH__

#include "dev/pcidev.hh"
#include "dev/pcireg.h"
#include "dev/io_device.hh"

#define BMIC0    0x0  // Bus master IDE command register
#define BMIS0    0x2  // Bus master IDE status register
#define BMIDTP0  0x4  // Bus master IDE descriptor table pointer register
#define BMIC1    0x8  // Bus master IDE command register
#define BMIS1    0xa  // Bus master IDE status register
#define BMIDTP1  0xc  // Bus master IDE descriptor table pointer register

// Bus master IDE command register bit fields
#define RWCON 0x08 // Bus master read/write control
#define SSBM  0x01 // Start/stop bus master

// Bus master IDE status register bit fields
#define DMA1CAP 0x40 // Drive 1 DMA capable
#define DMA0CAP 0x20 // Drive 0 DMA capable
#define IDEINTS 0x04 // IDE Interrupt Status
#define IDEDMAE 0x02 // IDE DMA error
#define BMIDEA  0x01 // Bus master IDE active

// IDE Command byte fields
#define IDE_SELECT_OFFSET       (6)
#define IDE_SELECT_DEV_BIT      0x10

#define IDE_FEATURE_OFFSET      IDE_ERROR_OFFSET
#define IDE_COMMAND_OFFSET      IDE_STATUS_OFFSET

// IDE Timing Register bit fields
#define IDETIM_DECODE_EN 0x8000

// PCI device specific register byte offsets
#define IDE_CTRL_CONF_START 0x40
#define IDE_CTRL_CONF_END ((IDE_CTRL_CONF_START) + sizeof(config_regs))


enum IdeRegType {
    COMMAND_BLOCK,
    CONTROL_BLOCK,
    BMI_BLOCK
};

class BaseInterface;
class Bus;
class HierParams;
class IdeDisk;
class IntrControl;
class PciConfigAll;
class PhysicalMemory;
class Platform;

/**
 * Device model for an Intel PIIX4 IDE controller
 */

class IdeController : public PciDev
{
    friend class IdeDisk;

    enum IdeChannel {
        PRIMARY = 0,
        SECONDARY = 1
    };

  private:
    /** Primary command block registers */
    Addr pri_cmd_addr;
    Addr pri_cmd_size;
    /** Primary control block registers */
    Addr pri_ctrl_addr;
    Addr pri_ctrl_size;
    /** Secondary command block registers */
    Addr sec_cmd_addr;
    Addr sec_cmd_size;
    /** Secondary control block registers */
    Addr sec_ctrl_addr;
    Addr sec_ctrl_size;
    /** Bus master interface (BMI) registers */
    Addr bmi_addr;
    Addr bmi_size;

  private:
    /** Registers used for bus master interface */
    union {
        uint8_t data[16];

        struct {
            uint8_t bmic0;
            uint8_t reserved_0;
            uint8_t bmis0;
            uint8_t reserved_1;
            uint32_t bmidtp0;
            uint8_t bmic1;
            uint8_t reserved_2;
            uint8_t bmis1;
            uint8_t reserved_3;
            uint32_t bmidtp1;
        };

        struct {
            uint8_t bmic;
            uint8_t reserved_4;
            uint8_t bmis;
            uint8_t reserved_5;
            uint32_t bmidtp;
        } chan[2];

    } bmi_regs;
    /** Shadows of the device select bit */
    uint8_t dev[2];
    /** Registers used in device specific PCI configuration */
    union {
        uint8_t data[22];

        struct {
            uint16_t idetim0;
            uint16_t idetim1;
            uint8_t sidetim;
            uint8_t reserved_0[3];
            uint8_t udmactl;
            uint8_t reserved_1;
            uint16_t udmatim;
            uint8_t reserved_2[8];
            uint16_t ideconfig;
        };
    } config_regs;

    // Internal management variables
    bool io_enabled;
    bool bm_enabled;
    bool cmd_in_progress[4];

  private:
    /** IDE disks connected to controller */
    IdeDisk *disks[4];

  private:
    /** Parse the access address to pass on to device */
    void parseAddr(const Addr &addr, Addr &offset, IdeChannel &channel, 
                   IdeRegType &reg_type);
                   
    /** Select the disk based on the channel and device bit */
    int getDisk(IdeChannel channel);

    /** Select the disk based on a pointer */
    int getDisk(IdeDisk *diskPtr);

  public:
    /** See if a disk is selected based on its pointer */
    bool isDiskSelected(IdeDisk *diskPtr);

  public:
    struct Params : public PciDev::Params
    {
	/** Array of disk objects */
	std::vector<IdeDisk *> disks;
	Bus *host_bus;
	Tick pio_latency;
	HierParams *hier;
    };
    const Params *params() const { return (const Params *)_params; }

  public:
    IdeController(Params *p);
    ~IdeController();

    virtual void writeConfig(int offset, int size, const uint8_t *data);
    virtual void readConfig(int offset, int size, uint8_t *data);

    void setDmaComplete(IdeDisk *disk);

    /**
     * Read a done field for a given target.
     * @param req Contains the address of the field to read.
     * @param data Return the field read.
     * @return The fault condition of the access.
     */
    virtual Fault read(MemReqPtr &req, uint8_t *data);
    
    /**
     * Write to the mmapped I/O control registers.
     * @param req Contains the address to write to.
     * @param data The data to write.
     * @return The fault condition of the access.
     */
    virtual Fault write(MemReqPtr &req, const uint8_t *data);

    /**
     * Serialize this object to the given output stream.
     * @param os The stream to serialize to.
     */
    virtual void serialize(std::ostream &os);

    /**
     * Reconstruct the state of this object from a checkpoint.
     * @param cp The checkpoint use.
     * @param section The section name of this object
     */
    virtual void unserialize(Checkpoint *cp, const std::string &section);

    /**
     * Return how long this access will take.
     * @param req the memory request to calcuate
     * @return Tick when the request is done
     */
    Tick cacheAccess(MemReqPtr &req);
};
#endif // __IDE_CTRL_HH_
