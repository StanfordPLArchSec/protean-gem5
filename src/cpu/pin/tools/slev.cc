// TODO: Don't dump the last interval, or at least extend it with some special kind
// of marker to indicate it's the last.

#include "slev.hh"

#include <fstream>
#include <iostream>
#include <list>
#include <cassert>
#include <pin.H>

#include "client.hh"

static KNOB<std::string> OutputFile(KNOB_MODE_WRITEONCE, "pintool", "slev", "", "Collect source location edge vectors");
static KNOB<unsigned long> IntervalSize(KNOB_MODE_WRITEONCE, "pintool", "slev-interval", "0", "SLEV interval size");
static KNOB<std::string> EdgeFile(KNOB_MODE_WRITEONCE, "pintool", "slev-edges", "", "SLEV path to edges");
static KNOB<std::string> MapFile(KNOB_MODE_WRITEONCE, "pintool", "slev-map", "", "SLEV: path to instruction-address-to-source-location map");

using Count = long;

static std::ofstream out;
static std::map<std::pair<std::string, std::string>, Count> edges;
static std::map<ADDRINT, std::string> map;
static long interval_size = 0;

static long prev_edges = 0;
static long total_edges = 0;
static long cur_insts = 0;
static long prev_insts = 0;
static long total_insts = 0;
static int num_intervals = 0;

struct Block {
    long id;
    long size;
    long hits;

    Block(long id, BBL bbl)
        : id(id), size(BBL_NumIns(bbl)), hits(0)
    {
    }

    void
    hit()
    {
        cur_insts += size;
        hits += size;
    }

    void
    reset()
    {
        hits = 0;
    }
};

static std::list<Block> blocks;

static const std::string *
getLoc(INS ins)
{
    const auto it = map.find(INS_Address(ins));
    if (it == map.end())
        return nullptr;
    return &it->second;
}

static bool
CheckStaticEdge(const std::string *src, const std::string *dst)
{
    if (!(src && dst))
        return false;
    const auto edge = std::make_pair(*src, *dst);
    return edges.count(std::make_pair(*src, *dst)) > 0;
}

static void
DumpInterval()
{
    out << "T";
    for (auto &block : blocks) {
        if (block.hits) {
            out << " :" << block.id << ":" << block.hits;
            block.reset();
        }
    }
    out << "\n# interval=" << num_intervals << " insts=" << prev_insts << "," << total_insts << " edges=" << prev_edges << "," << total_edges << "\n";

    // Update global counters.
    prev_edges = total_edges;
    prev_insts = total_insts;
    total_insts += cur_insts;
    cur_insts = 0;
    ++num_intervals;
}

static void
UpdateEdgeCount(uint64_t num_edges)
{
    total_edges += num_edges;
    if (cur_insts >= interval_size)
        DumpInterval();
}

static void
UpdateInstCount(Block *block)
{
    block->hit();
}

static void
InstrumentBBL(BBL bbl)
{
    const std::string *prev_loc = nullptr;
    long num_edges = 0;
    for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
        const std::string *loc = getLoc(ins);
        if (CheckStaticEdge(prev_loc, loc))
            ++num_edges;
        prev_loc = loc;
    }
    blocks.emplace_back(blocks.size(), bbl);
    Block *block = &blocks.back();
    BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR) UpdateInstCount,
                   IARG_PTR, block,
                   IARG_END);
    BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR) UpdateEdgeCount,
                   IARG_UINT64, (uint64_t) num_edges,
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
Finish(int32_t code, void *)
{
    DumpInterval();
    out.close();
}

bool
slev_register()
{
    if (OutputFile.Value().empty())
        return true;

    out.open(OutputFile.Value());
    if (!out) {
        std::cerr << "slev: failed to open output file\n";
        return false;
    }

    if (IntervalSize.Value() == 0) {
        std::cerr << "slev: -slev-interval: required\n";
        return false;
    }

    if (EdgeFile.Value().empty()) {
        std::cerr << "slev: -slev-edges: required\n";
        return false;
    }

    // Parse edge file.
    std::ifstream edge_is(EdgeFile.Value());
    if (!edge_is) {
        std::cerr << "slev: failed to open edge file\n";
        return false;
    }
    std::string loc1;
    std::string loc2;
    long count;
    while (edge_is >> loc1 >> loc2 >> count)
        edges[std::make_pair(loc1, loc2)] = count;
    edge_is.close();
    std::cerr << "slev: parsed " << edges.size() << " edges\n";

    // Parse map file.
    std::ifstream map_is(MapFile.Value());
    if (!map_is) {
        std::cerr << "slev: failed to open map file\n";
        return false;
    }
    ADDRINT addr;
    std::string loc;
    map_is >> std::hex;
    while (map_is >> addr >> loc)
        map[addr] = loc;
    map_is.close();
    std::cerr << "slev: parsed " << map.size() << " inst->locs\n";

    // Set interval size.
    interval_size = IntervalSize.Value();

    // Register callbacks.
    TRACE_AddInstrumentFunction(InstrumentTRACE, nullptr);
    PIN_AddFiniFunction(Finish, nullptr);

    return true;
}
