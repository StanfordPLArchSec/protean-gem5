#include "itrace.hh"

#include <pin.H>
#include <fstream>
#include <iosteam>

#include "client.hh"

static KNOB<bool> EnableInstTrace(KNOB_MODE_WRITEONCE, "pintool", "itrace", "0", "enable instruction trace");
static KNOB<bool> OutputFile(KNOB_MODE_WRITEONCE, "pintool", "itrace-out", "", "specify instruction trace output file");
static std::ofstream trace;

bool
itrace_register()
{
    if (!EnableInstTrace.Value())
        return true;

    if (OutputFile.Value().empty()) {
        std::cerr << prog << ": -itrace-out: required\n";
        return false;
    }
    trace.open(OutputFile.Value());

    
    
    
}
