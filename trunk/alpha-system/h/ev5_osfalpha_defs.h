/*
 * Copyright 1993 Hewlett-Packard Development Company, L.P. 
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef EV5_OSFALPHA_DEFS_INCLUDED
#define EV5_OSFALPHA_DEFS_INCLUDED 1

// from ev5_osfalpha_defs.mar from Lance's fetch directory
// lowercaseified and $ changed to _ and reformatting for gas...pb Nov/95

//
// PS Layout - PS
//	Loc	Size	name 	function
//	------	------	-----	-----------------------------------
//	<0:2>	3	IPL	Prio level
//	<3>	1	CM	Current Mode
//

#define	osfps_v_mode		3
#define	osfps_m_mode		(1<<osfps_v_mode)
#define	osfps_v_ipl		0
#define	osfps_m_ipl		(7<<osfps_v_ipl)

#define	osfipl_c_mchk		7
#define	osfipl_c_rt		6
#define	osfipl_c_clk		5
#define	osfipl_c_dev1		4
#define	osfipl_c_dev0		3
#define	osfipl_c_sw1		2
#define	osfipl_c_sw0		1
#define	osfipl_c_zero		0

#define	osfint_c_mchk		2
#define	osfint_c_clk		1
#define	osfint_c_dev		3
#define	osfint_c_ip		0		
#define	osfint_c_perf		4
#define	osfint_c_passrel	5		

//
// PTE layout - symbol prefix osfpte_
//
//	Loc	Size	name 	function
//	------	------	------	-----------------------------------
//	<63:32>	32	PFN	Page Frame Number
//	<31:16>	16	SOFT	Bits reserved for software use
//	<15:14>	2
//	<13>	1	UWE	User write enable
//	<12>	1	KWE	Kernel write enable
//	<11:10>	2
//	<9>	1	URE	User read enable
//	<8>	1	KRE	Kernel read enable
//	<7:6>	2	RES	Reserved SBZ 
//	<5>	1	HPF	Huge Page Flag
//	<4>	1	ASM	Wild card address space number match
//	<3>	1	FOE	Fault On execute
//	<2>	1	FOW	Fault On Write
//	<1>	1	FOR	Fault On Read
// 	<0>	1	V	valid bit
//

#define	osfpte_v_pfn	32
#define	osfpte_m_soft	(0xFFFF0000)
#define	osfpte_v_soft	16
#define	osfpte_m_uwe	(0x2000)
#define	osfpte_v_uwe	13
#define	osfpte_m_kwe	(0x1000)
#define	osfpte_v_kwe	12
#define	osfpte_m_ure	(0x0200)
#define	osfpte_v_ure	 9
#define	osfpte_m_kre	(0x0100)
#define	osfpte_v_kre	 8
#define	osfpte_m_hpf	(0x0020)
#define	osfpte_v_hpf	5
#define	osfpte_m_asm	(0x0010)
#define	osfpte_v_asm	4
#define	osfpte_m_foe	(0x0008)
#define	osfpte_v_foe	3
#define	osfpte_m_fow	(0x0004)
#define	osfpte_v_fow	2
#define	osfpte_m_for	(0x0002)
#define	osfpte_v_for	1
#define	osfpte_m_v	(0x0001)
#define	osfpte_v_v	0

#define	osfpte_m_ksegbits	(osfpte_m_kre | osfpte_m_kwe | osfpte_m_v | osfpte_m_asm)
#define	osfpte_m_prot	(osfpte_m_ure+osfpte_m_uwe | osfpte_m_kre | osfpte_m_kwe)

//
// VA layout - symbol prefix VA_
//
//	Loc	Size	name 	function
//	------	------	-------	-----------------------------------
//	<42:33>	10	SEG1	First seg table offset for mapping
//	<32:23>	10	SEG2	Second seg table offset for mapping
//	<22:13>	10	SEG3	Third seg table offset for mapping
//	<12:0>	13	OFFSET	Byte within page
//

#define	osfva_m_offset	(0x000000001FFF)
#define	osfva_v_offset	0
#define	osfva_m_seg3	(0x0000007FE000)
#define	osfva_v_seg3	13
#define	osfva_m_seg2	(0x0001FF800000)
#define	osfva_v_seg2	23	
#define	osfva_m_seg1	(0x7FE00000000)
#define	osfva_v_seg1	33	

#define	osfpcb_q_ksp	(0x0000)
#define	osfpcb_q_usp	(0x0008)
#define	osfpcb_q_Usp	(0x0008)
#define	osfpcb_q_mmptr	(0x0010)
#define	osfpcb_q_Mmptr	(0x0010)
#define	osfpcb_l_cc	(0x0018)
#define	osfpcb_l_asn	(0x001C)
#define	osfpcb_q_unique (0x0020)
#define	osfpcb_q_fen	(0x0028)
#define	osfpcb_v_pme	62

#define	osfsf_ps	(0x00)
#define	osfsf_pc	(0x08)
#define	osfsf_gp	(0x10)
#define	osfsf_a0	(0x18)
#define	osfsf_a1	(0x20)
#define	osfsf_a2	(0x28)
#define	osfsf_c_size	(0x30)

#endif
