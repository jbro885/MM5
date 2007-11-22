from m5 import *
from FunctionalMemory import FunctionalMemory

# This device exists only because there are some devices that I don't
# want to have a Platform parameter because it would cause a cycle in
# the C++ that cannot be easily solved.
#
# The real solution to this problem is to pass the ParamXXX structure
# to the constructor, but with the express condition that SimObject
# parameter values are not to be available at construction time.  If
# some further configuration must be done, it must be done during the
# initialization phase at which point all SimObject pointers will be
# valid.
class FooPioDevice(FunctionalMemory):
    type = 'PioDevice'
    abstract = True
    addr = Param.Addr("Device Address")
    mmu = Param.MemoryController(Parent.any, "Memory Controller")
    io_bus = Param.Bus(NULL, "The IO Bus to attach to")
    pio_latency = Param.Tick(1, "Programmed IO latency in bus cycles")

class FooDmaDevice(FooPioDevice):
    type = 'DmaDevice'
    abstract = True

class PioDevice(FooPioDevice):
    type = 'PioDevice'
    abstract = True
    platform = Param.Platform(Parent.any, "Platform")

class DmaDevice(PioDevice):
    type = 'DmaDevice'
    abstract = True

