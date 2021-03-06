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

#include <unistd.h>

#include <string>
#include <iostream>

#include "sim/syscall_emul.hh"
#include "base/trace.hh"
#include "cpu/exec_context.hh"
#include "cpu/base.hh"
#include "sim/process.hh"

#include "sim/sim_events.hh"

using namespace std;

void
SyscallDesc::doSyscall(int callnum, Process *process, ExecContext *xc)
{
    DPRINTFR(SyscallVerbose, "%s: syscall %s called at %d\n",
	     xc->cpu->name(), name, curTick);

    SyscallReturn retval = (*funcPtr)(this, callnum, process, xc);

    DPRINTFR(SyscallVerbose, "%s: syscall %s returns %d\n",
	     xc->cpu->name(), name, retval.value());

    if (!(flags & SyscallDesc::SuppressReturnValue))
	xc->setSyscallReturn(retval);
}


SyscallReturn
unimplementedFunc(SyscallDesc *desc, int callnum, Process *process,
		  ExecContext *xc)
{
    cerr << "Error: syscall " << desc->name
	 << " (#" << callnum << ") unimplemented.";
    cerr << "  Args: " << xc->getSyscallArg(0) << ", " << xc->getSyscallArg(1)
	 << ", ..." << endl;

    abort();
}


SyscallReturn
ignoreFunc(SyscallDesc *desc, int callnum, Process *process,
	   ExecContext *xc)
{
    DCOUT(SyscallWarnings) << "Warning: ignoring syscall " << desc->name
			   << "(" << xc->getSyscallArg(0)
			   << ", " << xc->getSyscallArg(1)
			   << ", ...)" << endl;

    return 0;
}


SyscallReturn
exitFunc(SyscallDesc *desc, int callnum, Process *process,
	 ExecContext *xc)
{

//    DPRINTFR(SyscallVerbose, "%s: Creating sim exit event\n", xc->cpu->name());
//    new SimExitEvent("syscall caused exit", xc->getSyscallArg(0) & 0xff);

	DPRINTFR(SyscallVerbose, "%s: processing exit syscall\n", xc->cpu->name());
	xc->cpu->registerProcessHalt();

    return 1;
}


SyscallReturn
getpagesizeFunc(SyscallDesc *desc, int num, Process *p, ExecContext *xc)
{
    return VMPageSize;
}


SyscallReturn
obreakFunc(SyscallDesc *desc, int num, Process *p, ExecContext *xc)
{
	// change brk addr to first arg
	Addr new_brk = xc->getSyscallArg(0);
	Addr old_brk = p->brk_point;

	if (new_brk != 0)
	{

		if(new_brk < old_brk){
			warn("Reducing stack size!\n");
			p->clearMemory(new_brk, old_brk);
		}

		p->brk_point = xc->getSyscallArg(0);
	}
	DPRINTF(SyscallVerbose, "Break Point changed to: %#X, was %#X, new_brk=%#X\n", p->brk_point, old_brk, new_brk);
	return p->brk_point;
}


SyscallReturn
closeFunc(SyscallDesc *desc, int num, Process *p, ExecContext *xc)
{
  int tgt_fd = xc->getSyscallArg(0);
  int fd = p->sim_fd(tgt_fd);
  bool closeFile = p->close_fd(tgt_fd);

  int retval = 0;
  if(closeFile){
    retval = close(fd);
  }
  return retval;
}


SyscallReturn
readFunc(SyscallDesc *desc, int num, Process *p, ExecContext *xc)
{
    int sim_fd = xc->getSyscallArg(0);
    Addr bufPtr = xc->getSyscallArg(1);
    int nbytes = xc->getSyscallArg(2);
    BufferArg bufArg(bufPtr, nbytes);

    int fd = p->sim_fd(sim_fd);

    int bytes_read = read(fd, bufArg.bufferPtr(), nbytes);

    DPRINTF(SyscallVerbose, "Read %d bytes of requested %d bytes from sim fd %d (host fd %d)\n",
    		nbytes,
    		bytes_read,
    		sim_fd,
    		fd);

    if (bytes_read != -1 ) bufArg.copyOut(xc->mem);

    return bytes_read;
}

SyscallReturn
writeFunc(SyscallDesc *desc, int num, Process *p, ExecContext *xc)
{
    int fd = p->sim_fd(xc->getSyscallArg(0));
    int nbytes = xc->getSyscallArg(2);
    BufferArg bufArg(xc->getSyscallArg(1), nbytes);

    bufArg.copyIn(xc->mem);

    int bytes_written = write(fd, bufArg.bufferPtr(), nbytes);

    fsync(fd);

    return bytes_written;
}


SyscallReturn
lseekFunc(SyscallDesc *desc, int num, Process *p, ExecContext *xc)
{
    int fd = p->sim_fd(xc->getSyscallArg(0));
    uint64_t offs = xc->getSyscallArg(1);
    int whence = xc->getSyscallArg(2);

    off_t result = lseek(fd, offs, whence);

    return (result == (off_t)-1) ? -errno : result;
}


SyscallReturn
munmapFunc(SyscallDesc *desc, int num, Process *p, ExecContext *xc)
{
    // given that we don't really implement mmap, munmap is really easy
    return 0;
}


const char *hostname = "m5.eecs.umich.edu";

SyscallReturn
gethostnameFunc(SyscallDesc *desc, int num, Process *p, ExecContext *xc)
{
    int name_len = xc->getSyscallArg(1);
    BufferArg name(xc->getSyscallArg(0), name_len);

    strncpy((char *)name.bufferPtr(), hostname, name_len);

    name.copyOut(xc->mem);

    return 0;
}

SyscallReturn
unlinkFunc(SyscallDesc *desc, int num, Process *p, ExecContext *xc)
{
    string path;

    if (xc->mem->readString(path, xc->getSyscallArg(0)) != No_Fault)
	return (TheISA::IntReg)-EFAULT;

    int result = unlink(path.c_str());
    return (result == -1) ? -errno : result;
}

SyscallReturn
renameFunc(SyscallDesc *desc, int num, Process *p, ExecContext *xc)
{
    string old_name;

    if (xc->mem->readString(old_name, xc->getSyscallArg(0)) != No_Fault)
	return -EFAULT;

    string new_name;

    if (xc->mem->readString(new_name, xc->getSyscallArg(1)) != No_Fault)
	return -EFAULT;

    int64_t result = rename(old_name.c_str(), new_name.c_str());
    return (result == -1) ? -errno : result;
}

SyscallReturn
truncateFunc(SyscallDesc *desc, int num, Process *p, ExecContext *xc)
{
    string path;

    if (xc->mem->readString(path, xc->getSyscallArg(0)) != No_Fault)
	return -EFAULT;

    off_t length = xc->getSyscallArg(1);

    int result = truncate(path.c_str(), length);
    return (result == -1) ? -errno : result;
}

SyscallReturn
ftruncateFunc(SyscallDesc *desc, int num, Process *process, ExecContext *xc)
{
    int fd = process->sim_fd(xc->getSyscallArg(0));

    if (fd < 0)
	return -EBADF;

    off_t length = xc->getSyscallArg(1);

    int result = ftruncate(fd, length);
    return (result == -1) ? -errno : result;
}

SyscallReturn
chownFunc(SyscallDesc *desc, int num, Process *p, ExecContext *xc)
{
    string path;

    if (xc->mem->readString(path, xc->getSyscallArg(0)) != No_Fault)
	return -EFAULT;

    /* XXX endianess */
    uint32_t owner = xc->getSyscallArg(1);
    uid_t hostOwner = owner;
    uint32_t group = xc->getSyscallArg(2);
    gid_t hostGroup = group;

    int result = chown(path.c_str(), hostOwner, hostGroup);
    return (result == -1) ? -errno : result;
}

SyscallReturn
fchownFunc(SyscallDesc *desc, int num, Process *process, ExecContext *xc)
{
    int fd = process->sim_fd(xc->getSyscallArg(0));

    if (fd < 0)
	return -EBADF;

    /* XXX endianess */
    uint32_t owner = xc->getSyscallArg(1);
    uid_t hostOwner = owner;
    uint32_t group = xc->getSyscallArg(2);
    gid_t hostGroup = group;

    int result = fchown(fd, hostOwner, hostGroup);
    return (result == -1) ? -errno : result;
}

SyscallReturn
accessFunc(SyscallDesc *desc, int num, Process *p, ExecContext *xc)
{
    string path;

    if (xc->mem->readString(path, xc->getSyscallArg(0)) != No_Fault)
	return -EFAULT;

    /* XXX flag translation? */
    int mode = xc->getSyscallArg(1);

    int result = access(path.c_str(), mode);
    return (result == -1) ? -errno : result;
}

SyscallReturn
getcwdFunc(SyscallDesc *desc, int num, Process *p, ExecContext *xc)
{

    int path_len = xc->getSyscallArg(1);
    char* pathname = (char*) malloc(path_len*sizeof(char));
    char* retval = getcwd(pathname, path_len);
    assert(retval != NULL);

    BufferArg path(xc->getSyscallArg(0), path_len);
    strncpy((char *)path.bufferPtr(), pathname, path_len);

    path.copyOut(xc->mem);
    int actualPathLen = strlen(pathname);

    return actualPathLen;
}
