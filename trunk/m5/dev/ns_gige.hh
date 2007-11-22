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
 * Device module for modelling the National Semiconductor
 * DP83820 ethernet controller
 */

#ifndef __DEV_NS_GIGE_HH__
#define __DEV_NS_GIGE_HH__

#include "base/inet.hh"
#include "base/statistics.hh"
#include "dev/etherint.hh"
#include "dev/etherpkt.hh"
#include "dev/io_device.hh"
#include "dev/ns_gige_reg.h"
#include "dev/pcidev.hh"
#include "dev/pktfifo.hh"
#include "mem/bus/bus.hh"
#include "sim/eventq.hh"

// Hash filtering constants
const uint16_t FHASH_ADDR  = 0x100;
const uint16_t FHASH_SIZE  = 0x100;

// EEPROM constants
const uint8_t  EEPROM_READ = 0x2;
const uint8_t  EEPROM_SIZE = 64; // Size in words of NSC93C46 EEPROM
const uint8_t  EEPROM_PMATCH2_ADDR = 0xA; // EEPROM Address of PMATCH word 2
const uint8_t  EEPROM_PMATCH1_ADDR = 0xB; // EEPROM Address of PMATCH word 1
const uint8_t  EEPROM_PMATCH0_ADDR = 0xC; // EEPROM Address of PMATCH word 0

/**
 * Ethernet device registers
 */
struct dp_regs {
    uint32_t	command;
    uint32_t	config;
    uint32_t	mear;
    uint32_t	ptscr;
    uint32_t    isr;
    uint32_t    imr;
    uint32_t    ier;
    uint32_t    ihr;
    uint32_t    txdp;
    uint32_t    txdp_hi;
    uint32_t    txcfg;
    uint32_t    gpior;
    uint32_t    rxdp;
    uint32_t    rxdp_hi;
    uint32_t    rxcfg;
    uint32_t    pqcr;
    uint32_t    wcsr;
    uint32_t    pcr;
    uint32_t    rfcr;
    uint32_t    rfdr;
    uint32_t    brar;
    uint32_t    brdr;
    uint32_t    srr;
    uint32_t    mibc;
    uint32_t    vrcr;
    uint32_t    vtcr;
    uint32_t    vdr;
    uint32_t    ccsr;
    uint32_t    tbicr;
    uint32_t    tbisr;
    uint32_t    tanar;
    uint32_t    tanlpar;
    uint32_t    taner;
    uint32_t    tesr;
};

struct dp_rom {
    /**
     * for perfect match memory.
     * the linux driver doesn't use any other ROM
     */
    uint8_t perfectMatch[ETH_ADDR_LEN];

    /**
     * for hash table memory.
     * used by the freebsd driver
     */
    uint8_t filterHash[FHASH_SIZE];
};

class NSGigEInt;
class PhysicalMemory;
class BaseInterface;
class HierParams;
class Bus;
class PciConfigAll;

/**
 * NS DP83820 Ethernet device model
 */
class NSGigE : public PciDev
{
  public:
    /** Transmit State Machine states */
    enum TxState 
    { 
	txIdle, 
	txDescRefr, 
	txDescRead, 
	txFifoBlock, 
	txFragRead,
	txDescWrite,
	txAdvance
    };

    /** Receive State Machine States */
    enum RxState 
    { 
	rxIdle, 
	rxDescRefr, 
	rxDescRead, 
	rxFifoBlock, 
	rxFragWrite,
	rxDescWrite, 
	rxAdvance 
    };

    enum DmaState
    {
	dmaIdle,
	dmaReading,
	dmaWriting,
	dmaReadWaiting,
	dmaWriteWaiting
    };

    /** EEPROM State Machine States */
    enum EEPROMState
    {
        eepromStart,
        eepromGetOpcode,
        eepromGetAddress,
        eepromRead
    };
    
  private:
    Addr addr;
    static const Addr size = sizeof(dp_regs);
    
  protected:
    typedef std::deque<PacketPtr> pktbuf_t;
    typedef pktbuf_t::iterator pktiter_t;

    /** device register file */
    dp_regs regs;   
    dp_rom rom;

    /** pci settings */
    bool ioEnable;
#if 0
    bool memEnable;
    bool bmEnable;
#endif

    /*** BASIC STRUCTURES FOR TX/RX ***/
    /* Data FIFOs */
    PacketFifo txFifo;
    PacketFifo rxFifo;

    /** various helper vars */
    PacketPtr txPacket;
    PacketPtr rxPacket;
    uint8_t *txPacketBufPtr;
    uint8_t *rxPacketBufPtr;
    uint32_t txXferLen;
    uint32_t rxXferLen;
    bool rxDmaFree;
    bool txDmaFree;

    /** DescCaches */
    ns_desc txDescCache;
    ns_desc rxDescCache;

    /* state machine cycle time */
    Tick clock;
    inline Tick cycles(int numCycles) const { return numCycles * clock; }

    /* tx State Machine */
    TxState txState;
    bool txEnable;

    /** Current Transmit Descriptor Done */
    bool CTDD;  
    /** halt the tx state machine after next packet */
    bool txHalt;
    /** ptr to the next byte in the current fragment */
    Addr txFragPtr; 
    /** count of bytes remaining in the current descriptor */
    uint32_t txDescCnt; 
    DmaState txDmaState;

    /** rx State Machine */
    RxState rxState;
    bool rxEnable;

    /** Current Receive Descriptor Done */
    bool CRDD; 
    /** num of bytes in the current packet being drained from rxDataFifo */
    uint32_t rxPktBytes; 
    /** halt the rx state machine after current packet */
    bool rxHalt;
    /** ptr to the next byte in current fragment */
    Addr rxFragPtr;
    /** count of bytes remaining in the current descriptor */
    uint32_t rxDescCnt;
    DmaState rxDmaState;

    bool extstsEnable;

    /** EEPROM State Machine */
    EEPROMState eepromState;
    bool eepromClk;
    uint8_t eepromBitsToRx;
    uint8_t eepromOpcode;
    uint8_t eepromAddress;
    uint16_t eepromData;

  protected:
    Tick dmaReadDelay;
    Tick dmaWriteDelay;

    Tick dmaReadFactor;
    Tick dmaWriteFactor;

    void *rxDmaData;
    Addr  rxDmaAddr;
    int   rxDmaLen;
    bool  doRxDmaRead();
    bool  doRxDmaWrite();
    void  rxDmaReadCopy();
    void  rxDmaWriteCopy();
    
    void *txDmaData;
    Addr  txDmaAddr;
    int   txDmaLen;
    bool  doTxDmaRead();
    bool  doTxDmaWrite();
    void  txDmaReadCopy();
    void  txDmaWriteCopy();

    void rxDmaReadDone();
    friend class EventWrapper<NSGigE, &NSGigE::rxDmaReadDone>;
    EventWrapper<NSGigE, &NSGigE::rxDmaReadDone> rxDmaReadEvent;

    void rxDmaWriteDone();
    friend class EventWrapper<NSGigE, &NSGigE::rxDmaWriteDone>;
    EventWrapper<NSGigE, &NSGigE::rxDmaWriteDone> rxDmaWriteEvent;

    void txDmaReadDone();
    friend class EventWrapper<NSGigE, &NSGigE::txDmaReadDone>;
    EventWrapper<NSGigE, &NSGigE::txDmaReadDone> txDmaReadEvent;

    void txDmaWriteDone();
    friend class EventWrapper<NSGigE, &NSGigE::txDmaWriteDone>;
    EventWrapper<NSGigE, &NSGigE::txDmaWriteDone> txDmaWriteEvent;

    bool dmaDescFree;
    bool dmaDataFree;


  protected:
    Tick txDelay;
    Tick rxDelay;

    void txReset();
    void rxReset();
    void regsReset(); 

    void rxKick();
    Tick rxKickTick;
    typedef EventWrapper<NSGigE, &NSGigE::rxKick> RxKickEvent;
    friend void RxKickEvent::process();
    RxKickEvent rxKickEvent;

    void txKick();
    Tick txKickTick;
    typedef EventWrapper<NSGigE, &NSGigE::txKick> TxKickEvent;
    friend void TxKickEvent::process();
    TxKickEvent txKickEvent;

    void eepromKick();

    /**
     * Retransmit event
     */
    void transmit();
    void txEventTransmit()
    {
	transmit();
	if (txState == txFifoBlock)
	    txKick();
    }
    typedef EventWrapper<NSGigE, &NSGigE::txEventTransmit> TxEvent;
    friend void TxEvent::process();
    TxEvent txEvent;

    void txDump() const;
    void rxDump() const;

    /**
     * receive address filter 
     */
    bool rxFilterEnable;
    bool rxFilter(const PacketPtr &packet);
    bool acceptBroadcast;
    bool acceptMulticast;
    bool acceptUnicast;
    bool acceptPerfect;
    bool acceptArp;
    bool multicastHashEnable;

    PhysicalMemory *physmem;

    /**
     * Interrupt management
     */
    void devIntrPost(uint32_t interrupts);
    void devIntrClear(uint32_t interrupts);
    void devIntrChangeMask();

    Tick intrDelay;
    Tick intrTick;
    bool cpuPendingIntr;
    void cpuIntrPost(Tick when);
    void cpuInterrupt();
    void cpuIntrClear();

    typedef EventWrapper<NSGigE, &NSGigE::cpuInterrupt> IntrEvent;
    friend void IntrEvent::process();
    IntrEvent *intrEvent;
    NSGigEInt *interface;

  public:
    struct Params : public PciDev::Params
    {
	PhysicalMemory *pmem;
	HierParams *hier;
	Bus *header_bus;
	Bus *payload_bus;
	Tick clock;
	Tick intr_delay;
	Tick tx_delay;
	Tick rx_delay;
	Tick pio_latency;
	bool dma_desc_free; 
	bool dma_data_free;
	Tick dma_read_delay;
	Tick dma_write_delay;
	Tick dma_read_factor;
	Tick dma_write_factor; 
	bool rx_filter;
	Net::EthAddr eaddr;
	uint32_t tx_fifo_size;
	uint32_t rx_fifo_size;
        uint32_t m5reg;
	bool dma_no_allocate;
    };

    NSGigE(Params *params);
    ~NSGigE();
    const Params *params() const { return (const Params *)_params; }

    virtual void writeConfig(int offset, int size, const uint8_t *data);
    virtual void readConfig(int offset, int size, uint8_t *data);

    virtual Fault read(MemReqPtr &req, uint8_t *data);
    virtual Fault write(MemReqPtr &req, const uint8_t *data);

    bool cpuIntrPending() const;
    void cpuIntrAck() { cpuIntrClear(); }

    bool recvPacket(PacketPtr packet);
    void transferDone();

    void setInterface(NSGigEInt *i) { assert(!interface); interface = i; }

    virtual void serialize(std::ostream &os);
    virtual void unserialize(Checkpoint *cp, const std::string &section);

  public:
    void regStats();

  private:
    Stats::Scalar<> txBytes;
    Stats::Scalar<> rxBytes;
    Stats::Scalar<> txPackets;
    Stats::Scalar<> rxPackets;
    Stats::Scalar<> txIpChecksums;
    Stats::Scalar<> rxIpChecksums;
    Stats::Scalar<> txTcpChecksums;
    Stats::Scalar<> rxTcpChecksums;
    Stats::Scalar<> txUdpChecksums;
    Stats::Scalar<> rxUdpChecksums;
    Stats::Scalar<> descDmaReads;
    Stats::Scalar<> descDmaWrites;
    Stats::Scalar<> descDmaRdBytes;
    Stats::Scalar<> descDmaWrBytes;
    Stats::Formula totBandwidth;
    Stats::Formula totPackets;
    Stats::Formula totBytes;
    Stats::Formula totPacketRate;
    Stats::Formula txBandwidth;
    Stats::Formula rxBandwidth;
    Stats::Formula txPacketRate;
    Stats::Formula rxPacketRate;
    Stats::Scalar<> postedSwi;
    Stats::Formula coalescedSwi;
    Stats::Scalar<> totalSwi;
    Stats::Scalar<> postedRxIdle;
    Stats::Formula coalescedRxIdle;
    Stats::Scalar<> totalRxIdle;
    Stats::Scalar<> postedRxOk;
    Stats::Formula coalescedRxOk;
    Stats::Scalar<> totalRxOk;
    Stats::Scalar<> postedRxDesc;
    Stats::Formula coalescedRxDesc;
    Stats::Scalar<> totalRxDesc;
    Stats::Scalar<> postedTxOk;
    Stats::Formula coalescedTxOk;
    Stats::Scalar<> totalTxOk;
    Stats::Scalar<> postedTxIdle;
    Stats::Formula coalescedTxIdle;
    Stats::Scalar<> totalTxIdle;
    Stats::Scalar<> postedTxDesc;
    Stats::Formula coalescedTxDesc;
    Stats::Scalar<> totalTxDesc;
    Stats::Scalar<> postedRxOrn;
    Stats::Formula coalescedRxOrn;
    Stats::Scalar<> totalRxOrn;
    Stats::Formula coalescedTotal;
    Stats::Scalar<> postedInterrupts;
    Stats::Scalar<> droppedPackets;

  public:
    Tick cacheAccess(MemReqPtr &req);
};

/*
 * Ethernet Interface for an Ethernet Device
 */
class NSGigEInt : public EtherInt
{
  private:
    NSGigE *dev;

  public:
    NSGigEInt(const std::string &name, NSGigE *d)
	: EtherInt(name), dev(d) { dev->setInterface(this); }

    virtual bool recvPacket(PacketPtr pkt) { return dev->recvPacket(pkt); }
    virtual void sendDone() { dev->transferDone(); }
};

#endif // __DEV_NS_GIGE_HH__
