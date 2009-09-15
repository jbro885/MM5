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

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <fstream>
#include <list>
#include <string>
#include <vector>

#include "base/inifile.hh"
#include "base/misc.hh"
#include "base/output.hh"
#include "base/str.hh"
#include "base/trace.hh"
#include "sim/config_node.hh"
#include "sim/eventq.hh"
#include "sim/param.hh"
#include "sim/serialize.hh"
#include "sim/sim_events.hh"
#include "sim/sim_exit.hh"
#include "sim/sim_object.hh"

using namespace std;

int Serializable::maxCount = 0;
int Serializable::count = 0;

void
Serializable::nameOut(ostream &os)
{
    os << "\n[" << name() << "]\n";
}

void
Serializable::nameOut(ostream &os, const string &_name)
{
    os << "\n[" << _name << "]\n";
}

void
Serializable::staticNameOut(ostream &os, const string &_name)
{
    os << "\n[" << _name << "]\n";
}

template <class T>
void
paramOut(ostream &os, const std::string &name, const T &param)
{
    os << name << "=";
    showParam(os, param);
    os << "\n";
}


template <class T>
void
paramIn(Checkpoint *cp, const std::string &section,
	const std::string &name, T &param)
{
    std::string str;
    if (!cp->find(section, name, str) || !parseParam(str, param)) {
	fatal("Can't unserialize '%s:%s'\n", section, name);
    }
}


template <class T>
void
arrayParamOut(ostream &os, const std::string &name,
	      const T *param, int size)
{
    os << name << "=";
    if (size > 0)
	showParam(os, param[0]);
    for (int i = 1; i < size; ++i) {
	os << " ";
	showParam(os, param[i]);
    }
    os << "\n";
}


template <class T>
void
arrayParamIn(Checkpoint *cp, const std::string &section,
		const std::string &name, T *param, int size)
{
	std::string str;
	if (!cp->find(section, name, str)) {
		fatal("Can't unserialize '%s:%s'\n", section, name);
	}

	// code below stolen from VectorParam<T>::parse().
	// it would be nice to unify these somehow...

	vector<string> tokens;

	tokenize(tokens, str, ' ');

	// Need this if we were doing a vector
	// value.resize(tokens.size());

	if (tokens.size() != size) {
		fatal("Array size mismatch on %s:%s'\n", section, name);
	}

	for (int i = 0; i < tokens.size(); i++) {
		// need to parse into local variable to handle vector<bool>,
		// for which operator[] returns a special reference class
		// that's not the same as 'bool&', (since it's a packed
		// vector)
		T scalar_value;
		if (!parseParam(tokens[i], scalar_value)) {
			string err("could not parse \"");

			err += str;
			err += "\"";

			fatal(err);
		}

		// assign parsed value to vector
		param[i] = scalar_value;
	}
}


void
objParamIn(Checkpoint *cp, const std::string &section,
	   const std::string &name, Serializable * &param)
{
    if (!cp->findObj(section, name, param)) {
	fatal("Can't unserialize '%s:%s'\n", section, name);
    }
}


#define INSTANTIATE_PARAM_TEMPLATES(type)				\
template void								\
paramOut(ostream &os, const std::string &name, type const &param);	\
template void								\
paramIn(Checkpoint *cp, const std::string &section,			\
	const std::string &name, type & param);				\
template void								\
arrayParamOut(ostream &os, const std::string &name,			\
	      type const *param, int size);				\
template void								\
arrayParamIn(Checkpoint *cp, const std::string &section,		\
	     const std::string &name, type *param, int size);

INSTANTIATE_PARAM_TEMPLATES(signed char)
INSTANTIATE_PARAM_TEMPLATES(unsigned char)
INSTANTIATE_PARAM_TEMPLATES(signed short)
INSTANTIATE_PARAM_TEMPLATES(unsigned short)
INSTANTIATE_PARAM_TEMPLATES(signed int)
INSTANTIATE_PARAM_TEMPLATES(unsigned int)
INSTANTIATE_PARAM_TEMPLATES(signed long)
INSTANTIATE_PARAM_TEMPLATES(unsigned long)
INSTANTIATE_PARAM_TEMPLATES(signed long long)
INSTANTIATE_PARAM_TEMPLATES(unsigned long long)
INSTANTIATE_PARAM_TEMPLATES(bool)
INSTANTIATE_PARAM_TEMPLATES(string)


/////////////////////////////

/// Container for serializing global variables (not associated with
/// any serialized object).
class Globals : public Serializable
{
  public:
    const string name() const;
    void serialize(ostream &os);
    void unserialize(Checkpoint *cp);
};

/// The one and only instance of the Globals class.
Globals globals;

const string
Globals::name() const
{
    return "Globals";
}

void
Globals::serialize(ostream &os)
{
    nameOut(os);
    SERIALIZE_SCALAR(curTick);

    nameOut(os, "MainEventQueue");
    mainEventQueue.serialize(os);
}

void
Globals::unserialize(Checkpoint *cp)
{

	// Magnus: don't restore globals since they will differ between cores due to SimPoints
//	const string &section = name();
//
//	UNSERIALIZE_SCALAR(curTick);
//
//	mainEventQueue.unserialize(cp, "MainEventQueue");
}

void
Serializable::serializeAll()
{
    string dir = Checkpoint::dir();
    if (mkdir(dir.c_str(), 0775) == -1 && errno != EEXIST)
	    fatal("couldn't mkdir %s\n", dir);

    string cpt_file = dir + Checkpoint::baseFilename;
    ofstream outstream(cpt_file.c_str());
    time_t t = time(NULL);
    outstream << "// checkpoint generated: " << ctime(&t);

    globals.serialize(outstream);
    SimObject::serializeAll(outstream);

    if (maxCount && ++count >= maxCount)
        SimExit(curTick + 1, "Maximum number of checkpoints dropped");
}


void
Serializable::unserializeGlobals(Checkpoint *cp)
{
    globals.unserialize(cp);
}


class SerializeEvent : public Event
{
  protected:
    Tick repeat;

  public:
    SerializeEvent(Tick _when, Tick _repeat);
    virtual void process();
    virtual void serialize(std::ostream &os)
    {
	panic("Cannot serialize the SerializeEvent");
    }

};

SerializeEvent::SerializeEvent(Tick _when, Tick _repeat)
    : Event(&mainEventQueue, Serialize_Pri), repeat(_repeat)
{
    setFlags(AutoDelete);
    schedule(_when);
}

void
SerializeEvent::process()
{
    Serializable::serializeAll();
    if (repeat)
	schedule(curTick + repeat);
}

const char *Checkpoint::baseFilename = "m5.cpt";

static string checkpointDirBase;

string
Checkpoint::dir()
{
    // use csprintf to insert curTick into directory name if it
    // appears to have a format placeholder in it.
    return (checkpointDirBase.find("%") != string::npos) ?
	csprintf(checkpointDirBase, curTick) : checkpointDirBase;
}

void
Checkpoint::setup(Tick when, Tick period)
{
    new SerializeEvent(when, period);
}

class SerializeParamContext : public ParamContext
{
  private:
    SerializeEvent *event;

  public:
    SerializeParamContext(const string &section);
    ~SerializeParamContext();
    void checkParams();
};

SerializeParamContext serialParams("serialize");

Param<string> serialize_dir(&serialParams, "dir",
			    "dir to stick checkpoint in "
			    "(sprintf format with cycle #)");

Param<Counter> serialize_cycle(&serialParams,
				"cycle",
				"cycle to serialize",
				0);

Param<Counter> serialize_period(&serialParams,
				"period",
				"period to repeat serializations",
				0);

Param<int> serialize_count(&serialParams, "count",
			   "maximum number of checkpoints to drop");

SerializeParamContext::SerializeParamContext(const string &section)
    : ParamContext(section), event(NULL)
{ }

SerializeParamContext::~SerializeParamContext()
{
}

void
SerializeParamContext::checkParams()
{
    checkpointDirBase = simout.resolve(serialize_dir);

    // guarantee that directory ends with a '/'
    if (checkpointDirBase[checkpointDirBase.size() - 1] != '/')
	checkpointDirBase += "/";

    if (serialize_cycle > 0)
	Checkpoint::setup(serialize_cycle, serialize_period);

    Serializable::maxCount = serialize_count;
}

void
debug_serialize()
{
    Serializable::serializeAll();
}

void
debug_serialize(Tick when)
{
    new SerializeEvent(when, 0);
}

////////////////////////////////////////////////////////////////////////
//
// SerializableClass member definitions
//
////////////////////////////////////////////////////////////////////////

// Map of class names to SerializableBuilder creation functions.
// Need to make this a pointer so we can force initialization on the
// first reference; otherwise, some SerializableClass constructors
// may be invoked before the classMap constructor.
map<string,SerializableClass::CreateFunc> *SerializableClass::classMap = 0;

// SerializableClass constructor: add mapping to classMap
SerializableClass::SerializableClass(const string &className,
				       CreateFunc createFunc)
{
    if (classMap == NULL)
	classMap = new map<string,SerializableClass::CreateFunc>();

    if ((*classMap)[className])
    {
	cerr << "Error: simulation object class " << className << " redefined"
	     << endl;
	fatal("");
    }

    // add className --> createFunc to class map
    (*classMap)[className] = createFunc;
}


//
//
Serializable *
SerializableClass::createObject(Checkpoint *cp,
				 const std::string &section)
{
    string className;

    if (!cp->find(section, "type", className)) {
	fatal("Serializable::create: no 'type' entry in section '%s'.\n",
	      section);
    }

    CreateFunc createFunc = (*classMap)[className];

    if (createFunc == NULL) {
	fatal("Serializable::create: no create function for class '%s'.\n",
	      className);
    }

    Serializable *object = createFunc(cp, section);

    assert(object != NULL);

    return object;
}


Serializable *
Serializable::create(Checkpoint *cp, const std::string &section)
{
    Serializable *object = SerializableClass::createObject(cp, section);
    object->unserialize(cp, section);
    return object;
}


Checkpoint::Checkpoint(const std::string &cpt_dir, const std::string &path,
		       const ConfigNode *_configNode)
    : db(new IniFile), basePath(path), configNode(_configNode), cptDir(cpt_dir)
{
    string filename = cpt_dir + "/" + Checkpoint::baseFilename;
    if (!db->load(filename)) {
	fatal("Can't load checkpoint file '%s'\n", filename);
    }
}

Checkpoint::~Checkpoint(){
	delete db;
}


bool
Checkpoint::find(const std::string &section, const std::string &entry,
		 std::string &value)
{
    return db->find(section, entry, value);
}


bool
Checkpoint::findObj(const std::string &section, const std::string &entry,
		    Serializable *&value)
{
    string path;

    if (!db->find(section, entry, path))
	return false;

    if ((value = configNode->resolveSimObject(path)) != NULL)
	return true;

    if ((value = objMap[path]) != NULL)
	return true;

    return false;
}


bool
Checkpoint::sectionExists(const std::string &section)
{
    return db->sectionExists(section);
}
