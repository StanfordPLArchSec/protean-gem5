#include <iostream>
#include <optional>
#include <pin.H>

#include "plugin.hh"
#include "instcount.hh"
#include "client.hh"

// FIXME: Don't need this, since it's zero overhead if no breakpoint has been set!
static KNOB<bool> enable(KNOB_MODE_WRITEONCE, "pintool", "instbreak", "0", "Enable instruction breakpointing");

static std::optional<ADDRINT> instbreak;

static void
SetBreakpoint(ADDRINT new_instbreak)
{
    instbreak = new_instbreak;
    PIN_RemoveInstrumentation();
}

static void
ClearBreakpoint()
{
    instbreak = std::nullopt;
    PIN_RemoveInstrumentation();
}

static ADDRINT
AnalyzeIf(ADDRINT instbreak, ADDRINT &instcount)
{
    return instbreak <= instcount;
}

static void
AnalyzeThen(CONTEXT *ctx)
{
    ClearBreakpoint();
    RunResult result;
    result.result = result.RUNRESULT_BREAK;
    std::cerr << "instbreak: switching to kernel\n";
    ContextSwitchToKernel(ctx, result);
    PIN_ExecuteAt(ctx);
}

static void
Instrument(TRACE trace, void *)
{
    if (IsKernelCode(trace) || !instbreak)
        return;
    TRACE_InsertIfCall(trace, IPOINT_BEFORE, (AFUNPTR) AnalyzeIf,
                       IARG_ADDRINT, *instbreak,
                       IARG_PTR, &instcount,
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
        TRACE_AddInstrumentFunction(Instrument, nullptr);
        return true;
    }

    bool
    command(const std::string &cmd, const std::vector<std::string> &args, std::string &result) override
    {
        if (cmd == "instbreak") {
            SetBreakpoint(std::stoull(args.at(0)));
            return true;
        }
        return false;
    }
} plugin;
}
