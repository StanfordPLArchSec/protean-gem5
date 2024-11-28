#include "pin.H"
#include <iostream>
#include <fstream>

static unsigned long num_blocks = 0;
static unsigned long num_traces = 0;

void PIN_FAST_ANALYSIS_CALL Increment(unsigned long *p) {
  ++*p;
}

static void StaticTrace(TRACE trace, void *) {
  TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR) Increment,
                   IARG_FAST_ANALYSIS_CALL,
                   IARG_PTR, &num_traces,
                   IARG_END);
  for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
    BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR) Increment,
                   IARG_FAST_ANALYSIS_CALL,
                   IARG_PTR, &num_blocks,
                   IARG_END);
  }
}

static void Finish(int32_t code, void *) {
  std::ofstream log("pin.log");
  log << "blocks " << num_blocks << "\n"
      << "traces " << num_traces << "\n";
}

int main(int argc, char *argv[]) {
  if (PIN_Init(argc, argv))
    return 1;

  TRACE_AddInstrumentFunction(StaticTrace, nullptr);
  PIN_AddFiniFunction(Finish, nullptr);
  PIN_StartProgram();
}
