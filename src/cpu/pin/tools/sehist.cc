#include "sehist.hh"

#include <pin.H>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>

#include "client.hh"

static KNOB<std::string> InPath(KNOB_MODE_WRITEONCE, "pintool", "sehist-in", "", "source edge histogram input -- map from instructions to source locations");
static KNOB<std::string> OutPath(KNOB_MODE_WRITEONCE, "pintool", "sehist-out", "", "source edge histogram output -- histogram of source location edge hit counts");

struct SrcLoc {
  std::string name;
  int id;
  std::vector<long> dsts;
};

static std::vector<SrcLoc> srclocs;
static std::unordered_map<ADDRINT, int> inst_to_src;
static long *edgevec;

static void
IncDstEdge(ADDRINT dst_id) {
  ++edgevec[dst_id];
}

static void
SetEdgeVec(long *new_edgevec) {
  edgevec = new_edgevec;
}

static SrcLoc *
getSrcLoc(INS ins)
{
    const auto it = inst_to_src.find(INS_Address(ins));
    if (it == inst_to_src.end())
        return nullptr;
    return &srclocs.at(it->second);
}

static void
InstrumentBBL(BBL bbl)
{
    SrcLoc *prev_loc = nullptr;
    for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
        if (SrcLoc *loc = getSrcLoc(ins); loc && loc != prev_loc) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) IncDstEdge,
                           IARG_ADDRINT, (long) loc->id,
                           IARG_END);
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) SetEdgeVec,
                           IARG_PTR, loc->dsts.data(),
                           IARG_END);
            prev_loc = loc;
            break; // REVERTME
        }
    }
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
Finish(int32_t code, void *) {
  std::ofstream out(OutPath.Value());
  if (!out) {
    std::cerr << "sehist: failed to open output file\n";
    std::abort();
  }
  for (const SrcLoc& src : srclocs)
    for (int dst_idx = 0; dst_idx < srclocs.size(); ++dst_idx)
      if (const long count = src.dsts[dst_idx])
        out << src.name << " " << srclocs[dst_idx].name << " " << count << "\n";
}

bool sehist_register() {
  if (InPath.Value().empty() && OutPath.Value().empty())
    return true;

  if (InPath.Value().empty() || OutPath.Value().empty()) {
    std::cerr << "sehist: must specify both sehist-in and sehist-out\n";
    return false;
  }

  // Parse the input file, which is in the following format:
  // <addr> <srcloc>
  // <addr> <srcloc>
  // ...
  std::ifstream in(InPath.Value());
  if (!in) {
    std::cerr << "sehist: failed to open input file\n";
    return false;
  }

  SrcLoc &dummy = srclocs.emplace_back();
  dummy.name = "<start>:0:0";
  dummy.id = 0;

  ADDRINT addr;
  std::string name;
  while (in >> std::hex >> addr >> name) {
    SrcLoc &srcloc = srclocs.emplace_back();
    srcloc.name = std::move(name);
    srcloc.id = srclocs.size() - 1;
    inst_to_src[addr] = srcloc.id;
  }

  // Populate srcloc edge vectors.
  for (SrcLoc &srcloc : srclocs)
    srcloc.dsts.resize(srclocs.size());

  edgevec = srclocs[0].dsts.data();

  std::cerr << "sehist: parsed " << (srclocs.size() - 1) << " srclocs\n";

  TRACE_AddInstrumentFunction(InstrumentTRACE, nullptr);
  PIN_AddFiniFunction(Finish, nullptr);

  return true;
}
