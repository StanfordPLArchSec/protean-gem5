#include <fstream>
#include <iostream>
#include <map>
#include "pin.H"

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

static void HandleBlock(Block *block) {
    ++block->hits;
    inst_count += block->size;
    if (inst_count >= interval_size) {
        // Interval reached.
        DumpInterval();
    }
}

static void DynamicRoutine() {
    ++func_count_end;
}

static void StaticRoutine(RTN rtn, void *) {
    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR) DynamicRoutine, IARG_END);
    RTN_Close(rtn);
}

static void Trace(TRACE trace, void *) {
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        assert(BBL_Original(bbl));
        const ADDRINT addr = BBL_Address(bbl);

        const auto it = blocks.emplace(addr, Block(++next_block_id, bbl)).first;
        const Block &block = it->second;
        assert(block.size == BBL_NumIns(bbl));

        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR) HandleBlock, IARG_ADDRINT, &block, IARG_END);
    }
}

static void Fini(int32_t code, void *) {
    out << "# func-total " << func_count_end << std::endl;
    out << "# total-insts " << get_total_insts() << std::endl;
    out.close();
}

bool 
bbv_register()
{
    if (!EnableBBV.Value())
        return true;

    PIN_InitSymbols();

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

    RTN_AddInstrumentFunction(StaticRoutine, nullptr);
    TRACE_AddInstrumentFunction(Trace, nullptr);
    PIN_AddFiniFunction(Fini, nullptr);

    return true;
}
