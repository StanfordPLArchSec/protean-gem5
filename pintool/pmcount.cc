#include "pmcount.hh"

#include <pin.H>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <cstdint>
#include <fstream>
#include <vector>

#include "client.hh"

static KNOB<bool> EnablePMCount(KNOB_MODE_WRITEONCE, "pintool", "pmcount", "0", "enable progress marker count tool");
static KNOB<std::string> OutFile(KNOB_MODE_WRITEONCE, "pintool", "pmcount-out", "", "[out] progress marker output");
static KNOB<std::string> InFile(KNOB_MODE_WRITEONCE, "pintool", "pmcount-in", "", "[in] progress marker input");

using BlockCounts = std::unordered_map<ADDRINT, std::pair<std::vector<ADDRINT>, uint64_t>>;
static BlockCounts block_counts;
static std::unordered_map<ADDRINT, std::pair<std::string, uint64_t>> inst_counts;
static std::ofstream out;

static void PIN_FAST_ANALYSIS_CALL AnalyzeBBL(uint64_t &count) {
    ++count;
}

static void
FlushBlock(BlockCounts::iterator it)
{
    const auto& [inst_addrs, n] = it->second;
    for (const ADDRINT& inst_addr : inst_addrs)
        inst_counts[inst_addr].second += n;
    block_counts.erase(it);
}

static ADDRINT
DynamicToStaticAddress(ADDRINT addr)
{
    return addr;
}

static uint64_t *
GetBlockEntry(BBL bbl)
{
    assert(BBL_Original(bbl));
    const ADDRINT bbl_addr = BBL_Address(bbl);
    const auto block_count_it = block_counts.find(bbl_addr);
    if (block_count_it != block_counts.end())
        return &block_count_it->second.second;

    std::vector<uint64_t> inst_addrs;
    for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
        ADDRINT inst_addr = DynamicToStaticAddress(INS_Address(ins));
        if (inst_addr && inst_counts.count(inst_addr)) {
            inst_addrs.push_back(inst_addr);
        }
    }

    if (inst_addrs.empty())
        return nullptr;

    std::cerr << "Block: " << std::hex << BBL_Address(bbl) << "\n";

    auto& p = block_counts[bbl_addr];
    p.first = std::move(inst_addrs);
    return &p.second;
}

static void
InstrumentTRACE(TRACE trace, void *)
{
    if (IsKernelCode(trace))
        return;
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        if (uint64_t *block_count_ptr = GetBlockEntry(bbl)) {
            BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR) AnalyzeBBL,
                           IARG_FAST_ANALYSIS_CALL,
                           IARG_PTR, block_count_ptr,
                           IARG_END);
        }
    }
}


static void
Finish(int32_t code, void *)
{
    for (const auto& [_, p] : block_counts) {
        for (const ADDRINT inst_addr : p.first) {
            const auto inst_count_it = inst_counts.find(inst_addr);
            assert(inst_count_it != inst_counts.end());
            inst_count_it->second.second += p.second;
        }
    }

    for (const auto &[addr, p] : inst_counts)
        if (p.second > 0)
            out << "0x" << std::hex << addr << " " << std::dec << p.second << " " << p.first << "\n";
    out.close();
}

bool
pmcount_register()
{
    if (!EnablePMCount.Value())
        return true;

    if (InFile.Value().empty())
        return false;

    std::ifstream in(InFile.Value());
    ADDRINT addr;
    std::string id;
    // TOOD: Log for each submodule.
    while (in >> std::hex >> addr >> id) {
        inst_counts[addr].first = id;
    }

    if (OutFile.Value().empty())
        return false;

    out.open(OutFile.Value());

    TRACE_AddInstrumentFunction(InstrumentTRACE, nullptr);
    PIN_AddFiniFunction(Finish, nullptr);

    return true;
}
