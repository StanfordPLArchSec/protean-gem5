#include "instcount.hh"

#include <pin.H>
#include <string>
#include "client.hh"
#include "plugin.hh"

static KNOB<bool> enable(KNOB_MODE_WRITEONCE, "pintool", "instcount", "0", "Enable instruction counting");
ADDRINT instcount;

static void
Analyze(ADDRINT n)
{
    instcount += n;
}

static void
Instrument(TRACE trace, void *)
{
    if (IsKernelCode(trace))
        return;
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR) Analyze,
                       IARG_ADDRINT, BBL_NumIns(bbl),
                       IARG_END);
    }
}

namespace {
struct InstCountPlugin : Plugin
{
    bool
    enabled() const override
    {
        return enable.Value();
    }
    
    bool
    reg() override
    {
        assert(enabled());
        TRACE_AddInstrumentFunction(Instrument, nullptr);
        return true;
    }

    bool
    command(const std::string &cmd, const std::vector<std::string> &args, std::string &result) override
    {
        if (cmd == "instcount") {
            result = std::to_string(instcount);
            return true;
        }

        return false;
    }
} plugin;
}
