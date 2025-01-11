# Copyright (c) 2012-2013 ARM Limited
# All rights reserved.
#
# The license below extends only to copyright in the software and shall
# not be construed as granting a license to any other intellectual
# property including but not limited to intellectual property relating
# to a hardware implementation of the functionality of the software
# licensed hereunder.  You may use the software subject to the license
# terms below provided that you ensure that this notice is replicated
# unmodified and in its entirety in all distributions of the software,
# modified or unmodified, in source code or in binary form.
#
# Copyright (c) 2006-2008 The Regents of The University of Michigan
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Simple test script
#
# "m5 test.py"

import argparse
import json
import os
import sys
import types
import collections

from common import (
    CacheConfig,
    CpuConfig,
    MemConfig,
    ObjectList,
    Options,
    Simulation,
)
from common.Caches import *
from common.FileSystemConfig import config_filesystem

import m5
from m5.defines import buildEnv
from m5.objects import *
from m5.params import NULL
from m5.util import (
    addToPath,
    fatal,
    warn,
)

from gem5.isas import ISA


def get_process(cmd: str, args) -> Process:
    process = Process(pid=100)
    process.executable = cmd
    process.cwd = os.getcwd() if args.chdir is None else args.chdir
    process.gid = os.getgid()

    # Clear out the environment.
    process.env = []

    process.cmd = [cmd, *args.args]

    return process


parser = argparse.ArgumentParser()
parser.add_argument(
    "--chdir",
    type=os.path.abspath,
    help="Set working directory of simulated process",
)
Options.addCommonOptions(parser)
Options.addSEOptions(parser)
parser.add_argument("cmd", help="Executable to simulate")
parser.add_argument("args", nargs="*", help="Arguments to pass to executable")
gem5_root = os.path.dirname(os.path.dirname(__file__))
parser.add_argument(
    "--pin",
    default=os.path.join(gem5_root, "pin", "pin"),
    help="Path to Intel Pin executable",
)
parser.add_argument(
    "--pin-tool",
    default=os.path.join(gem5_root, "pintool", "build", "libclient.so"),
    help="Path to host PinTool",
),
parser.add_argument(
    "--pin-kernel",
    default=os.path.join(gem5_root, "pintool", "build", "kernel"),
    help="Path to Pin guest kernel",
)
parser.add_argument("--pin-args", default = "")
parser.add_argument("--pin-tool-args", default = "")
parser.add_argument("--stdout")
parser.add_argument("--stderr")
parser.add_argument("--bbv", required = True, help = "Path to basic block trace file")
parser.add_argument("--bbvinfo", required = True, help = "Path to basic block extra info file")
parser.add_argument("--warmup", required = True, type = int, help = "Warmup period, in number of instructions")
parser.add_argument("--interval", required = True, type = int, help = "Interval size, in number of instructions")
parser.add_argument("--waypoints", required = True, help = "Path to waypoints list")
args = parser.parse_args()

process = get_process(args.cmd, args)
if args.stdout:
    process.output = args.stdout
if args.stderr:
    process.errout = args.stderr

# NHM-FIXME: Just read the kvm cpu directly?
# To get mem mode: CPUClass.memory_mode()
CPUClass = ObjectList.cpu_list.get("X86PinCPU")
assert int(CPUClass.numThreads) == 1
assert not args.smt
assert args.num_cpus == 1

# NHM-FIXME
np = 1
mp0_path = process.executable
system = System(
    cpu=[CPUClass(cpu_id=i) for i in range(np)],
    mem_mode=CPUClass.memory_mode(),
    mem_ranges=[AddrRange(args.mem_size)],
    cache_line_size=args.cacheline_size,
)
system.shared_backstore = f"physmem"
system.auto_unlink_shared_backstore = True
system.use_pagelist = True
cpu = system.cpu[0]

# Create a top-level voltage domain
system.voltage_domain = VoltageDomain(voltage=args.sys_voltage)

# Create a source clock for the system and set the clock period
system.clk_domain = SrcClockDomain(
    clock=args.sys_clock, voltage_domain=system.voltage_domain
)

# Create a CPU voltage domain
system.cpu_voltage_domain = VoltageDomain()

# Create a separate clock domain for the CPUs
system.cpu_clk_domain = SrcClockDomain(
    clock=args.cpu_clock, voltage_domain=system.cpu_voltage_domain
)

# If elastic tracing is enabled, then configure the cpu and attach the elastic
# trace probe
if args.elastic_trace_en:
    CpuConfig.config_etrace(CPUClass, system.cpu, args)


# Set pin params.
cpu = system.cpu[0]
cpu.pinToolArgs = f"-bbhist 1 -waypoints {args.waypoints} -waypointcount 1 {args.pin_tool_args}"
cpu.pinArgs = args.pin_args
cpu.countInsts = True

# for cpu in system.cpu:
#     cpu.usePerf = True
process.pinInSE = True

# All cpus belong to a common cpu_clk_domain, therefore running at a common
# frequency.
cpu.clk_domain = system.cpu_clk_domain

system.m5ops_base = max(0xFFFF0000, Addr(args.mem_size).getValue())

process.maxStackSize = args.max_stack_size

# NHM-FIXME
cpu.workload = process
cpu.createThreads()

# NHM-FIXME
MemClass = Simulation.setMemClass(args)
system.membus = SystemXBar()
system.system_port = system.membus.cpu_side_ports
CacheConfig.config_cache(args, system)
MemConfig.config_mem(args, system)
config_filesystem(system, args)

system.workload = SEWorkload.init_compatible(mp0_path)

root = Root(full_system=False, system=system)
m5.instantiate()
m5.startup()
exit_sysnos = [
    60, # exit
    231, # exit_group
]
for exit_sysno in exit_sysnos:
    cpu.executePinCommand(f"sysbreak {exit_sysno}")

clean_exit_cause = "exiting with last active thread context"
break_exit_cause = "pin-breakpoint"

def run_until(cmd: str):
    cpu.executePinCommand(cmd)
    exit_cause = m5.simulate()
    if exit_cause == clean_exit_cause:
        return True
    if exit_cause != break_exit_cause:
        print(f"pin-bbv: expected exit cause '{break_exit_cause}', got '{exit_cause}'",
              file = sys.stderr)
        exit(1)
    return False

class Exit(BaseException):
    pass

def run_for_n(counter: str, n: int):
    # TODO: Should standardize command to 'count <name>'
    count = int(cpu.executePinCommand(f"{counter}count"))
    cpu.executePinCommand(f"breakpoint {counter} {count + n}")
    exit_cause = m5.simulate().getCause()
    if exit_cause == clean_exit_cause:
        raise Exit()
    if exit_cause != break_exit_cause:
        print(f"pin-bbv: expected exit cause '{break_exit_cause}', got '{exit_cause}'",
              file = sys.stderr)
        exit(1)

def run_for_n_insts_next_waypoint(n: int) -> (int, int):
    run_for_n("inst", n)
    run_for_n("waypoint", 1)
    return cpu.executePinCommand("waypointcount")

# List of waypoint counts.
warmups = [0]
intervals = []
bbhists = []

try:
    # Prime the loop by running for <warmup> instructions
    # and then discarding and resetting the bbhist.
    intervals.append(run_for_n_insts_next_waypoint(args.warmup))
    
    cpu.executePinCommand("bbhist reset")

    while True:
        # Run for <interval> - <warmup> instructions.
        warmups.append(run_for_n_insts_next_waypoint(args.interval - args.warmup))

        # Run to the end of the current interval (<warmup> instructions).
        intervals.append(run_for_n_insts_next_waypoint(args.warmup))

        # Dump and reset the bbhist.
        bbhists.append(cpu.executePinCommand("bbhist dump"))
        cpu.executePinCommand("bbhist reset")
        print(f"bbv: dumped interval {len(bbhists)}!", file = sys.stderr)

except Exit:
    pass

# The workload exited cleanly.
# Now, process the data into proper files.
#   - bbv.txt: The basic block vector file.
#   - bbv.info.txt: Metadata about the vector file, organized into triples: warmup-begin warmup-end/interval-begin interval-end

assert len(warmups) >= len(intervals) and len(warmups) >= len(bbhists)
assert len(warmups) - len(intervals) <= 1 and len(warmups) - len(bbhists) <= 1

def parse_bbhist(s: str) -> list:
    lines = s.split('\n')
    assert len(lines[-1]) == 0
    lines = lines[:-1]
    result = list()
    
    for line in lines:
        count, block = line.split()
        insts = block.split(',')
        result.append((insts, int(count)))
    return result
    

def bbhist_to_insthist(bbhist: list) -> dict:
    insthist = collections.defaultdict(int)
    for block, count in bbhist:
        for inst in block:
            insthist[inst] += count
    return insthist
    

inst_to_id_dict = dict()
def inst_to_id(inst: str) -> int:
    if inst not in inst_to_id_dict:
        inst_to_id_dict[inst] = len(inst_to_id_dict) + 1
    return inst_to_id_dict[inst]

# Generate bbv.txt.
bbhist_lines = []
with open(args.bbv, "wt") as f:
    for bbhist in bbhists:
        # Generate line to append to bbv.txt.
        insthist = bbhist_to_insthist(parse_bbhist(bbhist))
        f.write('T')
        for inst, count in insthist.items():
            if count > 0:
                id = inst_to_id(inst)
                f.write(f' :{id}:{count}')
        f.write('\n')

# Generate bbv.info.txt.
with open(args.bbvinfo, "wt") as f:
    for i in range(len(bbhists)):
        warmup = warmups[i]
        interval_begin = intervals[i]
        interval_end = intervals[i+1]
        print(warmup, interval_begin, interval_end, file = f)
