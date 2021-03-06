from m5 import *
from BaseCPU import BaseCPU

class SimpleCPU(BaseCPU):
    type = 'SimpleCPU'
    width = Param.Int(1, "CPU width")
    function_trace = Param.Bool(False, "Enable function trace")
    function_trace_start = Param.Tick(0, "Cycle to start function trace")
    simpoint_bbv_size = Param.Int("Number of instructions in each BBV point")
    checkpoint_at_instruction = Param.Counter("Dump checkpoint and quit after n instructions")
