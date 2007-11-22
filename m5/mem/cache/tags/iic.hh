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
 * Declaration of the Indirect Index Cache (IIC) tags store.
 */

#ifndef __IIC_HH__
#define __IIC_HH__

#include <list>
#include <vector>

#include "mem/cache/cache_blk.hh"
#include "mem/cache/tags/repl/repl.hh"
#include "mem/mem_req.hh"
#include "base/statistics.hh"
#include "mem/cache/tags/base_tags.hh"

class BaseCache; // Forward declaration

/**
 * IIC cache blk.
 */
class IICTag : public CacheBlk
{
  public:
    /**
     * Copy the contents of the given IICTag into this one.
     * @param rhs The tag to copy.
     * @return const reference to this tag.
     */
    const IICTag& operator=(const IICTag& rhs)
    {
	CacheBlk::operator=(rhs);
	chain_ptr = rhs.chain_ptr;
	re = rhs.re;
	set = rhs.set;
	trivialData = rhs.trivialData;
	numData = rhs.numData;
	data_ptr.clear();
	for (int i = 0; i < rhs.numData; ++i) {
	    data_ptr.push_back(rhs.data_ptr[i]);
	}
	return *this;
    }
    
    /** Hash chain pointer into secondary store. */
    unsigned long chain_ptr;
    /** Data array pointers for each subblock. */
    std::vector<unsigned long> data_ptr;
    /** Replacement Entry pointer. */
    void *re;
    /**
     * An array to store small compressed data. Conceputally the same size
     * as the unsused data array pointers.
     */
    uint8_t *trivialData;
    /**
     * The number of allocated subblocks.
     */
    int numData;
};

/**
 * A hash set for the IIC primary lookup table.
 */
class IICSet{
public:
    /** The associativity of the primary table. */
    int assoc;
 
    /** The number of hash chains followed when finding the last block. */
    int depth;
    /** The current number of blocks on the chain. */
    int size;

    /** Tag pointer into the secondary tag storage. */
    unsigned long chain_ptr;

    /** The LRU list of the primary table. MRU is at 0 index. */
    IICTag ** tags;

    /**
     * Find the addr in this set, return the chain pointer to the secondary if
     * it isn't found.
     * @param asid The address space ID.
     * @param tag The address to find.
     * @param chain_ptr The chain pointer to start the search of the secondary
     * @return Pointer to the tag, NULL if not found.
     */
    IICTag* findTag(int asid, Addr tag, unsigned long &chain_ptr)
    {
	depth = 1;
	for (int i = 0; i < assoc; ++i) {
	    if (tags[i]->tag == tag && tags[i]->isValid()) {
		return tags[i];
	    }
	}
	chain_ptr = this->chain_ptr;
	return 0;
    }

    /**
     * Find an usused tag in this set.
     * @return Pointer to the unused tag, NULL if none are free.
     */
    IICTag* findFree()
    {
	for (int i = 0; i < assoc; ++i) {
	    if (!tags[i]->isValid()) {
		return tags[i];
	    }
	}
	return 0;
    }

    /**
     * Move a tag to the head of the LRU list
     * @param tag The tag to move.
     */
    void moveToHead(IICTag *tag);

    /**
     * Move a tag to the tail (LRU) of the LRU list
     * @param tag The tag to move.
     */
    void moveToTail(IICTag *tag);
};

/**
 * The IIC tag store. This is a hardware-realizable, fully-associative tag
 * store that uses software replacement, e.g. Gen.
 */
class IIC : public BaseTags
{
  public:
    /** Typedef of the block type used in this class. */
    typedef IICTag BlkType;
    /** Typedef for list of pointers to the local block type. */
    typedef std::list<IICTag*> BlkList;
  protected:
    /** The number of set in the primary table. */
    const int hashSets;
    /** The block size in bytes. */
    const int blkSize;
    /** The associativity of the primary table. */
    const int assoc;
    /** The base hit latency. */
    const int hitLatency;
    /** The subblock size, used for compression. */
    const int subSize;

    /** The number of subblocks */
    const int numSub;
    /** The number of bytes used by data pointers */
    const int trivialSize;

    /** The amount to shift address to get the tag. */
    const int tagShift;
    /** The mask to get block offset bits. */
    const unsigned blkMask;

    /** The amount to shift to get the subblock number. */
    const int subShift;
    /** The mask to get the correct subblock number. */
    const unsigned subMask; 

    /** The latency of a hash lookup. */
    const int hashDelay;
    /** The number of data blocks. */
    const int numBlocks;
    /** The total number of tags in primary and secondary. */
    const int numTags;
    /** The number of tags in the secondary tag store. */
    const int numSecondary;
    
    /** The Null tag pointer. */
    const int tagNull;
    /** The last tag in the primary table. */
    const int primaryBound;

    /** All of the tags */
    IICTag *tagStore;
    /**
     * Pointer to the head of the secondary freelist (maintained with chain
     * pointers.
     */
    unsigned long freelist;
    /**
     * The data block freelist.
     */
    std::list<unsigned long> blkFreelist;

    /** The primary table. */
    IICSet *sets;

    /** The replacement policy. */
    Repl *repl;    
    
    /** An array of data reference counters. */
    int *dataReferenceCount;

    /** The data blocks. */
    uint8_t *dataStore;
    
    /** Storage for the fast access data of each cache block. */
    uint8_t **dataBlks;
    
    /**
     * Count of the current number of free secondary tags.
     * Used for debugging.
     */
    int freeSecond;

    // IIC Statistics
    /**
     * @addtogroup IICStatistics IIC Statistics
     * @{
     */

    /** Hash hit depth of cache hits. */
    Stats::Distribution<> hitHashDepth;
    /** Hash depth for cache misses. */
    Stats::Distribution<> missHashDepth;
    /** Count of accesses to each hash set. */
    Stats::Distribution<> setAccess;

    /** The total hash depth for every miss. */
    Stats::Scalar<> missDepthTotal;
    /** The total hash depth for all hits. */
    Stats::Scalar<> hitDepthTotal;
    /** The number of hash misses. */
    Stats::Scalar<> hashMiss;
    /** The number of hash hits. */
    Stats::Scalar<> hashHit;
    /** @} */

  public:
    /**
     * Collection of parameters for the IIC.
     */
    class Params {
      public:
	/** The size in bytes of the cache. */
	int size;
	/** The number of sets in the primary table. */
	int numSets;
	/** The block size in bytes. */
	int blkSize;
	/** The associativity of the primary table. */
	int assoc;
	/** The number of cycles for each hash lookup. */
	int hashDelay;
	/** The number of cycles to read the data. */
	int hitLatency;
	/** The replacement policy. */
	Repl *rp;
	/** The subblock size in bytes. */
	int subblockSize;
    };
    
    /**
     * Construct and initialize this tag store.
     * @param params The IIC parameters.
     * @todo
     * Should make a way to have less tags in the primary than blks in the
     * cache. Also should be able to specify number of secondary blks.
     */
    IIC(Params &params);

    /**
     * Destructor.
     */
    virtual ~IIC();

    /**
     * Register the statistics.
     * @param name The name to prepend to the statistic descriptions.
     */
    void regStats(const std::string &name);

    /**
     * Regenerate the block address from the tag.
     * @param tag The tag of the block.
     * @param set Not needed for the iic.
     * @return The block address.
     */
    Addr regenerateBlkAddr(Addr tag, int set) {
	return (((Addr)tag << tagShift));
    }

    /**
     * Return the block size.
     * @return The block size.
     */
    int getBlockSize()
    {
	return blkSize;
    }

    /**
     * Return the subblock size.
     * @return The subblock size.
     */
    int getSubBlockSize()
    {
	return subSize;
    }

    /**
     * Return the hit latency.
     * @return the hit latency.
     */
    int getHitLatency() const
    {
	return hitLatency;
    }

    /**
     * Generate the tag from the address.
     * @param addr The address to a get a tag for.
     * @param blk Ignored here.
     * @return the tag.
     */
    Addr extractTag(Addr addr, IICTag *blk) const
    {
	return (addr >> tagShift);
    }

     /**
     * Generate the tag from the address.
     * @param addr The address to a get a tag for.
     * @return the tag.
     */
    Addr extractTag(Addr addr) const
    {
	return (addr >> tagShift);
    }

   /**
     * Return the set, always 0 for IIC.
     * @return 0.
     */
    int extractSet(Addr addr) const
    {
	return 0;
    }

    /**
     * Get the block offset of an address.
     * @param addr The address to get the offset of.
     * @return the block offset of the address.
     */
    int extractBlkOffset(Addr addr) const
    {
	return (addr & blkMask);
    }

    /**
     * Align an address to the block size.
     * @param addr the address to align.
     * @return The block address.
     */
    Addr blkAlign(Addr addr) const
    {
	return (addr & ~(Addr)blkMask);
    }

    /**
     * Check for the address in the tagstore.
     * @param asid The address space ID.
     * @param addr The address to find.
     * @return true if it is found.
     */
    bool probe(int asid, Addr addr) const;

    /**
     * Swap the position of two tags.
     * @param index1 The first tag location.
     * @param index2 The second tag location.
     */
    void tagSwap(unsigned long index1, unsigned long index2);

    /**
     * Clear the reference bit of the tag and return its old value.
     * @param index The pointer of the tag to manipulate.
     * @return The previous state of the reference bit.
     */
    bool clearRef(unsigned long index)
    {
	bool tmp = tagStore[index].isReferenced();
	tagStore[index].status &= ~BlkReferenced;
	return tmp;
    }

    /**
     * Decompress a block if it is compressed.
     * @param index The tag store index for the block to uncompress.
     */
    void decompressBlock(unsigned long index);
    
    /**
     * Try and compress a block if it is not already compressed.
     * @param index The tag store index for the block to compress.
     */
    void compressBlock(unsigned long index);

    /**
     * Invalidate the block containing the address.
     * @param asid The address space ID.
     * @param addr The address to invalidate.
     */
    void invalidateBlk(int asid, Addr addr);

    /**
     * Find the block and update the replacement data. This call also returns
     * the access latency as a side effect.
     * @param addr The address to find.
     * @param asid The address space ID.
     * @param lat The access latency.
     * @return A pointer to the block found, if any.
     */
    IICTag* findBlock(Addr addr, int asid, int &lat);

    /**
     * Find the block and update the replacement data. This call also returns
     * the access latency as a side effect.
     * @param req The req whose block to find
     * @param lat The access latency.
     * @return A pointer to the block found, if any.
     */
    IICTag* findBlock(MemReqPtr &req, int &lat);

    /**
     * Find the block, do not update the replacement data.
     * @param addr The address to find.
     * @param asid The address space ID.
     * @return A pointer to the block found, if any.
     */
    IICTag* findBlock(Addr addr, int asid) const;

    /**
     * Find a replacement block for the address provided.
     * @param req The request to a find a replacement candidate for.
     * @param writebacks List for any writebacks to be performed.
     * @param compress_blocks List of blocks to compress, for adaptive comp.
     * @return The block to place the replacement in.
     */
    IICTag* findReplacement(MemReqPtr &req, MemReqList &writebacks,
			    BlkList &compress_blocks);

    /**
     * Read the data from the internal storage of the given cache block.
     * @param blk The block to read the data from.
     * @param data The buffer to read the data into.
     * @return The cache block's data.
     */
    void readData(IICTag *blk, uint8_t *data);

    /**
     * Write the data into the internal storage of the given cache block.
     * @param blk The block to write to.
     * @param data The data to write.
     * @param size The number of bytes to write.
     * @param writebacks A list for any writebacks to be performed. May be
     * needed when writing to a compressed block.
     */
    void writeData(IICTag *blk, uint8_t *data, int size,
		   MemReqList & writebacks);
        
    /**
     * Perform a block aligned copy from the source address to the destination.
     * @param source The block-aligned source address.
     * @param dest The block-aligned destination address.
     * @param asid The address space DI.
     * @param writebacks List for any generated writeback requests.
     */
    void doCopy(Addr source, Addr dest, int asid, MemReqList &writebacks);

    /**
     * If a block is currently marked copy on write, copy it before writing.
     * @param req The write request.
     * @param writebacks List for any generated writeback requests.
     */
    void fixCopy(MemReqPtr &req, MemReqList &writebacks);

    /**
     * Called at end of simulation to complete average block reference stats.
     */
    virtual void cleanupRefs();
private:
    /**
     * Return the hash of the address.
     * @param addr The address to hash.
     * @return the hash of the address.
     */
    unsigned hash(Addr addr) const;

    /**
     * Search for a block in the secondary tag store. Returns the number of
     * hash lookups as a side effect.
     * @param asid The address space ID.
     * @param tag The tag to match.
     * @param chain_ptr The first entry to search.
     * @param depth The number of hash lookups made while searching.
     * @return A pointer to the block if found.
     */
    IICTag *secondaryChain(int asid, Addr tag, unsigned long chain_ptr,
			    int *depth) const;

    /**
     * Free the resources associated with the next replacement block.
     * @param writebacks A list of any writebacks to perform.
     */
    void freeReplacementBlock(MemReqList & writebacks);

    /**
     * Return the pointer to a free data block.
     * @param writebacks A list of any writebacks to perform.
     * @return A pointer to a free data block.
     */
    unsigned long getFreeDataBlock(MemReqList & writebacks);

    /**
     * Get a free tag in the given hash set.
     * @param set The hash set to search.
     * @param writebacks A list of any writebacks to perform.
     * @return a pointer to a free tag.
     */
    IICTag* getFreeTag(int set, MemReqList & writebacks);

    /**
     * Free the resources associated with the given tag.
     * @param tag_ptr The tag to free.
     */
    void freeTag(IICTag *tag_ptr);

    /**
     * Mark the given data block as being available.
     * @param data_ptr The data block to free.
     */
    void freeDataBlock(unsigned long data_ptr);
};
#endif // __IIC_HH__

