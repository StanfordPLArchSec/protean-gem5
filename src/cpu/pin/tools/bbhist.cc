#include "bbhist.hh"

#include <pin.H>
#include <vector>
#include <list>
#include <fstream>
#include <iostream>

#include "client.hh"

static KNOB<std::string> OutPath(KNOB_MODE_WRITEONCE, "pintool", "bbhist", "",
                                 "basic block edge histogram output path");
static std::ofstream out;

using Count = long;

struct Block {
    std::vector<ADDRINT> insts;
    Count count = 0;

    Block(BBL bbl)
    {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
            insts.push_back(INS_Address(ins));
    }
};

static std::list<Block> blocks;

static void
AnalyzeBBL(Count *count)
{
    *count += 1;
}

static void
InstrumentBBL(BBL bbl)
{
    blocks.emplace_back(bbl);
    BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR) AnalyzeBBL,
                   IARG_PTR, &blocks.back().count,
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
    for (const Block &block : blocks) {
        if (!block.count)
            continue;
        out << std::dec << block.count << " ";
        assert(!block.insts.empty());
        auto it = block.insts.begin();
        out << std::hex << *it++;
        for (; it != block.insts.end(); ++it)
            out << "," << *it;
        out << "\n";
    }
    out.close();
}

bool
bbhist_register()
{
    if (OutPath.Value().empty())
        return true;

    out.open(OutPath.Value());
    if (!out) {
        std::cerr << "bbhist: failed to open output file: " << OutPath.Value() << "\n";
        return false;
    }

    TRACE_AddInstrumentFunction(InstrumentTRACE, nullptr);
    PIN_AddFiniFunction(Finish, nullptr);

    return true;
}
