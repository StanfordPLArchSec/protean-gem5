#include <vector>
#include <map>
#include <cassert>
#include <iostream>
#include <sstream>
#include <pin.H>

#include "plugin.hh"
#include "client.hh"

namespace {

KNOB<bool> enable(KNOB_MODE_WRITEONCE, "pintool", "bbhist", "0", "Enable basic block histogram collection");

struct Block {
    std::vector<ADDRINT> insts;
    ADDRINT count = 0;

    Block(BBL bbl)
    {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
            insts.push_back(INS_Address(ins));
    }
};

std::vector<ADDRINT>
getInstVec(BBL bbl)
{
    std::vector<ADDRINT> insts;
    for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        insts.push_back(INS_Address(ins));
    return insts;
}

std::map<std::vector<ADDRINT>, ADDRINT> blocks;

void
Analyze(ADDRINT *counter)
{
    ++*counter;
}

void
InstrumentBBL(BBL bbl)
{
    ADDRINT &counter = blocks[getInstVec(bbl)];
    BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR) Analyze,
                   IARG_PTR, &counter,
                   IARG_END);
}

void
InstrumentTRACE(TRACE trace, void *)
{
    if (IsKernelCode(trace))
        return;
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
        InstrumentBBL(bbl);
}

void
Dump(std::ostream &os)
{
    for (const auto &[insts, count] : blocks) {
        if (count == 0)
            continue;
        os << std::dec << count << " ";
        auto it = insts.begin();
        assert(it != insts.end());
        os << std::hex << *it++;
        while (it != insts.end())
            os << "," << *it++;
        os << "\n";
    }
}

void
Reset()
{
    blocks.clear();
}

struct BasicBlockHistogramPlugin final : Plugin
{
    bool
    enabled() const override
    {
        return enable.Value();
    }

    bool
    reg() override
    {
        TRACE_AddInstrumentFunction(InstrumentTRACE, nullptr);
        return true;
    }

    bool
    command(const std::string &cmd, const std::vector<std::string> &args, std::string &result) override
    {
        if (cmd != "bbhist")
            return false;

        if (args.at(0) == "dump") {
            std::stringstream ss;
            Dump(ss);
            result = ss.str();
            return true;
        } else if (args.at(0) == "reset") {
            Reset();
            return true;
        }

        std::cerr << "bbhist: error: bad usage\n";
        std::cerr << "usage: bbhist (dump|reset)\n";
        std::abort();
    }
} plugin;

}
