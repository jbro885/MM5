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

#include <iostream>
#include "cpu/static_inst.hh"
#include "sim/root.hh"

template <class ISA>
StaticInstPtr<ISA> StaticInst<ISA>::nullStaticInstPtr;

template <class ISA>
typename StaticInst<ISA>::DecodeCache StaticInst<ISA>::decodeCache;

// Define the decode cache hash map.
template StaticInst<AlphaISA>::DecodeCache
StaticInst<AlphaISA>::decodeCache;

template <class ISA>
void
StaticInst<ISA>::dumpDecodeCacheStats()
{
    using namespace std;

    cerr << "Decode hash table stats @ " << curTick << ":" << endl;
    cerr << "\tnum entries = " << decodeCache.size() << endl;
    cerr << "\tnum buckets = " << decodeCache.bucket_count() << endl;
    vector<int> hist(100, 0);
    int max_hist = 0;
    for (int i = 0; i < decodeCache.bucket_count(); ++i) {
	int count = decodeCache.elems_in_bucket(i);
	if (count > max_hist)
	    max_hist = count;
	hist[count]++;
    }
    for (int i = 0; i <= max_hist; ++i) {
	cerr << "\tbuckets of size " << i << " = " << hist[i] << endl;
    }
}


template StaticInstPtr<AlphaISA>
StaticInst<AlphaISA>::nullStaticInstPtr;

template <class ISA>
bool
StaticInst<ISA>::hasBranchTarget(Addr pc, ExecContext *xc, Addr &tgt) const
{
    if (isDirectCtrl()) {
	tgt = branchTarget(pc);
	return true;
    }

    if (isIndirectCtrl()) {
	tgt = branchTarget(xc);
	return true;
    }

    return false;
}


// force instantiation of template function(s) above
template class StaticInst<AlphaISA>;
