#include "bbe.hh"

#include <pin.H>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>

#include "client.hh"

static KNOB<std::string> OutPath(KNOB_MODE_WRITEONCE, "pintool", "bbe", "", "basic block edge histogram output path");
static std::ofstream out;

using BlockInsts = std::vector<ADDRINT>;
using BlockId = size_t;
using Count = long;
static std::vector<BlockInsts> blocks;
static std::map<BlockInsts, BlockId> ids;
static std::vector<std::vector<Count>> edges; // Basic block edge matrix.
static BlockId src = 0;

static void
ResizeEdgeMatrix(BlockId n)
{
    assert(edges.size() < n);
    edges.resize(n);
    for (std::vector<Count> &edge : edges) {
        assert(edge.size() < n);
        edge.resize(n);
    }
}

static void PIN_FAST_ANALYSIS_CALL
AnalyzeBBL(ADDRINT dst)
{
    edges[src][dst] += 1;
    src = dst;
}

static void
InstrumentBBL(BBL bbl)
{
    BlockInsts insts;
    for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        insts.push_back(INS_Address(ins));
    const auto res = ids.emplace(insts, ids.size());
    const auto it = res.first;
    const BlockId id = it->second;
    if (res.second) {
        assert(blocks.size() == id);
        blocks.emplace_back(std::move(insts));
        ResizeEdgeMatrix(blocks.size());
    }

    BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR) AnalyzeBBL,
                   IARG_FAST_ANALYSIS_CALL,
                   IARG_ADDRINT, (ADDRINT) id,
                   IARG_END);
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
print_block_insts(std::ostream &os, const BlockInsts &insts, const std::string &sep)
{
    assert(!insts.empty());
    auto it = insts.begin();
    os << *it++;
    for (; it != insts.end(); ++it)
        os << sep << *it;
}

static void
Finish(int32_t code, void *)
{
    out << std::hex;
    const size_t n = edges.size();
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (const Count count = edges[i][j]) {
                out << std::hex;
                print_block_insts(out, blocks[i], ",");
                out << " ";
                print_block_insts(out, blocks[j], ",");
                out << " " << std::dec << count << "\n";
            }
        }
    }
    out.close();
}

bool
bbe_register()
{
    if (OutPath.Value().empty())
        return true;

    out.open(OutPath.Value());
    if (!out) {
        std::cerr << "bbe: failed to open output file: " << OutPath.Value() << "\n";
        return false;
    }

    TRACE_AddInstrumentFunction(InstrumentTRACE, nullptr);
    PIN_AddFiniFunction(Finish, nullptr);

    return true;
}
