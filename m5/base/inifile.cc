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

#define USE_CPP

#ifdef USE_CPP
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#endif

#include <fstream>
#include <iostream>

#include <vector>
#include <string>

#include "base/inifile.hh"
#include "base/str.hh"

using namespace std;

IniFile::IniFile()
{}

IniFile::~IniFile()
{
    SectionTable::iterator i = table.begin();
    SectionTable::iterator end = table.end();

    while (i != end) {
	delete (*i).second;
	++i;
    }
}


#ifdef USE_CPP
bool
IniFile::loadCPP(const string &file, vector<char *> &cppArgs)
{
    // Open the file just to verify that we can.  Otherwise if the
    // file doesn't exist or has bad permissions the user will get
    // confusing errors from cpp/g++.
    ifstream tmpf(file.c_str());

    if (!tmpf.is_open())
	return false;

    tmpf.close();

    char *cfile = strncpy(new char[file.size() + 1], file.c_str(),
			  file.size());
    char *dir = dirname(cfile);
    char *dir_arg = NULL;
    if (*dir != '.') {
	string arg = "-I";
	arg += dir;

	dir_arg = new char[arg.size() + 1];
	strncpy(dir_arg, arg.c_str(), arg.size());
    }

    delete [] cfile;

    char tempfile[] = "/tmp/configXXXXXX";
    int tmp_fd = mkstemp(tempfile);

    int pid = fork();

    if (pid == -1)
	return false;

    if (pid == 0) {
	char filename[FILENAME_MAX];
	string::size_type i = file.copy(filename, sizeof(filename) - 1);
	filename[i] = '\0';

	int arg_count = cppArgs.size();

	char **args = new char *[arg_count + 20];

	int nextArg = 0;
	args[nextArg++] = "g++";
	args[nextArg++] = "-E";
	args[nextArg++] = "-P";
	args[nextArg++] = "-nostdinc";
	args[nextArg++] = "-nostdinc++";
	args[nextArg++] = "-x";
	args[nextArg++] = "c++";
	args[nextArg++] = "-undef";

	for (int i = 0; i < arg_count; i++)
	    args[nextArg++] = cppArgs[i];

	if (dir_arg)
	    args[nextArg++] = dir_arg;

	args[nextArg++] = filename;
	args[nextArg++] = NULL;

	close(STDOUT_FILENO);
	if (dup2(tmp_fd, STDOUT_FILENO) == -1)
	    exit(1);

	execvp("g++", args);

	exit(0);
    }

    int retval;
    waitpid(pid, &retval, 0);

    delete [] dir_arg;

    // check for normal completion of CPP
    if (!WIFEXITED(retval) || WEXITSTATUS(retval) != 0)
	return false;

    close(tmp_fd);

    bool status = false;

    status = load(tempfile);

    unlink(tempfile);

    return status;
}
#endif

bool
IniFile::load(const string &file)
{
    ifstream f(file.c_str());

    if (!f.is_open())
	return false;

    return load(f);
}


const string &
IniFile::Entry::getValue() const
{
    referenced = true;
    return value;
}


void
IniFile::Section::addEntry(const std::string &entryName,
			   const std::string &value,
			   bool append)
{
    EntryTable::iterator ei = table.find(entryName);

    if (ei == table.end()) {
	// new entry
	table[entryName] = new Entry(value);
    }
    else if (append) {
	// append new reult to old entry
	ei->second->appendValue(value);
    }
    else {
	// override old entry
	ei->second->setValue(value);
    }
}


bool
IniFile::Section::add(const std::string &assignment)
{
    string::size_type offset = assignment.find('=');
    if (offset == string::npos) {
	// no '=' found
	cerr << "Can't parse .ini line " << assignment << endl;
	return false;
    }

    // if "+=" rather than just "=" then append value
    bool append = (assignment[offset-1] == '+');

    string entryName = assignment.substr(0, append ? offset-1 : offset);
    string value = assignment.substr(offset + 1);

    eat_white(entryName);
    eat_white(value);

    addEntry(entryName, value, append);
    return true;
}


IniFile::Entry *
IniFile::Section::findEntry(const std::string &entryName) const
{
    referenced = true;

    EntryTable::const_iterator ei = table.find(entryName);

    return (ei == table.end()) ? NULL : ei->second;
}


IniFile::Section *
IniFile::addSection(const string &sectionName)
{
    SectionTable::iterator i = table.find(sectionName);

    if (i != table.end()) {
	return i->second;
    }
    else {
	// new entry
	Section *sec = new Section();
	table[sectionName] = sec;
	return sec;
    }
}


IniFile::Section *
IniFile::findSection(const string &sectionName) const
{
    SectionTable::const_iterator i = table.find(sectionName);

    return (i == table.end()) ? NULL : i->second;
}


// Take string of the form "<section>:<parameter>=<value>" and add to
// database.  Return true if successful, false if parse error.
bool
IniFile::add(const string &str)
{
    // find ':'
    string::size_type offset = str.find(':');
    if (offset == string::npos)  // no ':' found
	return false;

    string sectionName = str.substr(0, offset);
    string rest = str.substr(offset + 1);

    eat_white(sectionName);
    Section *s = addSection(sectionName);

    return s->add(rest);
}

bool
IniFile::load(istream &f)
{
    Section *section = NULL;

    while (!f.eof()) {
	f >> ws; // Eat whitespace
	if (f.eof()) {
	    break;
	}

	string line;
	getline(f, line);
	if (line.size() == 0)
	    continue;

	eat_end_white(line);
	int last = line.size() - 1;

	if (line[0] == '[' && line[last] == ']') {
	    string sectionName = line.substr(1, last - 1);
	    eat_white(sectionName);
	    section = addSection(sectionName);
	    continue;
	}

	if (section == NULL)
	    continue;

	if (!section->add(line))
	    return false;
    }

    return true;
}

bool
IniFile::find(const string &sectionName, const string &entryName,
	      string &value) const
{
    Section *section = findSection(sectionName);
    if (section == NULL)
	return false;

    Entry *entry = section->findEntry(entryName);
    if (entry == NULL)
	return false;

    value = entry->getValue();

    return true;
}

bool
IniFile::sectionExists(const string &sectionName) const
{
    return findSection(sectionName) != NULL;
}


bool
IniFile::Section::printUnreferenced(const string &sectionName)
{
    bool unref = false;
    bool search_unref_entries = false;
    vector<string> unref_ok_entries;

    Entry *entry = findEntry("unref_entries_ok");
    if (entry != NULL) {
	tokenize(unref_ok_entries, entry->getValue(), ' ');
	if (unref_ok_entries.size()) {
	    search_unref_entries = true;
	}
    }

    for (EntryTable::iterator ei = table.begin();
	 ei != table.end(); ++ei) {
	const string &entryName = ei->first;
	Entry *entry = ei->second;

	if (entryName == "unref_section_ok" ||
	    entryName == "unref_entries_ok")
	{
	    continue;
	}

	if (!entry->isReferenced()) {
	    if (search_unref_entries &&
		(std::find(unref_ok_entries.begin(), unref_ok_entries.end(),
			   entryName) != unref_ok_entries.end()))
	    {
		continue;
	    }

	    cerr << "Parameter " << sectionName << ":" << entryName
		 << " not referenced." << endl;
	    unref = true;
	}
    }

    return unref;
}


bool
IniFile::printUnreferenced()
{
    bool unref = false;

    for (SectionTable::iterator i = table.begin();
	 i != table.end(); ++i) {
	const string &sectionName = i->first;
	Section *section = i->second;

	if (!section->isReferenced()) {
	    if (section->findEntry("unref_section_ok") == NULL) {
		cerr << "Section " << sectionName << " not referenced."
		     << endl;
		unref = true;
	    }
	}
	else {
	    if (section->printUnreferenced(sectionName)) {
		unref = true;
	    }
	}
    }

    return unref;
}


void
IniFile::Section::dump(const string &sectionName)
{
    for (EntryTable::iterator ei = table.begin();
	 ei != table.end(); ++ei) {
	cout << sectionName << ": " << (*ei).first << " => "
	     << (*ei).second->getValue() << "\n";
    }
}

void
IniFile::dump()
{
    for (SectionTable::iterator i = table.begin();
	 i != table.end(); ++i) {
	i->second->dump(i->first);
    }
}
