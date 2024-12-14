#include "instlist.hh"

#include <pin.H>
#include <fstream>
#include <iostream>
#include <unordered_set>

#include "client.hh"

static KNOB<std::string> OutPath(KNOB_MODE_WRITEONCE, "pintool", "instlist", "",
                                 "list unique instruction addresses executed");
static std::ofstream out;
static std::unordered_set<ADDRINT> insts;

static void
InstrumentINS(INS ins, void *)
{
    if (IsKernelCode(ins))
        return;

    insts.insert(INS_Address(ins));
}

static void
Finish(int32_t code, void *)
{
    for (ADDRINT inst : insts)
        out << std::hex << inst << "\n";
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

    INS_AddInstrumentFunction(InstrumentINS, nullptr);
    PIN_AddFiniFunction(Finish, nullptr);

    return true;
}
