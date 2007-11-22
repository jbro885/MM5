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

#ifndef __DEV_SINIC_HH__
#define __DEV_SINIC_HH__

#include "base/inet.hh"
#include "base/statistics.hh"
#include "dev/etherint.hh"
#include "dev/etherpkt.hh"
#include "dev/io_device.hh"
#include "dev/pcidev.hh"
#include "dev/pktfifo.hh"
#include "dev/sinicreg.hh"
#include "mem/bus/bus.hh"
#include "sim/eventq.hh"

namespace Sinic {

class Interface;
class Base : public PciDev
{
  protected:
    bool rxEnable;
    bool txEnable;
    Tick cycleTime;
    inline Tick cycles(int numCycles) const { return numCycles * cycleTime; }

  protected:
    Tick intrDelay;
    Tick intrTick;
    bool cpuIntrEnable;
    bool cpuPendingIntr;
    void cpuIntrPost(Tick when);
    void cpuInterrupt();
    void cpuIntrClear();

    typedef EventWrapper<Base, &Base::cpuInterrupt> IntrEvent;
    friend void IntrEvent::process();
    IntrEvent *intrEvent;
    Interface *interface;

    bool cpuIntrPending() const;
    void cpuIntrAck() { cpuIntrClear(); }

/**
 * Serialization stuff
 */
  public:
    virtual void serialize(std::ostream &os);
    virtual void unserialize(Checkpoint *cp, const std::string &section);

/**
 * Construction/Destruction/Parameters
 */
  public:
    struct Params : public PciDev::Params
    {
	Tick cycle_time;
	Tick intr_delay;
    };

    Base(Params *p);
};

class Device : public Base
{
  protected:
    Platform *plat;
    PhysicalMemory *physmem;

  protected:
    /** Receive State Machine States */
    enum RxState { 
	rxIdle, 
	rxFifoBlock,
	rxBeginCopy,
	rxCopy,
	rxCopyDone
    };

    /** Transmit State Machine states */
    enum TxState { 
	txIdle, 
	txFifoBlock, 
	txBeginCopy,
	txCopy,
	txCopyDone
    };

    /** device register file */
    struct {
	uint32_t Config;
	uint32_t RxMaxCopy;
	uint32_t TxMaxCopy;
	uint32_t RxThreshold;
	uint32_t TxThreshold;
	uint32_t IntrStatus;
	uint32_t IntrMask;
	uint64_t RxData;
	uint64_t RxDone;
	uint64_t TxData;
	uint64_t TxDone;
    } regs;

  private:
    Addr addr;
    static const Addr size = Regs::Size;
    
  protected:
    RxState rxState;
    PacketFifo rxFifo;
    PacketPtr rxPacket;
    uint8_t *rxPacketBufPtr;
    int rxPktBytes;
    uint64_t rxDoneData;
    Addr rxDmaAddr;
    uint8_t *rxDmaData;
    int rxDmaLen;

    TxState txState;
    PacketFifo txFifo;
    PacketPtr txPacket;
    uint8_t *txPacketBufPtr;
    int txPktBytes;
    Addr txDmaAddr;
    uint8_t *txDmaData;
    int txDmaLen;

  protected:
    void reset(); 

    void rxKick();
    Tick rxKickTick;
    typedef EventWrapper<Device, &Device::rxKick> RxKickEvent;
    friend void RxKickEvent::process();

    void txKick();
    Tick txKickTick;
    typedef EventWrapper<Device, &Device::txKick> TxKickEvent;
    friend void TxKickEvent::process();

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
    typedef EventWrapper<Device, &Device::txEventTransmit> TxEvent;
    friend void TxEvent::process();
    TxEvent txEvent;

    void txDump() const;
    void rxDump() const;

    /**
     * receive address filter 
     */
    bool rxFilter(const PacketPtr &packet);

/**
 * device configuration
 */
    void changeConfig(uint32_t newconfig);

/**
 * device ethernet interface
 */
  public:
    bool recvPacket(PacketPtr packet);
    void transferDone();
    void setInterface(Interface *i) { assert(!interface); interface = i; }

/**
 * DMA parameters
 */
  protected:
    void rxDmaCopy();
    void rxDmaDone();
    friend class EventWrapper<Device, &Device::rxDmaDone>;
    EventWrapper<Device, &Device::rxDmaDone> rxDmaEvent;

    void txDmaCopy();
    void txDmaDone();
    friend class EventWrapper<Device, &Device::txDmaDone>;
    EventWrapper<Device, &Device::rxDmaDone> txDmaEvent;

    Tick dmaReadDelay;
    Tick dmaReadFactor;
    Tick dmaWriteDelay;
    Tick dmaWriteFactor;

/**
 * PIO parameters
 */
  protected:
    MemReqPtr rxPioRequest;
    MemReqPtr txPioRequest;

/**
 * Interrupt management
 */
  protected:
    void devIntrPost(uint32_t interrupts);
    void devIntrClear(uint32_t interrupts = Regs::Intr_All);
    void devIntrChangeMask(uint32_t newmask);

/**
 * PCI Configuration interface
 */
  public:
    virtual void writeConfig(int offset, int size, const uint8_t *data);

/**
 * Memory Interface
 */
  public:
    virtual Fault read(MemReqPtr &req, uint8_t *data);
    virtual Fault write(MemReqPtr &req, const uint8_t *data);
    Tick cacheAccess(MemReqPtr &req);

/**
 * Statistics
 */
  private:
    Stats::Scalar<> rxBytes;
    Stats::Formula  rxBandwidth;
    Stats::Scalar<> rxPackets;
    Stats::Formula  rxPacketRate;
    Stats::Scalar<> rxIpPackets;
    Stats::Scalar<> rxTcpPackets;
    Stats::Scalar<> rxUdpPackets;
    Stats::Scalar<> rxIpChecksums;
    Stats::Scalar<> rxTcpChecksums;
    Stats::Scalar<> rxUdpChecksums;

    Stats::Scalar<> txBytes;
    Stats::Formula  txBandwidth;
    Stats::Formula totBandwidth;
    Stats::Formula totPackets;
    Stats::Formula totBytes;
    Stats::Formula totPacketRate;
    Stats::Scalar<> txPackets;
    Stats::Formula  txPacketRate;
    Stats::Scalar<> txIpPackets;
    Stats::Scalar<> txTcpPackets;
    Stats::Scalar<> txUdpPackets;
    Stats::Scalar<> txIpChecksums;
    Stats::Scalar<> txTcpChecksums;
    Stats::Scalar<> txUdpChecksums;

  public:
    virtual void regStats();

/**
 * Serialization stuff
 */
  public:
    virtual void serialize(std::ostream &os);
    virtual void unserialize(Checkpoint *cp, const std::string &section);

/**
 * Construction/Destruction/Parameters
 */
  public:
    struct Params : public Base::Params
    {
	IntrControl *i;
	PhysicalMemory *pmem;
	Tick tx_delay;
	Tick rx_delay;
	HierParams *hier;
	Bus *io_bus;
	Bus *payload_bus;
	Tick pio_latency;
	PhysicalMemory *physmem;
	IntrControl *intctrl;
	bool rx_filter;
	Net::EthAddr eaddr;
	uint32_t rx_max_copy;
	uint32_t tx_max_copy;
	uint32_t rx_fifo_size;
	uint32_t tx_fifo_size;
	uint32_t rx_fifo_threshold;
	uint32_t tx_fifo_threshold;
	Tick dma_read_delay;
	Tick dma_read_factor;
	Tick dma_write_delay;
	Tick dma_write_factor;
	bool dma_no_allocate;
    };

  protected:
    const Params *params() const { return (const Params *)_params; }

  public:
    Device(Params *params);
    ~Device();
};

/*
 * Ethernet Interface for an Ethernet Device
 */
class Interface : public EtherInt
{
  private:
    Device *dev;

  public:
    Interface(const std::string &name, Device *d)
	: EtherInt(name), dev(d) { dev->setInterface(this); }

    virtual bool recvPacket(PacketPtr pkt) { return dev->recvPacket(pkt); }
    virtual void sendDone() { dev->transferDone(); }
};

/* namespace Sinic */ }

#endif // __DEV_SINIC_HH__
