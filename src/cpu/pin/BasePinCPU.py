from m5.defines import buildEnv
from m5.objects.BaseCPU import BaseCPU
from m5.params import *
from m5.SimObject import *

class BasePinCPU(BaseCPU):
    type = "BasePinCPU"
    cxx_header = "cpu/pin/cpu.hh"
    cxx_class = "gem5::pin::CPU"

    @classmethod
    def memory_mode(cls):
        return "atomic"

    @classmethod
    def support_take_over(cls):
        return False

    pinExe = Param.String("Path to Intel Pin executable")
    pinKernel = Param.String("Path to guest Pin kernel")
    pinTool = Param.String("Path to host PinTool")
    pinToolArgs = Param.String("", "Arguments to pass to PinTool")
    pinArgs = Param.String("", "Arguments to pass to Pin")
    
    countInsts = Param.Bool(True, "Enable instruction counting (moderate performance penalty)")
    traceInsts = Param.Bool(False, "Enable instruction tracing (huge performance penalty)")
    enableBBV = Param.Bool(False, "Enable basic block profiling (e.g., for SimPoints)")
    interval = Param.Unsigned(10000000, "Basic block profiling interval (default: 10M instructions)")

    def addSimPointProbe(self, interval: int):
        self.enableBBV = True
        self.interval = interval
        
