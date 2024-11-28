#include "fhist.hh"

#include <pin.H>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <fstream>

#include "client.hh"

static KNOB<bool> EnableFunctionHist(KNOB_MODE_WRITEONCE, "pintool", "fhist", "0", "Enable Function Histogram");
static KNOB<std::string> OutputFile(KNOB_MODE_WRITEONCE, "pintool", "fhist-out", "fhist.txt", "Function Histogram output path");

static std::unordered_map<std::string, uint64_t> fhist;

static void
DynamicFunctionEntrypoint(uint64_t *count)
{
    ++*count;
}

static void
StaticFunctionEntrypoint(INS ins, void *)
{
    if (const std::string *name = GetSymbol(INS_Address(ins))) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) DynamicFunctionEntrypoint,
                       IARG_PTR, &fhist[*name],
                       IARG_END);
    }
}

static void
Finish(int32_t code, void *)
{
    std::ofstream os(OutputFile.Value());
    for (const auto &[name, count] : fhist) {
        os << name << " " << count << "\n";
    }
}

bool
fhist_register()
{
    if (!EnableFunctionHist.Value())
        return true;
    INS_AddInstrumentFunction(StaticFunctionEntrypoint, nullptr);
    PIN_AddFiniFunction(Finish, nullptr);
    return true;
}
