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

#ifndef __SIM_PARAM_HH__
#define __SIM_PARAM_HH__

#include <iostream>
#include <list>
#include <string>
#include <vector>

#include "sim/configfile.hh"
#include "sim/startup.hh"

// forward decls
class BaseParam;
class SimObject;

//
// The context of a parameter definition... usually a subclass of
// SimObjectBuilder (which derives from ParamContext), but abstracted
// here to support more global simulator control parameters as well.
//
class ParamContext : protected StartupCallback
{
  private:

    // static list of all ParamContext objects, built as a side effect
    // of the ParamContext constructor
    static std::list<ParamContext *> *ctxList;

  protected:

    // .ini file (database) for parameter lookup... initialized on call
    // to parseParams()
    IniFile *iniFilePtr;

    // .ini file section for parameter lookup
    const std::string iniSection;

    typedef std::vector<BaseParam *> ParamList;

    // list of parameters defined in this context
    ParamList *paramList;

    ParamList *getParamList() {
	if (!paramList)
	    paramList = new ParamList;
	return paramList;
    }

  public:

    /// Initialization phases for ParamContext objects.
    enum InitPhase {
	NoAutoInit = -1,	///< Don't initialize at all... params
				/// will be parsed later (used by
				/// SimObjectBuilder, which parses
				/// params in SimObject::create().
	OutputInitPhase = 0,	///< Output stream initialization
	TraceInitPhase = 1,	///< Trace context initialization:
				/// depends on output streams, but
				/// needs to come before others so we
				/// can use tracing in other
				/// ParamContext init code
	StatsInitPhase = 2,	///< Stats output initialization
	DefaultInitPhase = 3	///< Everything else
    };

    /// Records the initialization phase for this ParamContext.
    InitPhase initPhase;

    /// Constructor.
    /// @param _iniSection Name of .ini section corresponding to this context.
    /// @param _initPhase Initialization phase  (see InitPhase).
    ParamContext(const std::string &_iniSection,
		 InitPhase _initPhase = DefaultInitPhase);

    virtual ~ParamContext() {}

    // add a parameter to the context... called from the parameter
    // object's constructor (see BaseParam::BaseParam())
    void addParam(BaseParam *);

    // call parse() on all params in this context to convert string
    // representations to parameter values
    virtual void parseParams(IniFile &iniFile);

    // Check parameter values for validity & consistency. Default
    // implementation is no-op; derive subclass & override to add
    // actual functionality here
    virtual void checkParams();

    // Clean up at end of execution: close file descriptors, etc.
    // Default implementation is no-op; derive subclass & override to
    // add actual functionality here
    virtual void cleanup();

    // dump parameter descriptions
    void describeParams(std::ostream &);

    // Display the parameters & values used
    void showParams(std::ostream &);

    // print context information for parameter error
    virtual void printErrorProlog(std::ostream &);

    // resolve a SimObject name in this context to an object pointer.
    virtual SimObject *resolveSimObject(const std::string &name);

    // generate the name for this instance of this context (used as a
    // prefix to create unique names in resolveSimObject()
    virtual const std::string &getInstanceName() { return iniSection; }

    // return the configuration hierarchy node for this context.  Bare
    // ParamContext objects have no corresponding node, so the default
    // implementation returns NULL.
    virtual ConfigNode *getConfigNode() { return NULL; }

    // Parse all parameters registered with all ParamContext objects.
    static void parseAllContexts(IniFile &iniFile);

    // Check all parameters registered with all ParamContext objects.
    // (calls checkParams() on each)
    static void checkAllContexts();

    // Print all parameter values on indicated ostream.
    static void showAllContexts(std::ostream &os);

    // Clean up all registered ParamContext objects.  (calls cleanup()
    // on each)
    static void cleanupAllContexts();

    // print descriptions of all parameters registered with all
    // ParamContext objects
    static void describeAllContexts(std::ostream &os);
};


//
// Base class for all parameter objects
//
class BaseParam
{
  public:

    ParamContext *context;
    std::string name;
    std::string description;	// text description for help message
    bool wasSet;	// true if parameter was set by user
    bool hasDefault;	// true if parameter has default value

    BaseParam(ParamContext *_context, const std::string &_name,
	      const std::string &_description, bool _hasDefault)
	: context(_context), name(_name), description(_description),
	  wasSet(false), hasDefault(_hasDefault)
    {
	context->addParam(this);
    }

    virtual ~BaseParam() {}

    // a parameter is valid only if its value was set by the user or
    // it has a default value
    bool isValid() const
    {
	return (wasSet || hasDefault);
    }

    // set value by parsing string
    virtual void parse(const std::string &s) = 0;

    // display value to stream
    virtual void showValue(std::ostream &) const = 0;

    // display type to stream
    virtual void showType(std::ostream &) const = 0;

    // signal parse or usage error
    virtual void die(const std::string &err) const;
};

//
// Template classes to specialize parameters to specific types.
//
// Param<T> is for single-valued (scalar) parameters of type T.
// VectorParam<T> is for multi-valued (vector) parameters of type T.
// These are specified in the .ini file as a space-delimited list of
// arguments.
//
template <class T>
class Param : public BaseParam
{
  protected:

    T value;

  public:

    // Param with default value: set value to default
    Param(ParamContext *context,
	  const std::string &name, const std::string &description, T dfltValue)
	: BaseParam(context, name, description, true),
	  value(dfltValue)
    {
    }

    // Param with no default value: leave value uninitialized
    Param(ParamContext *context,
	  const std::string &name, const std::string &description)
	: BaseParam(context, name, description, false)
    {
    }

    virtual ~Param() {}

    operator T&()
    {
	// if we attempt to reference an invalid parameter (i.e., one
	// with no default value that was not set by the user), die.
	if (!isValid())
	    die("not found");
	return value;
    }

    // display value to stream
    virtual void showValue(std::ostream &os) const;

    // display type to stream
    virtual void showType(std::ostream &) const;

    // set value by parsing string
    virtual void parse(const std::string &s);
};


//
// Template class for vector-valued parameters (lists)
//
template <class T>
class VectorParam : public BaseParam
{
  protected:

    std::vector<T> value;

  public:

    typedef typename std::vector<T>::size_type size_type;

    // Param with default value: set value to default
    VectorParam(ParamContext *context, const std::string &name,
		const std::string &description,
		const std::vector<T> &dfltValue)
	: BaseParam(context, name, description, true),
	  value(dfltValue)
    {
    }

    // Param with no default value: leave value uninitialized
    VectorParam(ParamContext *context,
		const std::string &name, const std::string &description)
	: BaseParam(context, name, description, false)
    {
    }

    virtual ~VectorParam() {}

    // basic vector access methods
    size_type size() const
    {
	if (!isValid())
	    die("not found");
	return value.size();
    }

    const T &operator[](size_type n) const
    {
	if (!isValid())
	    die("not found");
	return value[n];
    }

    // return reference to value vector
    operator std::vector<T>&()
    {
	if (!isValid())
	    die("not found");
	return value;
    }

    // display value to stream
    virtual void showValue(std::ostream &os) const;

    // display type to stream
    virtual void showType(std::ostream &) const;

    // set value by parsing string
    virtual void parse(const std::string &s);
};

//
// Specialization of Param<int> and VectorParam<int> to handle
// enumerated types is done in two ways, using SimpleEnumParam and
// MappedEnumParam (and their vector counterparts,
// SimpleEnumVectorParam and MappedEnumVectorParam).  SimpleEnumParam
// takes an array of strings and maps them to integers based on array
// index.  MappedEnumParam takes an array of string-to-int mappings,
// allowing for mapping strings to non-contiguous integer values, or
// mapping multiple strings to the same integer value.
//
// Both SimpleEnumParam and MappedEnumParam are implemented using a
// single template class, EnumParam<Map>, which takes the type of the map
// as a parameter (const char * or EnumParamMap).  Similarly,
// SimpleEnumVectorParam and MappedEnumVectorParam are both
// implemented using EnumVectorParam<Map>.
//
template <class Map>
class EnumParam : public Param<int>
{
    const int num_values;
    const Map *map;

  public:

    // Param with default value: set value to default
    EnumParam(ParamContext *context,
	      const std::string &name, const std::string &description,
	      const Map *_map, int _num_values,
	      int dfltValue)
	: Param<int>(context, name, description, dfltValue),
	  num_values(_num_values), map(_map)
    {
    }

    // Param with no default value: leave value uninitialized
    EnumParam(ParamContext *context,
	      const std::string &name, const std::string &description,
	      const Map *_map, int _num_values)
	: Param<int>(context, name, description),
	  num_values(_num_values), map(_map)
    {
    }

    virtual ~EnumParam() {}

    // display value to stream
    virtual void showValue(std::ostream &os) const;

    // display type to stream
    virtual void showType(std::ostream &) const;

    // set value by parsing string
    virtual void parse(const std::string &s);
};

//
// Vector counterpart to SimpleEnumParam
//
template <class Map>
class EnumVectorParam : public VectorParam<int>
{
    const int num_values;
    const Map *map;

  public:

    // Param with default value: set value to default
    EnumVectorParam(ParamContext *context,
		    const std::string &name, const std::string &description,
		    const Map *_map, int _num_values,
		    std::vector<int> &dfltValue)
	: VectorParam<int>(context, name, description, dfltValue),
	  num_values(_num_values), map(_map)
    {
    }

    // Param with no default value: leave value uninitialized
    EnumVectorParam(ParamContext *context,
		    const std::string &name, const std::string &description,
		    const Map *_map, int _num_values)
	: VectorParam<int>(context, name, description),
	  num_values(_num_values), map(_map)
    {
    }

    virtual ~EnumVectorParam() {}

    // display value to stream
    virtual void showValue(std::ostream &os) const;

    // display type to stream
    virtual void showType(std::ostream &) const;

    // set value by parsing string
    virtual void parse(const std::string &s);
};

// Specialize EnumParam for a particular enumeration type ENUM
// (automates casting to get value of enum type)

template <class ENUM>
class SimpleEnumParam : public EnumParam<const char *>
{
  public:

    SimpleEnumParam(ParamContext *context,
		    const std::string &name, const std::string &description,
		    const char **_map, int _num_values,
		    ENUM dfltValue)
	: EnumParam<const char *>(context, name, description,
				  _map, _num_values, (int)dfltValue)
    {
    }

    SimpleEnumParam(ParamContext *context,
		    const std::string &name, const std::string &description,
		    const char **_map, int _num_values)
	: EnumParam<const char *>(context, name, description,
				  _map, _num_values)
    {
    }

    operator ENUM() const
    {
	if (!isValid())
	    die("not found");
	return (ENUM)value;
    }
};


// Specialize EnumParam for a particular enumeration type ENUM
// (automates casting to get value of enum type)

template <class ENUM>
class SimpleEnumVectorParam : public EnumVectorParam<const char *>
{
  public:

    // skip default value constructor: too much pain to convert
    // vector<ENUM> initializer to vector<int>


    SimpleEnumVectorParam(ParamContext *context,
			  const std::string &name,
			  const std::string &description,
			  const char **_map, int _num_values)
	: EnumVectorParam<const char *>(context, name, description,
					_map, _num_values)
    {
    }

    ENUM operator[](size_type n)
    {
	if (!isValid())
	    die("not found");
	return (ENUM)value[n];
    }
};


//
// Handle enums via string-to-int map (see comment above).
//

// An array of string-to-int mappings must be supplied using the
// following type.
typedef struct {
    const char *name;
    int value;
} EnumParamMap;

// Specialize EnumParam for a particular enumeration type ENUM
// (automates casting to get value of enum type)

template <class ENUM>
class MappedEnumParam : public EnumParam<EnumParamMap>
{
  public:

    MappedEnumParam(ParamContext *context,
		    const std::string &name, const std::string &description,
		    const EnumParamMap *_map, int _num_values,
		    ENUM dfltValue)
	: EnumParam<EnumParamMap>(context, name, description,
				  _map, _num_values, (int)dfltValue)
    {
    }

    MappedEnumParam(ParamContext *context,
		    const std::string &name, const std::string &description,
		    const EnumParamMap *_map, int _num_values)
	: EnumParam<EnumParamMap>(context, name, description,
				  _map, _num_values)
    {
    }

    operator ENUM()
    {
	if (!isValid())
	    die("not found");
	return (ENUM)value[this->n];
    }
};


// Specialize EnumParam for a particular enumeration type ENUM
// (automates casting to get value of enum type)

template <class ENUM>
class MappedEnumVectorParam : public EnumVectorParam<EnumParamMap>
{
  public:

    // skip default value constructor: too much pain to convert
    // vector<ENUM> initializer to vector<int>


    MappedEnumVectorParam(ParamContext *context,
			  const std::string &name,
			  const std::string &description,
			  const EnumParamMap *_map, int _num_values)
	: EnumVectorParam<EnumParamMap>(context, name, description,
					_map, _num_values)
    {
    }

    ENUM operator[](size_type n)
    {
	if (!isValid())
	    die("not found");
	return (ENUM)value[n];
    }
};


//
// Parameters that point to other simulation objects (e.g. caches,
// busses, etc.) are handled by specializing SimObjectBaseParam to the
// specific subtype.  The main purpose of SimObjectBaseParam is to
// provide a place to stick several helper functions common to all
// SimObject-derived parameters.
//
class SimObjectBaseParam : public BaseParam
{
  public:

    SimObjectBaseParam(ParamContext *context, const std::string &name,
		       const std::string &description, bool hasDefault)
	: BaseParam(context, name, description, hasDefault)
    {
    }

    virtual ~SimObjectBaseParam() {}

    // helper function for SimObjectParam<T>::showValue()
    void showValue(std::ostream &os, SimObject *obj) const;

    // helper function for SimObjectParam<T>::parse()
    void parse(const std::string &s, SimObject *&value);

    // helper function for SimObjectParam<T>::parse()
    void parse(const std::string &s, std::vector<SimObject *>&value_vec);
};


//
// Parameter to a specific type of SimObject.  Note that T must be a
// pointer to a class derived from SimObject (e.g., <CPU *>).
//

template <class T> class SimObjectParam;

template <class T>
class SimObjectParam<T *> : public SimObjectBaseParam
{
  protected:

    T *value;

  public:

    // initialization w/o default
    SimObjectParam(ParamContext *context,
		   const std::string &name, const std::string &description)
	: SimObjectBaseParam(context, name, description, false)
    {
    }

    // initialization wit=h default
    SimObjectParam(ParamContext *context,
		   const std::string &name, const std::string &description,
		   T *dfltValue)
	: SimObjectBaseParam(context, name, description, true),
	  value(dfltValue)
    {
    }

    virtual ~SimObjectParam() {}

    // convert to pointer
    operator T*()
    {
	if (!isValid())
	    die("not found");
	return value;
    }

    T *operator->() const
    {
	if (!isValid())
	    die("not found");
	return value;
    }

    // display value to stream
    virtual void showValue(std::ostream &os) const
    {
	SimObjectBaseParam::showValue(os, value);
    }

    // display type to stream: see REGISTER_SIM_OBJECT macro in
    // sim_object.hh for declaration
    virtual void showType(std::ostream &os) const;

    // set value by parsing string
    virtual void parse(const std::string &s)
    {
	SimObject *so_ptr;
	// first parse to generic SimObject *
	SimObjectBaseParam::parse(s, so_ptr);
	// now dynamic_cast to specific derived type
	value = dynamic_cast<T *>(so_ptr);
	// check for failure of dynamic_cast
	if (value == NULL && so_ptr != NULL)
	    die("not of appropriate type");
    }
};


//
// Vector counterpart to SimObjectParam<T>
//

template <class T> class SimObjectVectorParam;

template <class T>
class SimObjectVectorParam<T *> : public SimObjectBaseParam
{
  protected:

    std::vector<T *> value;

  public:

    typedef typename std::vector<T *>::size_type size_type;

    SimObjectVectorParam(ParamContext *context,
			 const std::string &name,
			 const std::string &description)
	: SimObjectBaseParam(context, name, description, false)
    {
    }

    SimObjectVectorParam(ParamContext *context,
			 const std::string &name,
			 const std::string &description,
			 std::vector<T *> dfltValue)
	: SimObjectBaseParam(context, name, description, true),
	  value(dfltValue)
    {
    }

    virtual ~SimObjectVectorParam() {}

    // basic vector access methods
    size_type size() const
    {
	if (!isValid())
	    die("not found");
	return value.size();
    }

    T *&operator[](size_type n)
    {
	if (!isValid())
	    die("not found");
	return value[n];
    }

    // return reference to value vector
    operator std::vector<T *>&()
    {
	if (!isValid())
	    die("not found");
	return value;
    }

    // display value to stream
    virtual void showValue(std::ostream &os) const
    {
	for (int i = 0; i < value.size(); i++) {
	    if (i != 0)
                os << " ";
	    SimObjectBaseParam::showValue(os, value[i]);
	}
    }

    // display type to stream: see
    virtual void showType(std::ostream &os) const;

    // set value by parsing string
    virtual void parse(const std::string &s)
    {
	std::vector<SimObject *> so_ptr_vec;
	// first parse to generic SimObject * vector (from SimObjectBaseParam)
	SimObjectBaseParam::parse(s, so_ptr_vec);

	value.resize(so_ptr_vec.size());

	for (int i = 0; i < so_ptr_vec.size(); ++i) {
	    // now dynamic_cast to specific derived type
	    value[i] = dynamic_cast<T *>(so_ptr_vec[i]);
	    // check for failure of dynamic_cast
	    if (value[i] == NULL && so_ptr_vec[i] != NULL)
		die("not of appropriate type");
	}
    }
};

//
// Macro to define showType() methods for SimObjectParam &
// SimObjectVectorParam.  Can't do this automatically as it requires a
// string name for the type, which you can't get from a template
// argument.  For concrete derived SimObject types, this macro is
// automatically invoked by REGISTER_SIM_OBJECT() (see sim_object.hh).
//
#define DEFINE_SIM_OBJECT_CLASS_NAME(CLASS_NAME, OBJ_CLASS)		\
template<>								\
void									\
SimObjectParam<OBJ_CLASS *>::showType(std::ostream &os)	const		\
{									\
    os << CLASS_NAME;							\
}									\
									\
template<>								\
void									\
SimObjectVectorParam<OBJ_CLASS *>::showType(std::ostream &os) const	\
{									\
    os << "vector of " << CLASS_NAME;					\
}


//
// Declarations for low-level parsing & displaying functions.  These
// are used internally, but should not be used directly by clients of
// the parameter mechanism, but are declared here so they can be
// shared with the serialization code (see sim/serialize.cc).
template <class T> bool parseParam(const std::string &str, T &data);
template <class T> void showParam(std::ostream &os, const T &data);

#endif // _SIM_PARAM_HH_
