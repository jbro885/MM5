from m5 import *
AddToPath('..')
from DetailedUniConfig import *
import Benchmarks

BaseCPU.workload = [ Benchmarks.GCCLongCP() for i in xrange(4) ]
BaseCPU.max_insts_any_thread = 1000000
root = DetailedStandAlone()
