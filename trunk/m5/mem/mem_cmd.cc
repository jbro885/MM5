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

/**
 * @file
 * Definitions of memory commands.
 */
#include <iostream>

#include "mem/mem_cmd.hh"

char*
MemCmd::strings[NUM_MEM_CMDS] = {
    "badMemCmd", "read", "write", "swpf", "hwpf", "writeback",
    "invalidate", "readEx", "writeInv", "upgrade", "copy", "squash",
    "directory writeback", "directory redirected read", "directory owner transfer",
    "directory owner writeback", "directory sharer writeback", "new owner multicast"
};

int
MemCmd::behaviors[NUM_MEM_CMDS] = {
    0, rd, wr, rd|pf, rd|pf, wr|nr, in|nr, rd|in, wr|in, in, nr, nr|hw,
    rd|nr|directory, rd|directory, rd|directory, rd|directory, rd|directory, rd|directory
};


const char *MemCmd::memAccessDesc[NUM_MEM_ACCESS_RESULTS] = {
    "Hit",
    "Miss",
    "TLB miss",
    "Bad address",
    "MSHRs full",
    "MSHR targets full",
    "Miss pending",
    "Not issued",
    "Not predicted",
    "Canceled",
    "Dropped",
    "Filtered",
    "No result",
    "Success",
    "Blocked hard prefetch",
    "Blocked prefetch",
    "Blocked all"
};


std::ostream &
operator<<(std::ostream &out, MemCmd cmd)
{
    out << cmd.toString() << " [" << cmd.toIndex() << "]";

    return out;
}
