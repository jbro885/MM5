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

#ifndef __BUILDER_HH__
#define __BUILDER_HH__

#include <iosfwd>
#include <list>
#include <map>
#include <vector>

#include "sim/param.hh"

class SimObject;

//
// A SimObjectBuilder serves as an evaluation context for a set of
// parameters that describe a specific instance of a SimObject.  This
// evaluation context corresponds to a section in the .ini file (as
// with the base ParamContext) plus an optional node in the
// configuration hierarchy (the configNode member) for resolving
// SimObject references.  SimObjectBuilder is an abstract superclass;
// derived classes specialize the class for particular subclasses of
// SimObject (e.g., BaseCache).
//
// For typical usage, see the definition of
// SimObjectClass::createObject().
//
class SimObjectBuilder : public ParamContext
{
  private:
    // The corresponding node in the configuration hierarchy.
    // (optional: may be null if the created object is not in the
    // hierarchy)
    ConfigNode *configNode;

  public:
    SimObjectBuilder(ConfigNode *_configNode);

    virtual ~SimObjectBuilder();

    // call parse() on all params in this context to convert string
    // representations to parameter values
    virtual void parseParams(IniFile &iniFile);

    // parameter error prolog (override of ParamContext)
    virtual void printErrorProlog(std::ostream &);

    // generate the name for this SimObject instance (derived from the
    // configuration hierarchy node label and position)
    virtual const std::string &getInstanceName() { return iniSection; }

    // return the configuration hierarchy node for this context.
    virtual ConfigNode *getConfigNode() { return configNode; }

    // Create the actual SimObject corresponding to the parameter
    // values in this context.  This function is overridden in derived
    // classes to call a specific constructor for a particular
    // subclass of SimObject.
    virtual SimObject *create() = 0;
};


//
// Handy macros for initializing parameter members of classes derived
// from SimObjectBuilder.  Assumes that the name of the parameter
// member object is the same as the textual parameter name seen by the
// user.  (Note that '#p' is expanded by the preprocessor to '"p"'.)
//
#define INIT_PARAM(p, desc)		p(this, #p, desc)
#define INIT_PARAM_DFLT(p, desc, dflt)	p(this, #p, desc, dflt)

//
// Initialize an enumeration variable... assumes that 'map' is the
// name of an array of mappings (char * for SimpleEnumParam, or
// EnumParamMap for MappedEnumParam).
//
#define INIT_ENUM_PARAM(p, desc, map)	\
	p(this, #p, desc, map, sizeof(map)/sizeof(map[0]))
#define INIT_ENUM_PARAM_DFLT(p, desc, map, dflt)	\
	p(this, #p, desc, map, sizeof(map)/sizeof(map[0]), dflt)

//
// An instance of SimObjectClass corresponds to a class derived from
// SimObject.  The SimObjectClass instance serves to bind the string
// name (found in the config file) to a function that creates an
// instance of the appropriate derived class.
//
// This would be much cleaner in Smalltalk or Objective-C, where types
// are first-class objects themselves.
//
class SimObjectClass
{
  public:
    // Type CreateFunc is a pointer to a function that creates a new
    // simulation object builder based on a .ini-file parameter
    // section (specified by the first string argument), a unique name
    // for the object (specified by the second string argument), and
    // an optional config hierarchy node (specified by the third
    // argument).  A pointer to the new SimObjectBuilder is returned.
    typedef SimObjectBuilder *(*CreateFunc)(ConfigNode *configNode);

    static std::map<std::string,CreateFunc> *classMap;

    // Constructor.  For example:
    //
    // SimObjectClass baseCacheClass("BaseCache", newBaseCacheBuilder);
    //
    SimObjectClass(const std::string &className, CreateFunc createFunc);

    // create SimObject given name of class and pointer to
    // configuration hierarchy node
    static SimObject *createObject(IniFile &configDB, ConfigNode *configNode);

    // print descriptions of all parameters registered with all
    // SimObject classes
    static void describeAllClasses(std::ostream &os);
};

//
// Macros to encapsulate the magic of declaring & defining
// SimObjectBuilder and SimObjectClass objects
//

#define BEGIN_DECLARE_SIM_OBJECT_PARAMS(OBJ_CLASS)		\
class OBJ_CLASS##Builder : public SimObjectBuilder		\
{								\
  public:

#define END_DECLARE_SIM_OBJECT_PARAMS(OBJ_CLASS)		\
								\
    OBJ_CLASS##Builder(ConfigNode *configNode);			\
    virtual ~OBJ_CLASS##Builder() {}				\
								\
    OBJ_CLASS *create();					\
};

#define BEGIN_INIT_SIM_OBJECT_PARAMS(OBJ_CLASS)			\
OBJ_CLASS##Builder::OBJ_CLASS##Builder(ConfigNode *configNode)	\
    : SimObjectBuilder(configNode),


#define END_INIT_SIM_OBJECT_PARAMS(OBJ_CLASS)			\
{								\
}

#define CREATE_SIM_OBJECT(OBJ_CLASS)				\
OBJ_CLASS *OBJ_CLASS##Builder::create()

#define REGISTER_SIM_OBJECT(CLASS_NAME, OBJ_CLASS)		\
SimObjectBuilder *						\
new##OBJ_CLASS##Builder(ConfigNode *configNode)			\
{								\
    return new OBJ_CLASS##Builder(configNode);			\
}								\
								\
SimObjectClass the##OBJ_CLASS##Class(CLASS_NAME,		\
				     new##OBJ_CLASS##Builder);	\
								\
/* see param.hh */						\
DEFINE_SIM_OBJECT_CLASS_NAME(CLASS_NAME, OBJ_CLASS)


#endif // __BUILDER_HH__
