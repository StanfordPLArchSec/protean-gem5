#include <limits>
#include <iostream>
#include <pin.H>
#include <unistd.h>

#include "plugin.hh"
#include "instcount.hh"
#include "client.hh"

static KNOB<bool> enable(KNOB_MODE_WRITEONCE, "pintool", "instbreak", "0", "Enable instruction breakpointing");

static ADDRINT instbreak = std::numeric_limits<ADDRINT>::max();

static ADDRINT
AnalyzeIf(ADDRINT instbreak)
{
    return instbreak <= instcount;
}

static void
AnalyzeThen(CONTEXT *ctx)
{
    RunResult result;
    result.result = result.RUNRESULT_BREAK;
    std::cerr << "instbreak: switching to kernel\n";
    ContextSwitchToKernel(ctx, result);
    PIN_ExecuteAt(ctx);
}

static void
Instrument(TRACE trace, void *)
{
    if (IsKernelCode(trace))
        return;
    TRACE_InsertIfCall(trace, IPOINT_BEFORE, (AFUNPTR) AnalyzeIf,
                       IARG_ADDRINT, instbreak,
                       IARG_END);
    TRACE_InsertThenCall(trace, IPOINT_BEFORE, (AFUNPTR) AnalyzeThen,
                         IARG_CONTEXT,
                         IARG_END);
}

namespace {
struct InstBreakPlugin : Plugin
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
        if (cmd == "instbreak") {
            instbreak = std::stoull(args.at(0));
            PIN_RemoveInstrumentation(); // Need to reinstrument Instrument function.
            return true;
        }
        return false;
    }
} plugin;
}
