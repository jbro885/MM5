from m5 import *

class InterferenceProbabilityPolicy(Enum): vals = ['random', 'sequential']

class CacheInterference(SimObject):
    type = 'CacheInterference'

    cpuCount = Param.Int("Number of cores")
    leaderSets = Param.Int("Number of leader sets")
    size = Param.MemorySize("Cache capacity in bytes")
    load_ipp = Param.InterferenceProbabilityPolicy("interference probability policy to use for loads")
    writeback_ipp = Param.InterferenceProbabilityPolicy("interference probability policy to use for writebacks")
    interferenceManager= Param.InterferenceManager("Pointer to the interference manager")
    blockSize= Param.Int("Cache block size")
    assoc= Param.Int("Associativity")
    hitLatency = Param.Latency("The cache hit latency")
    divisionFactor = Param.Int("The number of cores in shared mode when run in private mode")
    constituencyFactor = Param.Float("The average percentage of blocks accessed in a constituency")
