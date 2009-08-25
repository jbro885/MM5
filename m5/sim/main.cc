/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005
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

///
/// @file sim/main.cc
///
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <signal.h>

#include <list>
#include <string>
#include <vector>

#include "base/copyright.hh"
#include "base/embedfile.hh"
#include "base/inifile.hh"
#include "base/misc.hh"
#include "base/output.hh"
#include "base/pollevent.hh"
#include "base/statistics.hh"
#include "base/str.hh"
#include "base/time.hh"
#include "cpu/base.hh"
#include "cpu/smt.hh"
#include "python/pyconfig.hh"
#include "sim/async.hh"
#include "sim/builder.hh"
#include "sim/configfile.hh"
#include "sim/host.hh"
#include "sim/sim_events.hh"
#include "sim/sim_exit.hh"
#include "sim/sim_object.hh"
#include "sim/stat_control.hh"
#include "sim/stats.hh"
#include "sim/root.hh"

using namespace std;

// See async.h.
volatile bool async_event = false;
volatile bool async_dump = false;
volatile bool async_dumpreset = false;
volatile bool async_exit = false;
volatile bool async_io = false;
volatile bool async_alarm = false;

/// Stats signal handler.
void
dumpStatsHandler(int sigtype)
{
    async_event = true;
    async_dump = true;
}

void
dumprstStatsHandler(int sigtype)
{
    async_event = true;
    async_dumpreset = true;
}

/// Exit signal handler.
void
exitNowHandler(int sigtype)
{
    if(sigtype == 15){
        cout << "SIGTERM recieved, terminating...\n";
    }

    async_event = true;
    async_exit = true;
}

/// Abort signal handler.
void
abortHandler(int sigtype)
{
    cerr << "Program aborted at cycle " << curTick << endl;

#if TRACING_ON
    // dump trace buffer, if there is one
    Trace::theLog.dump(cerr);
#endif
}

/// Simulator executable name
char *myProgName = (char*) "";

/// Show brief help message.
void
showBriefHelp(ostream &out)
{
    char *prog = basename(myProgName);

    ccprintf(out, "Usage:\n");
    ccprintf(out,
"%s [-d <dir>] [-E <var>[=<val>]] [-I <dir>] [-P <python>]\n"
"        [--<var>=<val>] <config file>\n"
"\n"
"   -d            set the output directory to <dir>\n"
"   -E            set the environment variable <var> to <val> (or 'True')\n"
"   -I            add the directory <dir> to python's path\n"
"   -P            execute <python> directly in the configuration\n"
"   --var=val     set the python variable <var> to '<val>'\n"
"   <configfile>  config file name (ends in .py)\n\n",
	     prog);

    ccprintf(out, "%s -X\n   -X            extract embedded files\n\n", prog);
    ccprintf(out, "%s -h\n   -h            print short help\n\n", prog);
}

/// Print welcome message.
void
sayHello(ostream &out)
{
    extern const char *compileDate;	// from date.cc

    ccprintf(out, "M5 Simulator System\n");
    // display copyright
    ccprintf(out, "%s\n", briefCopyright);
    ccprintf(out, "M5 compiled on %d\n", compileDate);

    char *host = getenv("HOSTNAME");
    if (!host)
	host = getenv("HOST");

    if (host)
	ccprintf(out, "M5 executing on %s\n", host);

    ccprintf(out, "M5 simulation started %s\n", Time::start);
}

///
/// Echo the command line for posterity in such a way that it can be
/// used to rerun the same simulation (given the same .ini files).
///
void
echoCommandLine(int argc, char **argv, ostream &out)
{
    out << "command line: " << argv[0];
    for (int i = 1; i < argc; i++) {
	string arg(argv[i]);

	out << ' ';

	// If the arg contains spaces, we need to quote it.
	// The rest of this is overkill to make it look purty.

	// print dashes first outside quotes
	int non_dash_pos = arg.find_first_not_of("-");
	out << arg.substr(0, non_dash_pos);	// print dashes
	string body = arg.substr(non_dash_pos);	// the rest

	// if it's an assignment, handle the lhs & rhs separately
	int eq_pos = body.find("=");
	if (eq_pos == string::npos) {
	    out << quote(body);
	}
	else {
	    string lhs(body.substr(0, eq_pos));
	    string rhs(body.substr(eq_pos + 1));

	    out << quote(lhs) << "=" << quote(rhs);
	}
    }
    out << endl << endl;
}

char *
getOptionString(int &index, int argc, char **argv)
{
    char *option = argv[index] + 2;
    if (*option != '\0')
	return option;

    // We didn't find an argument, it must be in the next variable.
    if (++index >= argc)
	panic("option string for option '%s' not found", argv[index - 1]);

    return argv[index];
}

#ifndef MAGTEST

int
main(int argc, char **argv)
{
    // Save off program name
    myProgName = argv[0];

    signal(SIGFPE, SIG_IGN);		// may occur on misspeculated paths
    signal(SIGTRAP, SIG_IGN);
    signal(SIGUSR1, dumpStatsHandler);		// dump intermediate stats
    signal(SIGUSR2, dumprstStatsHandler);	// dump and reset stats
    signal(SIGINT, exitNowHandler);		// dump final stats and exit
    signal(SIGABRT, abortHandler);
    signal(SIGTERM, exitNowHandler); // dump stats when killed, Magnus

    bool configfile_found = false;
    PythonConfig pyconfig;
    string outdir;

    if (argc < 2) {
        showBriefHelp(cerr);
        exit(1);
    }

    sayHello(cerr);

    // Parse command-line options.
    // Since most of the complex options are handled through the
    // config database, we don't mess with getopts, and just parse
    // manually.
    for (int i = 1; i < argc; ++i) {
	char *arg_str = argv[i];

	// if arg starts with '--', parse as a special python option
	// of the format --<python var>=<string value>, if the arg
	// starts with '-', it should be a simulator option with a
	// format similar to getopt.  In any other case, treat the
	// option as a configuration file name and load it.
	if (arg_str[0] == '-' && arg_str[1] == '-') {
	    string str = &arg_str[2];
	    string var, val;

	    if (!split_first(str, var, val, '='))
		panic("Could not parse configuration argument '%s'\n"
		      "Expecting --<variable>=<value>\n", arg_str);

	    pyconfig.setVariable(var, val);
	} else if (arg_str[0] == '-') {
	    char *option;
	    string var, val;

	    // switch on second char
	    switch (arg_str[1]) {
	      case 'd':
		outdir = getOptionString(i, argc, argv);
		break;

	      case 'h':
		showBriefHelp(cerr);
		exit(1);

	      case 'E':
		option = getOptionString(i, argc, argv);
		if (!split_first(option, var, val, '='))
		    val = "True";

		if (setenv(var.c_str(), val.c_str(), true) == -1)
		    panic("setenv: %s\n", strerror(errno));
		break;

	      case 'I':
		option = getOptionString(i, argc, argv);
		pyconfig.addPath(option);
		break;

	      case 'P':
		option = getOptionString(i, argc, argv);
		pyconfig.writeLine(option);
		break;

	      case 'X': {
		  list<EmbedFile> lst;
		  EmbedMap::all(lst);
		  list<EmbedFile>::iterator i = lst.begin();
		  list<EmbedFile>::iterator end = lst.end();

		  while (i != end) {
		      cprintf("Embedded File: %s\n", i->name);
		      cout.write(i->data, i->length);
		      ++i;
		  }

		  return 0;
	      }

	      default:
		showBriefHelp(cerr);
		panic("invalid argument '%s'\n", arg_str);
	    }
	} else {
	    string file(arg_str);
	    string base, ext;

	    if (!split_last(file, base, ext, '.') || ext != "py")
		panic("Config file '%s' must end in '.py'\n", file);

	    pyconfig.load(file);
	    configfile_found = true;
	}
    }

    if (outdir.empty()) {
	char *env = getenv("OUTPUT_DIR");
	outdir = env ? env : ".";
    }

    simout.setDirectory(outdir);

    char *env = getenv("CONFIG_OUTPUT");
    if (!env)
	env = (char*) "config.out";
    configStream = simout.find(env);

    if (!configfile_found)
	panic("no configuration file specified!");

    // The configuration database is now complete; start processing it.
    IniFile inifile;
    if (!pyconfig.output(inifile))
	panic("Error processing python code");

    // Initialize statistics database
    Stats::InitSimStats();

    // Now process the configuration hierarchy and create the SimObjects.
    ConfigHierarchy configHierarchy(inifile);
    configHierarchy.build();
    configHierarchy.createSimObjects();

    // Parse and check all non-config-hierarchy parameters.
    ParamContext::parseAllContexts(inifile);
    ParamContext::checkAllContexts();

    // Print hello message to stats file if it's actually a file.  If
    // it's not (i.e. it's cout or cerr) then we already did it above.
    if (simout.isFile(*outputStream))
	sayHello(*outputStream);

    // Echo command line and all parameter settings to stats file as well.
    echoCommandLine(argc, argv, *outputStream);
    ParamContext::showAllContexts(*configStream);

    // Do a second pass to finish initializing the sim objects
    SimObject::initAll();

    // Restore checkpointed state, if any.
    configHierarchy.unserializeSimObjects();

    // Done processing the configuration database.
    // Check for unreferenced entries.
    if (inifile.printUnreferenced())
	panic("unreferenced sections/entries in the intermediate ini file");

    SimObject::regAllStats();

    // uncomment the following to get PC-based execution-time profile
#ifdef DO_PROFILE
    init_profile((char *)&_init, (char *)&_fini);
#endif

    // Check to make sure that the stats package is properly initialized
    Stats::check();

    // Reset to put the stats in a consistent state.
    Stats::setStatsResetState(false);
    Stats::reset();
    Stats::setStatsResetState(false);

    warn("Entering event queue.  Starting simulation...\n");
    SimStartup();
    while (!mainEventQueue.empty()) {

    	assert(curTick <= mainEventQueue.nextTick() &&
    			"event scheduled in the past");

    	// forward current cycle to the time of the first event on the
    	// queue
    	curTick = mainEventQueue.nextTick();

    	mainEventQueue.serviceOne();

    	if (async_event) {
    		async_event = false;
    		if (async_dump) {
    			async_dump = false;

    			using namespace Stats;
    			SetupEvent(Dump, curTick);
    		}

    		if (async_dumpreset) {
    			async_dumpreset = false;

    			using namespace Stats;
    			SetupEvent(Dump | Reset, curTick);
    		}

    		if (async_exit) {
    			async_exit = false;
    			new SimExitEvent("User requested STOP");
    		}

    		if (async_io || async_alarm) {
    			async_io = false;
    			async_alarm = false;
    			pollQueue.service();
    		}
    	}
    }

    // This should never happen... every conceivable way for the
    // simulation to terminate (hit max cycles/insts, signal,
    // simulated system halts/exits) generates an exit event, so we
    // should never run out of events on the queue.
    exitNow("no events on event loop!  All CPUs must be idle.", 1);

    return 0;
}

#endif
