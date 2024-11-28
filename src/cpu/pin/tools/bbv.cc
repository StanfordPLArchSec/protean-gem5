#include "bbv.hh"

#include <fstream>
#include <iostream>
#include <map>
#include <pin.H>

#include "client.hh"

static KNOB<bool> EnableBBV(KNOB_MODE_WRITEONCE, "pintool", "bbv", "0", "enable BBV tracing");
static KNOB<std::string> OutputFile(KNOB_MODE_WRITEONCE, "pintool", "bbv-out", "", "specify output BBV file");
static KNOB<unsigned long> IntervalSize(KNOB_MODE_WRITEONCE, "pintool", "bbv-interval", "0", "specify interval size");

struct Block {
    unsigned long id;
    unsigned long size;
    unsigned long hits;
    std::string disasm;

    Block(unsigned long id, BBL bbl): id(id), size(BBL_NumIns(bbl)), hits(0) {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            disasm += "\t" + INS_Disassemble(ins) + "\n";
        }
    
        assert(id > 0);
        reset();
    }
  
    void reset() {
        hits = 0;
    }
};

static std::ofstream out;
static unsigned long interval_size;
static unsigned long inst_count = 0;
static unsigned long func_count_begin = 0;
static unsigned long func_count_end = 0;
static unsigned long next_block_id = 0;
static std::map<ADDRINT, Block> blocks;
static unsigned long num_intervals = 0;
static unsigned long total_insts = 0;

static unsigned long get_total_insts() {
    return inst_count + total_insts;
}

static void DumpInterval() {
    out << "T";
    for (auto& [_, block] : blocks) {
        if (block.hits != 0)
            out << " :" << block.id << ":" << (block.hits * block.size);
        block.reset();
    }
    out << std::endl;
    out << "# func-range " << num_intervals << " " << func_count_begin << " " << func_count_end << " " << get_total_insts() << " " << std::endl;
    func_count_begin = func_count_end;
    total_insts += inst_count;
    assert(inst_count >= interval_size);
    inst_count -= interval_size;
    ++num_intervals;
}

static void
HandleBlock(unsigned long& hits, uint32_t size)
{
    ++hits;
    inst_count += size;
    if (inst_count >= interval_size) {
        // Interval reached.
        DumpInterval();
    }
}

static void
DynamicCall()
{
    ++func_count_end;
}

// TODO: Should use PIN_CreateAt instead.
static void
StaticCall(INS ins, void *)
{
    if (GetSymbol(INS_Address(ins)))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) DynamicCall, IARG_END);
}

static void Trace(TRACE trace, void *) {
    if (IsKernelCode(trace))
        return;
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        assert(BBL_Original(bbl));
        const ADDRINT addr = BBL_Address(bbl);

        const auto it = blocks.emplace(addr, Block(++next_block_id, bbl)).first;
        Block &block = it->second;
        if (block.size != BBL_NumIns(bbl)) {
            log() << "BBV: warning: block size for instruction address 0x" << std::hex
                  << addr << " changed from " << std::dec << block.size << " to " << BBL_NumIns(bbl)
                  << std::endl;
            block.size = BBL_NumIns(bbl);
        }
        assert(block.size == BBL_NumIns(bbl));

        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR) HandleBlock,
                       IARG_PTR, &block.hits,
                       IARG_UINT32, block.size,
                       IARG_END);
    }
}

static void Fini(int32_t code, void *) {
    out << "# func-total " << func_count_end << std::endl;
    out << "# total-insts " << get_total_insts() << std::endl;
    out.close();

    std::ofstream call_count_os("bbv.callcount.txt");
    call_count_os << func_count_end << std::endl;
}

bool 
bbv_register()
{
    if (!EnableBBV.Value())
        return true;

    if (OutputFile.Value().empty()) {
        std::cerr << "pinpoints: -o: required\n";
        return false;
    }
    out.open(OutputFile.Value());

    if (IntervalSize.Value() == 0) {
        std::cerr << "pinpoints: -interval: required\n";
        return false;
    }
    interval_size = IntervalSize.Value();

    INS_AddInstrumentFunction(StaticCall, nullptr);
    TRACE_AddInstrumentFunction(Trace, nullptr);
    PIN_AddFiniFunction(Fini, nullptr);

    return true;
}
