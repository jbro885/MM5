/*
 * Copyright (c) 2001, 2002, 2003, 2004, 2005
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
 * Disk Image Definitions
 */

#include <sys/types.h>
#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#include "base/callback.hh"
#include "base/misc.hh"
#include "base/trace.hh"
#include "dev/disk_image.hh"
#include "sim/builder.hh"
#include "sim/sim_exit.hh"
#include "targetarch/byte_swap.hh"

using namespace std;

////////////////////////////////////////////////////////////////////////
//
// Raw Disk image
//
RawDiskImage::RawDiskImage(const string &name, const string &filename,
			   bool rd_only)
    : DiskImage(name), disk_size(0)
{ open(filename, rd_only); }

RawDiskImage::~RawDiskImage()
{ close(); }

void
RawDiskImage::open(const string &filename, bool rd_only)
{
    if (!filename.empty()) {
	initialized = true;
	readonly = rd_only;
	file = filename;

	ios::openmode mode = ios::in | ios::binary;
	if (!readonly)
	    mode |= ios::out;
	stream.open(file.c_str(), mode);
	if (!stream.is_open())
	    panic("Error opening %s", filename);
    }
}

void
RawDiskImage::close()
{
    stream.close();
}

off_t
RawDiskImage::size() const
{
    if (disk_size == 0) {
	if (!stream.is_open())
	    panic("file not open!\n");
	stream.seekg(0, ios::end);
	disk_size = stream.tellg();
    }

    return disk_size / SectorSize;
}

off_t
RawDiskImage::read(uint8_t *data, off_t offset) const
{
    if (!initialized)
	panic("RawDiskImage not initialized");

    if (!stream.is_open())
	panic("file not open!\n");

    if (stream.seekg(offset * SectorSize, ios::beg) < 0)
	panic("Could not seek to location in file");

    streampos pos = stream.tellg();
    stream.read((char *)data, SectorSize);

    DPRINTF(DiskImageRead, "read: offset=%d\n", (uint64_t)offset);
    DDUMP(DiskImageRead, data, SectorSize);

    return stream.tellg() - pos;
}

off_t
RawDiskImage::write(const uint8_t *data, off_t offset)
{
    if (!initialized)
	panic("RawDiskImage not initialized");

    if (readonly)
	panic("Cannot write to a read only disk image");

    if (!stream.is_open())
	panic("file not open!\n");

    if (stream.seekp(offset * SectorSize, ios::beg) < 0)
	panic("Could not seek to location in file");

    DPRINTF(DiskImageWrite, "write: offset=%d\n", (uint64_t)offset);
    DDUMP(DiskImageWrite, data, SectorSize);

    streampos pos = stream.tellp();
    stream.write((const char *)data, SectorSize);
    return stream.tellp() - pos;
}

DEFINE_SIM_OBJECT_CLASS_NAME("DiskImage", DiskImage)

BEGIN_DECLARE_SIM_OBJECT_PARAMS(RawDiskImage)

    Param<string> image_file;
    Param<bool> read_only;

END_DECLARE_SIM_OBJECT_PARAMS(RawDiskImage)

BEGIN_INIT_SIM_OBJECT_PARAMS(RawDiskImage)

    INIT_PARAM(image_file, "disk image file"),
    INIT_PARAM_DFLT(read_only, "read only image", false)

END_INIT_SIM_OBJECT_PARAMS(RawDiskImage)


CREATE_SIM_OBJECT(RawDiskImage)
{
    return new RawDiskImage(getInstanceName(), image_file, read_only);
}

REGISTER_SIM_OBJECT("RawDiskImage", RawDiskImage)

////////////////////////////////////////////////////////////////////////
//
// Copy on Write Disk image
//
const int CowDiskImage::VersionMajor = 1;
const int CowDiskImage::VersionMinor = 0;

CowDiskImage::CowDiskImage(const string &name, DiskImage *kid, int hash_size)
    : DiskImage(name), child(kid), table(NULL)
{ init(hash_size); }

class CowDiskCallback : public Callback
{
  private:
    CowDiskImage *image;

  public:
    CowDiskCallback(CowDiskImage *i) : image(i) {}
    void process() { image->save(); delete this; }
};

CowDiskImage::CowDiskImage(const string &name, DiskImage *kid, int hash_size,
			   const string &file, bool read_only)
    : DiskImage(name), filename(file), child(kid), table(NULL)
{
    if (!open(filename)) {
	assert(!read_only && "why have a non-existent read only file?");
	init(hash_size);
    }

    if (!read_only)
	registerExitCallback(new CowDiskCallback(this));
}

CowDiskImage::~CowDiskImage()
{
    SectorTable::iterator i = table->begin();
    SectorTable::iterator end = table->end();

    while (i != end) {
	delete (*i).second;
	++i;
    }
}

void
SafeRead(ifstream &stream, void *data, int count)
{
    stream.read((char *)data, count);
    if (!stream.is_open())
	panic("file not open");

    if (stream.eof())
	panic("premature end-of-file");

    if (stream.bad() || stream.fail())
	panic("error reading cowdisk image");
}

template<class T>
void
SafeRead(ifstream &stream, T &data)
{ 
    SafeRead(stream, &data, sizeof(data)); 
}

template<class T>
void
SafeReadSwap(ifstream &stream, T &data)
{ 
    SafeRead(stream, &data, sizeof(data)); 
    data = letoh(data); //is this the proper byte order conversion?
}

bool
CowDiskImage::open(const string &file)
{
    ifstream stream(file.c_str());
    if (!stream.is_open())
	return false;

    if (stream.fail() || stream.bad())
	panic("Error opening %s", file);

    uint64_t magic;
    SafeRead(stream, magic);

    if (memcmp(&magic, "COWDISK!", sizeof(magic)) != 0)
	panic("Could not open %s: Invalid magic", file);

    uint32_t major, minor;
    SafeReadSwap(stream, major);
    SafeReadSwap(stream, minor);

    if (major != VersionMajor && minor != VersionMinor)
	panic("Could not open %s: invalid version %d.%d != %d.%d",
	      file, major, minor, VersionMajor, VersionMinor);

    uint64_t sector_count;
    SafeReadSwap(stream, sector_count);
    table = new SectorTable(sector_count);


    for (uint64_t i = 0; i < sector_count; i++) {
	uint64_t offset;
	SafeReadSwap(stream, offset);

	Sector *sector = new Sector;
	SafeRead(stream, sector, sizeof(Sector));

	assert(table->find(offset) == table->end());
	(*table)[offset] = sector;
    }

    stream.close();

    initialized = true;
    return true;
}

void
CowDiskImage::init(int hash_size)
{
    table = new SectorTable(hash_size);

    initialized = true;
}

void
SafeWrite(ofstream &stream, const void *data, int count)
{
    stream.write((const char *)data, count);
    if (!stream.is_open())
	panic("file not open");

    if (stream.eof())
	panic("premature end-of-file");

    if (stream.bad() || stream.fail())
	panic("error reading cowdisk image");
}

template<class T>
void
SafeWrite(ofstream &stream, const T &data)
{ 
    SafeWrite(stream, &data, sizeof(data)); 
}

template<class T>
void
SafeWriteSwap(ofstream &stream, const T &data)
{ 
    T swappeddata = letoh(data); //is this the proper byte order conversion?
    SafeWrite(stream, &swappeddata, sizeof(data)); 
}
void
CowDiskImage::save()
{
    save(filename);
}

void
CowDiskImage::save(const string &file)
{
    if (!initialized)
	panic("RawDiskImage not initialized");

    ofstream stream(file.c_str());
    if (!stream.is_open() || stream.fail() || stream.bad())
	panic("Error opening %s", file);

    uint64_t magic;
    memcpy(&magic, "COWDISK!", sizeof(magic));
    SafeWrite(stream, magic);

    SafeWriteSwap(stream, (uint32_t)VersionMajor);
    SafeWriteSwap(stream, (uint32_t)VersionMinor);
    SafeWriteSwap(stream, (uint64_t)table->size());

    uint64_t size = table->size();
    SectorTable::iterator iter = table->begin();
    SectorTable::iterator end = table->end();

    for (uint64_t i = 0; i < size; i++) {
	if (iter == end)
	    panic("Incorrect Table Size during save of COW disk image");

	SafeWriteSwap(stream, (uint64_t)(*iter).first);
	SafeWrite(stream, (*iter).second->data, sizeof(Sector));
	++iter;
    }

    stream.close();
}

void
CowDiskImage::writeback()
{
    SectorTable::iterator i = table->begin();
    SectorTable::iterator end = table->end();

    while (i != end) {
	child->write((*i).second->data, (*i).first);
	++i;
    }
}

off_t
CowDiskImage::size() const
{ return child->size(); }

off_t
CowDiskImage::read(uint8_t *data, off_t offset) const
{
    if (!initialized)
	panic("CowDiskImage not initialized");

    if (offset > size())
	panic("access out of bounds");

    SectorTable::const_iterator i = table->find(offset);
    if (i == table->end())
	return child->read(data, offset);
    else {
	memcpy(data, (*i).second->data, SectorSize);
	DPRINTF(DiskImageRead, "read: offset=%d\n", (uint64_t)offset);
	DDUMP(DiskImageRead, data, SectorSize);
	return SectorSize;
    }
}

off_t
CowDiskImage::write(const uint8_t *data, off_t offset)
{
    if (!initialized)
	panic("RawDiskImage not initialized");

    if (offset > size())
	panic("access out of bounds");

    SectorTable::iterator i = table->find(offset);
    if (i == table->end()) {
	Sector *sector = new Sector;
	memcpy(sector, data, SectorSize);
	table->insert(make_pair(offset, sector));
    } else {
	memcpy((*i).second->data, data, SectorSize);
    }

    DPRINTF(DiskImageWrite, "write: offset=%d\n", (uint64_t)offset);
    DDUMP(DiskImageWrite, data, SectorSize);

    return SectorSize;
}

void
CowDiskImage::serialize(ostream &os)
{
    string cowFilename = name() + ".cow";
    SERIALIZE_SCALAR(cowFilename);
    save(Checkpoint::dir() + "/" + cowFilename);
}

void
CowDiskImage::unserialize(Checkpoint *cp, const string &section)
{
    string cowFilename;
    UNSERIALIZE_SCALAR(cowFilename);
    cowFilename = cp->cptDir + "/" + cowFilename;
    open(cowFilename);
}

BEGIN_DECLARE_SIM_OBJECT_PARAMS(CowDiskImage)

    SimObjectParam<DiskImage *> child;
    Param<string> image_file;
    Param<int> table_size;
    Param<bool> read_only;

END_DECLARE_SIM_OBJECT_PARAMS(CowDiskImage)

BEGIN_INIT_SIM_OBJECT_PARAMS(CowDiskImage)

    INIT_PARAM(child, "child image"),
    INIT_PARAM_DFLT(image_file, "disk image file", ""),
    INIT_PARAM_DFLT(table_size, "initial table size", 65536),
    INIT_PARAM_DFLT(read_only, "don't write back to the copy-on-write file",
		    true)

END_INIT_SIM_OBJECT_PARAMS(CowDiskImage)


CREATE_SIM_OBJECT(CowDiskImage)
{
    if (((string)image_file).empty())
	return new CowDiskImage(getInstanceName(), child, table_size);
    else
	return new CowDiskImage(getInstanceName(), child, table_size,
				image_file, read_only);
}

REGISTER_SIM_OBJECT("CowDiskImage", CowDiskImage)
