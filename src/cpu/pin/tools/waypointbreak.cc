#include <optional>
#include <iostream>
#include <pin.H>

#include "waypointcount.hh"
#include "ops.hh"
#include "client.hh"
#include "plugin.hh"

// TODO: Factor out common code with instbreak.

static std::optional<ADDRINT> waypointbreak;


static void
SetBreakpoint(ADDRINT new_waypointbreak)
{
    waypointbreak = new_waypointbreak;
    PIN_RemoveInstrumentation();
}

static void
ClearBreakpoint()
{
    waypointbreak = std::nullopt;
    PIN_RemoveInstrumentation();
}

static ADDRINT
AnalyzeIf(ADDRINT waypointbreak)
{
    return waypointbreak <= waypointcount;
}

static void
AnalyzeThen(CONTEXT *ctx)
{
    ClearBreakpoint();
    RunResult result;
    result.result = result.RUNRESULT_BREAK;
    std::cerr << "waypointbreak: switching to kernel\n";
    ContextSwitchToKernel(ctx, result);
    PIN_ExecuteAt(ctx);
}

static void
Instrument(TRACE trace, void *)
{
    if (IsKernelCode(trace) || !waypointbreak)
        return;
    TRACE_InsertIfCall(trace, IPOINT_BEFORE, (AFUNPTR) AnalyzeIf,
                       IARG_ADDRINT, *waypointbreak,
                       IARG_END);
    TRACE_InsertThenCall(trace, IPOINT_BEFORE, (AFUNPTR) AnalyzeThen,
                         IARG_CONTEXT,
                         IARG_END);
}

namespace {
struct WaypointBreakPlugin final : Plugin
{
    bool enabled() const override { return true; }

    bool
    reg() override
    {
        TRACE_AddInstrumentFunction(Instrument, nullptr);
        return true;
    }

    bool
    command(const std::string &cmd, const std::vector<std::string> &args, std::string &result) override
    {
        if (cmd == "waypointbreak") {
            SetBreakpoint(std::stoull(args.at(0)));
            return true;
        }
        return false;
    }
} plugin;
}
