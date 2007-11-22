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

#ifndef __DEV_SINICREG_HH__
#define __DEV_SINICREG_HH__

#define __SINIC_REG32(NAME, VAL) static const uint32_t NAME = (VAL)
#define __SINIC_REG64(NAME, VAL) static const uint64_t NAME = (VAL)

#define __SINIC_VAL32(NAME, OFFSET, WIDTH) \
	static const uint32_t NAME##_width = WIDTH; \
	static const uint32_t NAME##_offset = OFFSET; \
	static const uint32_t NAME##_mask = (1 << WIDTH) - 1; \
	static const uint32_t NAME = ((1 << WIDTH) - 1) << OFFSET; \
	static inline uint32_t get_##NAME(uint32_t reg) \
	{ return (reg & NAME) >> OFFSET; } \
	static inline uint32_t set_##NAME(uint32_t reg, uint32_t val) \
	{ return (reg & ~NAME) | ((val << OFFSET) & NAME); }

#define __SINIC_VAL64(NAME, OFFSET, WIDTH) \
	static const uint64_t NAME##_width = WIDTH; \
	static const uint64_t NAME##_offset = OFFSET; \
	static const uint64_t NAME##_mask = (ULL(1) << WIDTH) - 1; \
	static const uint64_t NAME = ((ULL(1) << WIDTH) - 1) << OFFSET;	\
	static inline uint64_t get_##NAME(uint64_t reg) \
	{ return (reg & NAME) >> OFFSET; } \
	static inline uint64_t set_##NAME(uint64_t reg, uint64_t val) \
	{ return (reg & ~NAME) | ((val << OFFSET) & NAME); }

namespace Sinic {
namespace Regs {

// Registers
__SINIC_REG32(Config,      0x00); // 32: configuration register 
__SINIC_REG32(RxMaxCopy,   0x04); // 32: max rx copy
__SINIC_REG32(TxMaxCopy,   0x08); // 32: max tx copy
__SINIC_REG32(RxThreshold, 0x0c); // 32: receive fifo threshold
__SINIC_REG32(TxThreshold, 0x10); // 32: transmit fifo threshold
__SINIC_REG32(IntrStatus,  0x14); // 32: interrupt status
__SINIC_REG32(IntrMask,    0x18); // 32: interrupt mask
__SINIC_REG32(RxData,      0x20); // 64: receive data
__SINIC_REG32(RxDone,      0x28); // 64: receive done
__SINIC_REG32(RxWait,      0x30); // 64: receive done (busy wait)
__SINIC_REG32(TxData,      0x38); // 64: transmit data
__SINIC_REG32(TxDone,      0x40); // 64: transmit done
__SINIC_REG32(TxWait,      0x48); // 64: transmit done (busy wait)
__SINIC_REG32(HwAddr,      0x50); // 64: mac address
__SINIC_REG32(Size,        0x58);

// Config register bits
__SINIC_VAL32(Config_Reset,  31, 1); // reset chip
__SINIC_VAL32(Config_Filter,  7, 1); // enable receive filter
__SINIC_VAL32(Config_Vlan,    6, 1); // enable vlan tagging
__SINIC_VAL32(Config_Virtual, 5, 1); // enable virtual addressing
__SINIC_VAL32(Config_Desc,    4, 1); // enable tx/rx descriptors
__SINIC_VAL32(Config_Poll,    3, 1); // enable polling
__SINIC_VAL32(Config_IntEn,   2, 1); // enable interrupts
__SINIC_VAL32(Config_TxEn,    1, 1); // enable transmit
__SINIC_VAL32(Config_RxEn,    0, 1); // enable receive

// Interrupt register bits
__SINIC_VAL32(Intr_TxFifo, 5, 1);  // Fifo oflow/uflow/threshold
__SINIC_VAL32(Intr_TxData, 4, 1);  // DMA Completed w/ interrupt
__SINIC_VAL32(Intr_TxDone, 3, 1);  // Packet transmitted
__SINIC_VAL32(Intr_RxFifo, 2, 1); // Fifo oflow/uflow/threshold
__SINIC_VAL32(Intr_RxData, 1, 1); // DMA Completed w/ interrupt
__SINIC_VAL32(Intr_RxDone, 0, 1); // Packet received
__SINIC_REG32(Intr_All,     0x3f);
__SINIC_REG32(Intr_NoDelay, 0x24);
__SINIC_REG32(Intr_Res,    ~0x3f);

// RX Data Description
__SINIC_VAL64(RxData_Len, 40, 20); // 0 - 1M
__SINIC_VAL64(RxData_Addr, 0, 40); // Address 1TB

// TX Data Description
__SINIC_VAL64(TxData_More,     63,  1);
__SINIC_VAL64(TxData_Checksum, 62,  1);
__SINIC_VAL64(TxData_Len,      40, 20); // 0 - 1M
__SINIC_VAL64(TxData_Addr,      0, 40); // Address 1TB

// RX Done/Busy Information
__SINIC_VAL64(RxDone_Complete,  63,  1);
__SINIC_VAL64(RxDone_IpPacket,  45,  1);
__SINIC_VAL64(RxDone_TcpPacket, 44,  1);
__SINIC_VAL64(RxDone_UdpPacket, 43,  1);
__SINIC_VAL64(RxDone_IpError,   42,  1);
__SINIC_VAL64(RxDone_TcpError,  41,  1);
__SINIC_VAL64(RxDone_UdpError,  40,  1);
__SINIC_VAL64(RxDone_More,      32,  1);
__SINIC_VAL64(RxDone_FifoLen,   20,  8); // up to 255 packets
__SINIC_VAL64(RxDone_CopyLen,    0, 20); // up to 256k

// TX Done/Busy Information
__SINIC_VAL64(TxDone_Complete, 63,  1);
__SINIC_VAL64(TxDone_FifoLen,  20,  8); // up to 255 packets
__SINIC_VAL64(TxDone_CopyLen,   0, 20); // up to 256k

inline int
regSize(int offset)
{
    static const char sizes[] = {
	4,
	4,
	4,
	4,
	4,
	4,
	4,
	0,
	8, 0,
	8, 0,
	8, 0,
	8, 0,
	8, 0,
	8, 0,
	8, 0
    };

    if (offset & 0x3)
	return 0;

    if (offset >= Size)
	return 0;

    return sizes[offset / 4];
}

inline const char *
regName(int offset)
{
    static const char *names[] = {
	"Config",
	"RxMaxCopy",
	"TxMaxCopy",
	"RxThreshold",
	"TxThreshold",
	"IntrStatus",
	"IntrMask",
	"invalid",
	"RxData", "invalid",
	"RxDone", "invalid",
	"RxWait", "invalid",
	"TxData", "invalid",
	"TxDone", "invalid",
	"TxWait", "invalid",
	"HwAddr", "invalid"
    };

    if (offset & 0x3)
	return "invalid";

    if (offset >= Size)
	return "invalid";

    return names[offset / 4];
}

/* namespace Regs */ }
/* namespace Sinic */ }

#endif // __DEV_SINICREG_HH__
