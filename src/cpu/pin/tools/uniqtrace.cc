#include "uniqtrace.hh"

#include <pin.H>
#include <fstream>
#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "client.hh"

static KNOB<std::string> OutPath(KNOB_MODE_WRITEONCE, "pintool", "qtrace", "",
                                 "emit unique instruction trace to this path "
                                 "(if empty, disables unique instruction tracing");
static std::ofstream out;

static std::list<ADDRINT> inst_trace;
static std::unordered_set<ADDRINT> inst_seen;

struct Block
{
    std::vector<ADDRINT> insts;
    unsigned long hits = 0;

    Block(BBL bbl)
    {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
            insts.push_back(INS_Address(ins));
    }
};

static std::unordered_map<ADDRINT, Block> blocks;

static void
InstrumentINS(INS ins, void *)
{
    // TODO: should have transparent wrapper for this.
    if (IsKernelCode(ins))
        return;
    const ADDRINT addr = INS_Address(ins);
    if (inst_seen.insert(addr).second)
        inst_trace.push_back(addr);
}

static void PIN_FAST_ANALYSIS_CALL
AnalyzeBBL(unsigned long& hits)
{
    ++hits;
}

static void
InstrumentBBL(BBL bbl)
{
    assert(BBL_Original(bbl));
    Block& block = blocks.emplace(std::piecewise_construct,
                                  std::make_tuple(BBL_Address(bbl)),
                                  std::make_tuple(bbl)).first->second;
    BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR) AnalyzeBBL,
                   IARG_FAST_ANALYSIS_CALL,
                   IARG_PTR, &block.hits,
                   IARG_END);
}

static void
InstrumentTRACE(TRACE trace, void *)
{
    if (IsKernelCode(trace))
        return;
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
        InstrumentBBL(bbl);
}

static void
Finish(int32_t code, void *)
{
    std::unordered_map<ADDRINT, unsigned long> inst_hits;
    for (const auto& [_, block] : blocks)
        for (ADDRINT inst : block.insts)
            inst_hits[inst] += block.hits;
    for (ADDRINT inst : inst_trace)
        out << "0x" << std::hex << inst << " " << std::dec << inst_hits.at(inst) << "\n";
    out.close();
}


bool
qtrace_register()
{
    if (OutPath.Value().empty())
        return true;

    out.open(OutPath.Value());
    if (!out) {
        std::cerr << "qtrace: failed to open output file: " << OutPath.Value() << "\n";
        return false;
    }

    INS_AddInstrumentFunction(InstrumentINS, nullptr);
    TRACE_AddInstrumentFunction(InstrumentTRACE, nullptr);
    PIN_AddFiniFunction(Finish, nullptr);

    return true;
}
