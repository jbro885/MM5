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
 * Device model for an IDE disk
 */

#ifndef __IDE_DISK_HH__
#define __IDE_DISK_HH__

#include "dev/disk_image.hh"
#include "dev/ide_atareg.h"
#include "dev/ide_ctrl.hh"
#include "dev/ide_wdcreg.h"
#include "dev/io_device.hh"
#include "sim/eventq.hh"

#define DMA_BACKOFF_PERIOD 200

#define MAX_DMA_SIZE    (65536)  // 64K
#define MAX_MULTSECT    (128)

#define PRD_BASE_MASK  0xfffffffe
#define PRD_COUNT_MASK 0xfffe
#define PRD_EOT_MASK   0x8000

typedef struct PrdEntry {
    uint32_t baseAddr;
    uint16_t byteCount;
    uint16_t endOfTable;
} PrdEntry_t;

class PrdTableEntry {
  public:
    PrdEntry_t entry;

    uint32_t getBaseAddr() 
    { 
        return (entry.baseAddr & PRD_BASE_MASK); 
    }

    uint32_t getByteCount() 
    { 
        return ((entry.byteCount == 0) ? MAX_DMA_SIZE : 
                (entry.byteCount & PRD_COUNT_MASK));
    } 

    uint16_t getEOT() 
    { 
        return (entry.endOfTable & PRD_EOT_MASK); 
    }
};

#define DATA_OFFSET     (0)
#define ERROR_OFFSET    (1)
#define FEATURES_OFFSET (1)
#define NSECTOR_OFFSET  (2)
#define SECTOR_OFFSET   (3)
#define LCYL_OFFSET     (4)
#define HCYL_OFFSET     (5)
#define SELECT_OFFSET   (6)
#define DRIVE_OFFSET    (6)
#define STATUS_OFFSET   (7)
#define COMMAND_OFFSET  (7)

#define CONTROL_OFFSET  (2)
#define ALTSTAT_OFFSET  (2)

#define SELECT_DEV_BIT  0x10
#define CONTROL_RST_BIT 0x04
#define CONTROL_IEN_BIT 0x02
#define STATUS_BSY_BIT  0x80
#define STATUS_DRDY_BIT 0x40
#define STATUS_DRQ_BIT  0x08
#define STATUS_SEEK_BIT 0x10
#define STATUS_DF_BIT   0x20
#define DRIVE_LBA_BIT   0x40

#define DEV0 (0)
#define DEV1 (1)

typedef struct CommandReg {
    uint16_t data;
    uint8_t error;
    uint8_t sec_count;
    uint8_t sec_num;
    uint8_t cyl_low;
    uint8_t cyl_high;
    union {
        uint8_t drive;
        uint8_t head;
    };
    uint8_t command;
} CommandReg_t;

typedef enum Events {
    None = 0,
    Transfer,
    ReadWait,
    WriteWait,
    PrdRead,
    DmaRead,
    DmaWrite
} Events_t;

typedef enum DevAction {
    ACT_NONE = 0,
    ACT_CMD_WRITE,
    ACT_CMD_COMPLETE,
    ACT_CMD_ERROR,
    ACT_SELECT_WRITE,
    ACT_STAT_READ,
    ACT_DATA_READY,
    ACT_DATA_READ_BYTE,
    ACT_DATA_READ_SHORT,
    ACT_DATA_WRITE_BYTE,
    ACT_DATA_WRITE_SHORT,
    ACT_DMA_READY,
    ACT_DMA_DONE,
    ACT_SRST_SET,
    ACT_SRST_CLEAR
} DevAction_t;

typedef enum DevState {
    // Device idle
    Device_Idle_S = 0,
    Device_Idle_SI,
    Device_Idle_NS,

    // Software reset
    Device_Srst,

    // Non-data commands
    Command_Execution,

    // PIO data-in (data to host)
    Prepare_Data_In,
    Data_Ready_INTRQ_In,
    Transfer_Data_In,

    // PIO data-out (data from host)
    Prepare_Data_Out,
    Data_Ready_INTRQ_Out,
    Transfer_Data_Out,

    // DMA protocol
    Prepare_Data_Dma,
    Transfer_Data_Dma
} DevState_t;

typedef enum DmaState {
    Dma_Idle = 0,
    Dma_Start,
    Dma_Transfer
} DmaState_t;

class PhysicalMemory;
class IdeController;

/**
 * IDE Disk device model
 */
class IdeDisk : public SimObject
{
  protected:
    /** The IDE controller for this disk. */
    IdeController *ctrl;
    /** The DMA interface to use for transfers */
    DMAInterface<Bus> *dmaInterface;
    /** The image that contains the data of this disk. */
    DiskImage *image;
    /** Pointer to physical memory for DMA transfers */
    PhysicalMemory *physmem;

  protected:
    /** The disk delay in microseconds. */
    int diskDelay;

  private:
    /** Drive identification structure for this disk */
    struct ataparams driveID;
    /** Data buffer for transfers */
    uint8_t *dataBuffer;
    /** Number of bytes in command data transfer */
    uint32_t cmdBytes;
    /** Number of bytes left in command data transfer */
    uint32_t cmdBytesLeft;
    /** Number of bytes left in DRQ block */
    uint32_t drqBytesLeft;
    /** Current sector in access */
    uint32_t curSector;
    /** Command block registers */
    CommandReg_t cmdReg;
    /** Status register */
    uint8_t status;
    /** Interrupt enable bit */
    bool nIENBit;
    /** Device state */
    DevState_t devState;
    /** Dma state */
    DmaState_t dmaState;
    /** Dma transaction is a read */
    bool dmaRead;
    /** PRD table base address */
    uint32_t curPrdAddr;
    /** PRD entry */
    PrdTableEntry curPrd;
    /** Number of bytes transfered by DMA interface for current transfer */
    uint32_t dmaInterfaceBytes;
    /** Device ID (master=0/slave=1) */
    int devID;
    /** Interrupt pending */
    bool intrPending;

  public:
    /**
     * Create and initialize this Disk.
     * @param name The name of this disk.
     * @param img The disk image of this disk.
     * @param phys Pointer to physical memory
     * @param id The disk ID (master=0/slave=1)
     * @param disk_delay The disk delay in milliseconds
     */
    IdeDisk(const std::string &name, DiskImage *img, PhysicalMemory *phys,
            int id, Tick disk_delay);

    /**
     * Delete the data buffer.
     */
    ~IdeDisk();

    /**
     * Reset the device state
     */
    void reset(int id);

    /**
     * Set the controller for this device
     * @param c The IDE controller
     */
    void setController(IdeController *c, DMAInterface<Bus> *dmaIntr) {
        if (ctrl) panic("Cannot change the controller once set!\n");
        ctrl = c;
        dmaInterface = dmaIntr;
    }

    // Device register read/write
    void read(const Addr &offset, IdeRegType regtype, uint8_t *data);
    void write(const Addr &offset, IdeRegType regtype, const uint8_t *data);

    // Start/abort functions
    void startDma(const uint32_t &prdTableBase);
    void abortDma();

  private:
    void startCommand();

    // Interrupt management
    void intrPost();
    void intrClear();

    // DMA stuff
    void doDmaTransfer();
    friend class EventWrapper<IdeDisk, &IdeDisk::doDmaTransfer>;
    EventWrapper<IdeDisk, &IdeDisk::doDmaTransfer> dmaTransferEvent;

    void doDmaRead();
    friend class EventWrapper<IdeDisk, &IdeDisk::doDmaRead>;
    EventWrapper<IdeDisk, &IdeDisk::doDmaRead> dmaReadWaitEvent;

    void doDmaWrite();
    friend class EventWrapper<IdeDisk, &IdeDisk::doDmaWrite>;
    EventWrapper<IdeDisk, &IdeDisk::doDmaWrite> dmaWriteWaitEvent;

    void dmaPrdReadDone();
    friend class EventWrapper<IdeDisk, &IdeDisk::dmaPrdReadDone>;
    EventWrapper<IdeDisk, &IdeDisk::dmaPrdReadDone> dmaPrdReadEvent;

    void dmaReadDone();
    friend class EventWrapper<IdeDisk, &IdeDisk::dmaReadDone>;
    EventWrapper<IdeDisk, &IdeDisk::dmaReadDone> dmaReadEvent;

    void dmaWriteDone();
    friend class EventWrapper<IdeDisk, &IdeDisk::dmaWriteDone>;
    EventWrapper<IdeDisk, &IdeDisk::dmaWriteDone> dmaWriteEvent;

    // Disk image read/write
    void readDisk(uint32_t sector, uint8_t *data);
    void writeDisk(uint32_t sector, uint8_t *data);

    // State machine management
    void updateState(DevAction_t action);

    // Utility functions
    bool isBSYSet() { return (status & STATUS_BSY_BIT); }
    bool isIENSet() { return nIENBit; }
    bool isDEVSelect();

    void setComplete()
    {
        // clear out the status byte
        status = 0;
        // set the DRDY bit
        status |= STATUS_DRDY_BIT;
        // set the SEEK bit
        status |= STATUS_SEEK_BIT;
    }

    uint32_t getLBABase() 
    {
        return  (Addr)(((cmdReg.head & 0xf) << 24) | (cmdReg.cyl_high << 16) | 
                       (cmdReg.cyl_low << 8) | (cmdReg.sec_num));
    }

    inline Addr pciToDma(Addr pciAddr);

    uint32_t bytesInDmaPage(Addr curAddr, uint32_t bytesLeft);

    /**
     * Serialize this object to the given output stream.
     * @param os The stream to serialize to.
     */
    void serialize(std::ostream &os);

    /**
     * Reconstruct the state of this object from a checkpoint.
     * @param cp The checkpoint to use.
     * @param section The section name describing this object.
     */
    void unserialize(Checkpoint *cp, const std::string &section);
};


#endif // __IDE_DISK_HH__
