# Copyright (c) 2005
# The Regents of The University of Michigan
# All Rights Reserved
#
# This code is part of the M5 simulator, developed by Nathan Binkert,
# Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
# from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
# and Andrew Schultz.
#
# Permission is granted to use, copy, create derivative works and
# redistribute this software and such derivative works for any
# purpose, so long as the copyright notice above, this grant of
# permission, and the disclaimer below appear in all copies made; and
# so long as the name of The University of Michigan is not used in any
# advertising or publicity pertaining to the use or distribution of
# this software without specific, written prior authorization.
#
# THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
# UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
# WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
# LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
# INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
# ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
# IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGES.

# The SmartDict class fixes a couple of issues with using the content
# of os.environ or similar dicts of strings as Python variables:
#
# 1) Undefined variables should return False rather than raising KeyError.
#
# 2) String values of 'False', '0', etc., should evaluate to False
#    (not just the empty string).
#
# #1 is solved by overriding __getitem__, and #2 is solved by using a
# proxy class for values and overriding __nonzero__ on the proxy.
# Everything else is just to (a) make proxies behave like normal
# values otherwise, (b) make sure any dict operation returns a proxy
# rather than a normal value, and (c) coerce values written to the
# dict to be strings.


from convert import *

class Variable(str):
    """Intelligent proxy class for SmartDict.  Variable will use the
    various convert functions to attempt to convert values to useable
    types"""
    def __int__(self):
        return toInteger(str(self))
    def __long__(self):
        return toLong(str(self))
    def __float__(self):
        return toFloat(str(self))
    def __nonzero__(self):
        return toBool(str(self))
    def convert(self, other):
        t = type(other)
        if t == bool:
            return bool(self)
        if t == int:
            return int(self)
        if t == long:
            return long(self)
        if t == float:
            return float(self)
        return str(self)
    def __lt__(self, other):
        return self.convert(other) < other
    def __le__(self, other):
        return self.convert(other) <= other
    def __eq__(self, other):
        return self.convert(other) == other
    def __ne__(self, other):
        return self.convert(other) != other
    def __gt__(self, other):
        return self.convert(other) > other
    def __ge__(self, other):
        return self.convert(other) >= other

    def __add__(self, other):
        return self.convert(other) + other
    def __sub__(self, other):
        return self.convert(other) - other
    def __mul__(self, other):
        return self.convert(other) * other
    def __div__(self, other):
        return self.convert(other) / other
    def __truediv__(self, other):
        return self.convert(other) / other

    def __radd__(self, other):
        return other + self.convert(other)
    def __rsub__(self, other):
        return other - self.convert(other)
    def __rmul__(self, other):
        return other * self.convert(other)
    def __rdiv__(self, other):
        return other / self.convert(other)
    def __rtruediv__(self, other):
        return other / self.convert(other)

class UndefinedVariable(object):
    """Placeholder class to represent undefined variables.  Will
    generally cause an exception whenever it is used, but evaluates to
    zero for boolean truth testing such as in an if statement"""
    def __nonzero__(self):
        return False
        
class SmartDict(dict):
    """Dictionary class that holds strings, but intelligently converts
    those strings to other types depending on their usage"""

    def __getitem__(self, key):
        """returns a Variable proxy if the values exists in the database and
        returns an UndefinedVariable otherwise"""

        if key in self:
            return Variable(dict.get(self, key))
        else:
            # Note that this does *not* change the contents of the dict,
            # so that even after we call env['foo'] we still get a
            # meaningful answer from "'foo' in env" (which
            # calls dict.__contains__, which we do not override).
            return UndefinedVariable()

    def __setitem__(self, key, item):
        """intercept the setting of any variable so that we always
        store strings in the dict"""
        dict.__setitem__(self, key, str(item))
    
    def values(self):
        return [ Variable(v) for v in dict.values(self) ]

    def itervalues(self):
        for value in dict.itervalues(self):
            yield Variable(value)

    def items(self):
        return [ (k, Variable(v)) for k,v in dict.items(self) ]

    def iteritems(self):
        for key,value in dict.iteritems(self):
            yield key, Variable(value)
    
    def get(self, key, default='False'):
        return Variable(dict.get(self, key, str(default)))

    def setdefault(self, key, default='False'):
        return Variable(dict.setdefault(self, key, str(default)))

__all__ = [ 'SmartDict' ]
