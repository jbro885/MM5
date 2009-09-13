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

#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>

#include <cstdio>
#include <string>

#include "base/intmath.hh"
#include "base/loader/object_file.hh"
#include "base/loader/symtab.hh"
#include "base/statistics.hh"
#include "config/full_system.hh"
#include "cpu/exec_context.hh"
#include "cpu/smt.hh"
#include "encumbered/cpu/full/thread.hh"
#include "encumbered/eio/eio.hh"
#include "encumbered/mem/functional/main.hh"
#include "sim/builder.hh"
#include "sim/fake_syscall.hh"
#include "sim/process.hh"
#include "sim/stats.hh"

#ifdef TARGET_ALPHA
#include "arch/alpha/alpha_tru64_process.hh"
#include "arch/alpha/alpha_linux_process.hh"
#endif

using namespace std;

//
// The purpose of this code is to fake the loader & syscall mechanism
// when there's no OS: thus there's no resone to use it in FULL_SYSTEM
// mode when we do have an OS
//
#if FULL_SYSTEM
#error "process.cc not compatible with FULL_SYSTEM"
#endif

// current number of allocated processes
int num_processes = 0;

Process::Process(const string &nm,
		 int stdin_fd, 	// initial I/O descriptors
		 int stdout_fd,
		 int stderr_fd,
		 int _memSizeMB)
    : SimObject(nm)
{

	// allocate memory space
	memory = new MainMemory(name() + ".MainMem", _memSizeMB);

    // allocate initial register file
    init_regs = new RegFile;
    memset(init_regs, 0, sizeof(RegFile));

    // initialize first 3 fds (stdin, stdout, stderr)
    fd_map[STDIN_FILENO] = stdin_fd;
    fd_map[STDOUT_FILENO] = stdout_fd;
    fd_map[STDERR_FILENO] = stderr_fd;

    // mark remaining fds as free
    for (int i = 3; i <= MAX_FD; ++i) {
    	fd_map[i] = -1;
    }

    mmap_start = mmap_end = 0;
    nxm_start = nxm_end = 0;
    // other parameters will be initialized when the program is loaded
}

void
Process::regStats()
{
    using namespace Stats;

    num_syscalls
	.name(name() + ".PROG:num_syscalls")
	.desc("Number of system calls")
	;
}

//
// static helper functions
//
int
Process::openInputFile(const string &filename)
{
    int fd = open(filename.c_str(), O_RDONLY);

    if (fd == -1) {
	perror(NULL);
	cerr << "unable to open \"" << filename << "\" for reading\n";
	fatal("can't open input file");
    }

    return fd;
}


int
Process::openOutputFile(const string &filename)
{
    int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0774);

    if (fd == -1) {
	perror(NULL);
	cerr << "unable to open \"" << filename << "\" for writing\n";
	fatal("can't open output file");
    }

    return fd;
}


int
Process::registerExecContext(ExecContext *xc)
{
    // add to list
    int myIndex = execContexts.size();
    execContexts.push_back(xc);

    if (myIndex == 0) {
	// copy process's initial regs struct
	xc->regs = *init_regs;
    }

    // return CPU number to caller and increment available CPU count
    return myIndex;
}

void
Process::startup()
{
    if (execContexts.empty())
	return;

    // first exec context for this process... initialize & enable
    ExecContext *xc = execContexts[0];

    // mark this context as active so it will start ticking.
    xc->activate(0);
}

void
Process::replaceExecContext(ExecContext *xc, int xcIndex)
{
    if (xcIndex >= execContexts.size()) {
	panic("replaceExecContext: bad xcIndex, %d >= %d\n",
	      xcIndex, execContexts.size());
    }

    execContexts[xcIndex] = xc;
}

// map simulator fd sim_fd to target fd tgt_fd
void
Process::dup_fd(int sim_fd, int tgt_fd)
{
    if (tgt_fd < 0 || tgt_fd > MAX_FD)
	panic("Process::dup_fd tried to dup past MAX_FD (%d)", tgt_fd);

    fd_map[tgt_fd] = sim_fd;
}


// generate new target fd for sim_fd
int
Process::open_fd(int sim_fd, FileParameters params)
{
    int free_fd;

    // in case open() returns an error, don't allocate a new fd
    if (sim_fd == -1) return -1;

    // find first free target fd
    for (free_fd = 0; fd_map[free_fd] >= 0; ++free_fd) {
    	if (free_fd == MAX_FD) panic("Process::open_fd: out of file descriptors!");
    }

    fd_map[free_fd] = sim_fd;

    assert(tgtFDFileParams.find(free_fd) == tgtFDFileParams.end());
    tgtFDFileParams[free_fd] = params;
    return free_fd;
}

void
Process::close_fd(int tgt_fd){
	tgtFDFileParams.erase(tgt_fd);
	assert(tgtFDFileParams.find(tgt_fd) == tgtFDFileParams.end());
}


// look up simulator fd for given target fd
int
Process::sim_fd(int tgt_fd)
{
    if (tgt_fd > MAX_FD)
	return -1;

    return fd_map[tgt_fd];
}

std::string
Process::generateFileStateName(const char* prefix, int tgt_fd){
	stringstream tmp;
	tmp << prefix << "_" << tgt_fd;
	return tmp.str();
}

void
Process::serialize(std::ostream &os){

	// serialize members
	SERIALIZE_SCALAR(text_base);
	SERIALIZE_SCALAR(text_size);

	SERIALIZE_SCALAR(data_base);
	SERIALIZE_SCALAR(data_size);

	SERIALIZE_SCALAR(brk_point);

	SERIALIZE_SCALAR(stack_base);
	SERIALIZE_SCALAR(stack_size);
	SERIALIZE_SCALAR(stack_min);

	SERIALIZE_SCALAR(next_thread_stack_base);

	SERIALIZE_SCALAR(mmap_start);
	SERIALIZE_SCALAR(mmap_end);

	SERIALIZE_SCALAR(nxm_start);
	SERIALIZE_SCALAR(nxm_end);

	// serialize open file state
	map<int, FileParameters>::iterator it = tgtFDFileParams.begin();
	for( ; it != tgtFDFileParams.end(); it++){

		int tgtFD = it->first;
		int thisSimFD = sim_fd(it->first);
		off_t pos = lseek(thisSimFD, 0, SEEK_CUR);
		FileParameters params = it->second;

		SERIALIZE_SCALAR_NAME(generateFileStateName("pos", tgtFD), pos);
		SERIALIZE_SCALAR_NAME(generateFileStateName("path", tgtFD), params.path);
		SERIALIZE_SCALAR_NAME(generateFileStateName("host_flags", tgtFD), params.hostFlags);
		SERIALIZE_SCALAR_NAME(generateFileStateName("mode", tgtFD), params.mode);
	}

	//NOTE: unserialization of MainMem is done automatically from SimObject
}

void
Process::unserialize(Checkpoint *cp, const std::string &section){

	// unserialize members
	UNSERIALIZE_SCALAR(text_base);
	UNSERIALIZE_SCALAR(text_size);

	UNSERIALIZE_SCALAR(data_base);
	UNSERIALIZE_SCALAR(data_size);

	UNSERIALIZE_SCALAR(brk_point);

	UNSERIALIZE_SCALAR(stack_base);
	UNSERIALIZE_SCALAR(stack_size);
	UNSERIALIZE_SCALAR(stack_min);

	UNSERIALIZE_SCALAR(next_thread_stack_base);

	UNSERIALIZE_SCALAR(mmap_start);
	UNSERIALIZE_SCALAR(mmap_end);

	UNSERIALIZE_SCALAR(nxm_start);
	UNSERIALIZE_SCALAR(nxm_end);

	// unserialize process memory
	memory->unserialize(cp, section+".MainMem");

	assert(fd_map[STDIN_FILENO] != -1);
	assert(fd_map[STDOUT_FILENO] != -1);
	assert(fd_map[STDERR_FILENO] != -1);

	// unserialize open files
	for(int i=3;i<MAX_FD+1;i++){
		string pathName = generateFileStateName("path", i);
		string path;

		assert(fd_map[i] == -1);
		if(cp->find(section, pathName, path)){

			int pos = 0;
			int hostFlags = 0;
			int mode = 0;

			UNSERIALIZE_SCALAR_NAME(generateFileStateName("pos",i), pos);
			UNSERIALIZE_SCALAR_NAME(generateFileStateName("host_flags",i), hostFlags);
			UNSERIALIZE_SCALAR_NAME(generateFileStateName("mode",i), mode);

			char* base = basename((char*) path.c_str());
			cout << "path " << path << " gave basename " << base << "\n";

			int sim_fd = open(base, hostFlags, mode);
			if(sim_fd == -1){
				panic("Could not open file %s in unserialize", path.c_str());
			}

			int newPos = lseek(sim_fd, pos, SEEK_SET);
			assert(newPos == pos);

			fd_map[i] = sim_fd;
		}
	}
}

//
// need to declare these here since there is no concrete Process type
// that can be constructed (i.e., no REGISTER_SIM_OBJECT() macro call,
// which is where these get declared for concrete types).
//
DEFINE_SIM_OBJECT_CLASS_NAME("Process", Process)


////////////////////////////////////////////////////////////////////////
//
// LiveProcess member definitions
//
////////////////////////////////////////////////////////////////////////


static void
copyStringArray(vector<string> &strings, Addr array_ptr, Addr data_ptr,
		FunctionalMemory *memory)
{
    for (int i = 0; i < strings.size(); ++i) {
	memory->access(Write, array_ptr, &data_ptr, sizeof(Addr));
	memory->writeString(data_ptr, strings[i].c_str());
	array_ptr += sizeof(Addr);
	data_ptr += strings[i].size() + 1;
    }
    // add NULL terminator
    data_ptr = 0;
    memory->access(Write, array_ptr, &data_ptr, sizeof(Addr));
}

LiveProcess::LiveProcess(const string &nm, ObjectFile *objFile,
			 int stdin_fd, int stdout_fd, int stderr_fd,
			 vector<string> &argv, vector<string> &envp,
			 int _memSizeMB)
    : Process(nm, stdin_fd, stdout_fd, stderr_fd, _memSizeMB)
{
    prog_fname = argv[0];

    prog_entry = objFile->entryPoint();
    text_base = objFile->textBase();
    text_size = objFile->textSize();
    data_base = objFile->dataBase();
    data_size = objFile->dataSize() + objFile->bssSize();
    brk_point = RoundUp<uint64_t>(data_base + data_size, VMPageSize);

    // load object file into target memory
    objFile->loadSections(memory);

    // load up symbols, if any... these may be used for debugging or
    // profiling.
    if (!debugSymbolTable) {
	debugSymbolTable = new SymbolTable();
	if (!objFile->loadGlobalSymbols(debugSymbolTable) ||
	    !objFile->loadLocalSymbols(debugSymbolTable)) {
	    // didn't load any symbols
	    delete debugSymbolTable;
	    debugSymbolTable = NULL;
	}
    }

    // Set up stack.  On Alpha, stack goes below text section.  This
    // code should get moved to some architecture-specific spot.
    stack_base = text_base - (409600+4096);

    // Set up region for mmaps.  Tru64 seems to start just above 0 and
    // grow up from there.
    mmap_start = mmap_end = 0x10000;

    // Set pointer for next thread stack.  Reserve 8M for main stack.
    next_thread_stack_base = stack_base - (8 * 1024 * 1024);

    // Calculate how much space we need for arg & env arrays.
    int argv_array_size = sizeof(Addr) * (argv.size() + 1);
    int envp_array_size = sizeof(Addr) * (envp.size() + 1);
    int arg_data_size = 0;
    for (int i = 0; i < argv.size(); ++i) {
	arg_data_size += argv[i].size() + 1;
    }
    int env_data_size = 0;
    for (int i = 0; i < envp.size(); ++i) {
	env_data_size += envp[i].size() + 1;
    }

    int space_needed =
	argv_array_size + envp_array_size + arg_data_size + env_data_size;
    // for SimpleScalar compatibility
    if (space_needed < 16384)
	space_needed = 16384;

    // set bottom of stack
    stack_min = stack_base - space_needed;
    // align it
    stack_min &= ~7;
    stack_size = stack_base - stack_min;

    // map out initial stack contents
    Addr argv_array_base = stack_min + sizeof(uint64_t); // room for argc
    Addr envp_array_base = argv_array_base + argv_array_size;
    Addr arg_data_base = envp_array_base + envp_array_size;
    Addr env_data_base = arg_data_base + arg_data_size;

    // write contents to stack
    uint64_t argc = argv.size();
    memory->access(Write, stack_min, &argc, sizeof(uint64_t));

    copyStringArray(argv, argv_array_base, arg_data_base, memory);
    copyStringArray(envp, envp_array_base, env_data_base, memory);

    init_regs->intRegFile[ArgumentReg0] = argc;
    init_regs->intRegFile[ArgumentReg1] = argv_array_base;
    init_regs->intRegFile[StackPointerReg] = stack_min;
    init_regs->intRegFile[GlobalPointerReg] = objFile->globalPointer();
    init_regs->pc = prog_entry;
    init_regs->npc = prog_entry + sizeof(MachInst);
}


LiveProcess *
LiveProcess::create(const string &nm,
		    int stdin_fd, int stdout_fd, int stderr_fd,
		    string executable,
		    vector<string> &argv, vector<string> &envp,
		    int _maxMemMB)
{
    LiveProcess *process = NULL;
    ObjectFile *objFile = createObjectFile(executable);
    if (objFile == NULL) {
    	fatal("Can't load object file %s", executable);
    }

    // check object type & set up syscall emulation pointer
    if (objFile->getArch() == ObjectFile::Alpha) {
    	switch (objFile->getOpSys()) {
    	case ObjectFile::Tru64:
    		process = new AlphaTru64Process(nm, objFile,
    				stdin_fd, stdout_fd, stderr_fd,
    				argv, envp, _maxMemMB);
    		break;

    	case ObjectFile::Linux:
    		process = new AlphaLinuxProcess(nm, objFile,
    				stdin_fd, stdout_fd, stderr_fd,
    				argv, envp, _maxMemMB);
    		break;

    	default:
    		fatal("Unknown/unsupported operating system.");
    	}
    } else {
    	fatal("Unknown object file architecture.");
    }

    delete objFile;

    if (process == NULL) fatal("Unknown error creating process object.");

    return process;
}


BEGIN_DECLARE_SIM_OBJECT_PARAMS(LiveProcess)

    VectorParam<string> cmd;
    Param<string> executable;
    Param<string> input;
    Param<string> output;
    VectorParam<string> env;
    Param<int> maxMemMB;

END_DECLARE_SIM_OBJECT_PARAMS(LiveProcess)


BEGIN_INIT_SIM_OBJECT_PARAMS(LiveProcess)

    INIT_PARAM(cmd, "command line (executable plus arguments)"),
    INIT_PARAM(executable, "executable (overrides cmd[0] if set)"),
    INIT_PARAM(input, "filename for stdin (dflt: use sim stdin)"),
    INIT_PARAM(output, "filename for stdout/stderr (dflt: use sim stdout)"),
    INIT_PARAM(env, "environment settings"),
    INIT_PARAM_DFLT(maxMemMB, "Maximum memory consumption of functional memory in MB", 256)

END_INIT_SIM_OBJECT_PARAMS(LiveProcess)


CREATE_SIM_OBJECT(LiveProcess)
{
    string in = input;
    string out = output;

    // initialize file descriptors to default: same as simulator
    int stdin_fd, stdout_fd, stderr_fd;

    if (in == "stdin" || in == "cin")
	stdin_fd = STDIN_FILENO;
    else
	stdin_fd = Process::openInputFile(input);

    if (out == "stdout" || out == "cout")
	stdout_fd = STDOUT_FILENO;
    else if (out == "stderr" || out == "cerr")
	stdout_fd = STDERR_FILENO;
    else
	stdout_fd = Process::openOutputFile(out);

    stderr_fd = (stdout_fd != STDOUT_FILENO) ? stdout_fd : STDERR_FILENO;

    return LiveProcess::create(getInstanceName(),
			       stdin_fd, stdout_fd, stderr_fd,
			       (string)executable == "" ? cmd[0] : executable,
			       cmd, env, maxMemMB);
}

REGISTER_SIM_OBJECT("LiveProcess", LiveProcess)
