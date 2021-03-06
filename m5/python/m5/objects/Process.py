from m5 import *
class Process(SimObject):
    type = 'Process'
    abstract = True
    output = Param.String('cout', 'filename for stdout/stderr')

class LiveProcess(Process):
    type = 'LiveProcess'
    executable = Param.String('', "executable (overrides cmd[0] if set)")
    cmd = VectorParam.String("command line (executable plus arguments)")
    env = VectorParam.String('', "environment settings")
    input = Param.String('cin', "filename for stdin")
    maxMemMB = Param.Int("Maximum memory consumption of functional memory in MB")
    cpuID = Param.Int("The ID of the CPU this process is running on")
    victimEntries = Param.Int("The size of the victim buffer")

class EioProcess(Process):
    type = 'EioProcess'
    chkpt = Param.String('', "EIO checkpoint file name (optional)")
    file = Param.String("EIO trace file name")
