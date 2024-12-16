#include "instlist.hh"

#include <pin.H>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "client.hh"

static KNOB<std::string> OutPath(KNOB_MODE_WRITEONCE, "pintool", "instlist", "",
                                 "list unique instruction addresses executed");
static std::ofstream out;
static std::unordered_map<ADDRINT, std::vector<std::vector<ADDRINT>>> blocks;

static void
InstrumentBBL(BBL bbl)
{
    std::vector<ADDRINT> &insts = blocks[BBL_Address(bbl)].emplace_back();
    for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        insts.push_back(INS_Address(ins));
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
    out << std::hex;
    for (const auto &[block, insts] : blocks) {
        out << block;
        for (const auto &insts2 : insts) {
            for (ADDRINT inst : insts2)
                out << " " << inst;
            out << " ::";
        }
        out << "\n";
    }
    out.close();
}

bool instlist_register()
{
    if (OutPath.Value().empty())
        return true;

    out.open(OutPath.Value());
    if (!out) {
        std::cerr << "instlist: failed to open output file: " << OutPath.Value() << "\n";
        return false;
    }

    TRACE_AddInstrumentFunction(InstrumentTRACE, nullptr);
    PIN_AddFiniFunction(Finish, nullptr);

    return true;
}
