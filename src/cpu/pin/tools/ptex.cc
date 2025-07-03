#include <pin.H>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <map>
#include "plugin.hh"
#include "client.hh"
#include "xxhash.hh"

using Addr = ADDRINT;

namespace {

KNOB<bool> enable(KNOB_MODE_WRITEONCE, "pintool", "ptex", "1", "Enable PTeX fast-forwarding");
BUFFER_ID protstore_tracebuf_id;
std::unordered_map<REG, int> g_protregs;

constexpr uint8_t prot_prefix = 0x36;

bool
has_prot_prefix(INS ins)
{
    uint8_t prefix = prot_prefix;
    PIN_SafeCopy(&prefix, reinterpret_cast<const void *>(INS_Address(ins)), 1);
    return prefix == prot_prefix;
}

#if 0
void *
protstore_tracebuf_callback(BUFFER_ID tracebuf_id, THREADID tid, const CONTEXT *ctx, void *buf_raw, uint64_t num_elements, void *)
{
    Addr *buf_begin = static_cast<Addr *>(buf_raw);
    Addr *buf_end = buf_begin + num_elements;

    // Convert to vpages.
    std::transform(buf_begin, buf_end, buf_begin, [] (Addr addr) { return addr >> 12; });

    // Remove consecutive pages.
    buf_end = std::unique(buf_begin, buf_end);

    // Sort.
    std::sort(buf_begin, buf_end);

    // Unique again.
    buf_end = std::unique(buf_begin, buf_end);

    std::cerr << "PTeX: tracebuf (" << std::dec << (buf_end - buf_begin) << "):";
#if 0
    for (Addr *buf_it = buf_begin; buf_it != buf_end; ++buf_it)
        std::cerr << " " << std::hex << *buf_it;
#endif
    std::cerr << "\n";

    return buf_raw;
}
#else

bool prot_page_hashes[1 << 24]; // Zero-initialized.
std::vector<Addr> newly_protected_pages;
uint64_t num_prot_pages = 0;
// TODO: Collect a list of new pages and send them over to gem5.

uint32_t
page_to_index(uint64_t page) {
    page ^= page >> 33;
    page *= 0xff51afd7ed558ccdULL;
    page ^= page >> 33;
    page *= 0xc4ceb9fe1a85ec53ULL;
    page ^= page >> 33;
    return (uint32_t)(page & 0xFFFFFF);  // truncate to 24 bits
}

void
protstore_tracebuf_addr(Addr addr)
{
    const Addr page = addr >> 12;
    const uint32_t hash = page_to_index(page);
    bool &prot = prot_page_hashes[hash >> 8];
    if (prot)
        return;
    // Newly protected page.
    ++num_prot_pages;
    prot = true;
    newly_protected_pages.push_back(page << 12);
}

void *
protstore_tracebuf_callback(BUFFER_ID tracebuf_id, THREADID tid, const CONTEXT *ctx, void *buf_raw, uint64_t num_elements, void *)
{
    Addr *buf_begin = static_cast<Addr *>(buf_raw);
    Addr *buf_end = buf_begin + num_elements;
# if 0
    buf_end = std::unique(buf_begin, buf_end);
# endif
    std::for_each(buf_begin, buf_end, protstore_tracebuf_addr);
    return buf_raw;
}
#endif

int
protstore_check_reg_prot(int *prot)
{
    return *prot;
}

using ProtRegs = std::map<REG, bool>;

void
instrument_ins_store(INS ins, const ProtRegs &l_protregs)
{
    // Stores with PROT prefixes: always mark protected.
    if (has_prot_prefix(ins)) {
        for (uint32_t memop = 0; memop < INS_MemoryOperandCount(ins); ++memop) {
            if (INS_MemoryOperandIsWritten(ins, memop)) {
                INS_InsertFillBuffer(ins, IPOINT_BEFORE, protstore_tracebuf_id,
                                     IARG_MEMORYOP_EA, memop, 0,
                                     IARG_END);
                std::cerr << "PTeX: protstore: found prot-prefixed store at " << std::hex
                          << INS_Address(ins) << ": " << INS_Disassemble(ins) << "\n";
            }
        }
        return;
    }

    // For each (written memory operand, data register) pair.
    for (uint32_t memop = 0; memop < INS_MemoryOperandCount(ins); ++memop) {
        if (!INS_MemoryOperandIsWritten(ins, memop))
            continue;

        for (uint32_t op = 0; op < INS_OperandCount(ins); ++op) {
            if (!(INS_OperandIsReg(ins, op) && INS_OperandRead(ins, op)))
                continue;
            const REG reg = REG_FullRegName(INS_OperandReg(ins, op));
            if (reg == REG_RIP)
                continue;

            // Is this register always protected or always unprotected?
            const auto protreg = l_protregs.find(reg);
            const char *kind = nullptr;
            if (protreg == l_protregs.end()) {
                // Depends.
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) protstore_check_reg_prot,
                                 IARG_PTR, &g_protregs[reg],
                                 IARG_END);
                INS_InsertFillBufferThen(ins, IPOINT_BEFORE, protstore_tracebuf_id,
                                         IARG_MEMORYOP_EA, memop, 0,
                                         IARG_END);
                kind = "depends";
            } else if (protreg->second) {
                // Always protected.
                INS_InsertFillBuffer(ins, IPOINT_BEFORE, protstore_tracebuf_id,
                                     IARG_MEMORYOP_EA, memop, 0,
                                     IARG_END);
                kind = "always-prot";
            } else {
                kind = "always-unprot";
            }

            std::cerr << "PTeX: protstore: " << std::hex << INS_Address(ins) << ": "
                      << INS_Disassemble(ins) << ": memop=" << std::dec << memop
                      << " reg=" << REG_StringShort(reg) << " kind=" << kind << "\n";
        }
    }
}

void
instrument_ins_regw(INS ins, ProtRegs &l_protregs)
{
    // Is this instruction PROT-prefixed?
    const bool prot = has_prot_prefix(ins);

    for (uint32_t op = 0; op < INS_OperandCount(ins); ++op) {
        if (!(INS_OperandIsReg(ins, op) && INS_OperandWritten(ins, op)))
            continue;
        const REG reg_orig = INS_OperandReg(ins, op);
        const REG reg = REG_FullRegName(reg_orig);
        if (reg == REG_RIP || reg == REG_RFLAGS)
            continue;
        if (prot) {
            std::cerr << "PTeX: protregs: " << std::hex << INS_Address(ins) << ": "
                      << INS_Disassemble(ins) << ": protect reg "
                      << REG_StringShort(reg) << "\n";
            l_protregs[reg] = true;
        } else if (REG_is_gr8(reg) || REG_is_gr16(reg)) {
            // Ignore these, since we have partial overwrites.
        } else {
            std::cerr << "PTeX: protregs: " << std::hex << INS_Address(ins) << ": "
                      << INS_Disassemble(ins) << ": unprotect reg "
                      << REG_StringShort(reg) << "\n";
            l_protregs[reg] = false;
        }
    }
}

void
instrument_ins(INS ins, ProtRegs &l_protregs)
{
    instrument_ins_store(ins, l_protregs);
    instrument_ins_regw(ins, l_protregs);
}

void
do_prot(int *x)
{
    *x = 1;
}

void
do_unprot(int *x)
{
    *x = 0;
}

void
instrument_bbl(BBL bbl)
{
    ProtRegs l_protregs;
    for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        instrument_ins(ins, l_protregs);

    // Batch-update the protregs.
    for (const auto &[reg, prot] : l_protregs) {
        if (prot) {
            INS_InsertCall(BBL_InsTail(bbl), IPOINT_BEFORE, (AFUNPTR) do_prot,
                           IARG_PTR, &g_protregs[reg],
                           IARG_END);
        } else {
            INS_InsertCall(BBL_InsTail(bbl), IPOINT_BEFORE, (AFUNPTR) do_unprot,
                           IARG_PTR, &g_protregs[reg],
                           IARG_END);
        }
    }
}


void
instrument_trace(TRACE trace, void *)
{
    if (IsKernelCode(trace))
        return;
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
        instrument_bbl(bbl);
}

void
finish(int32_t code, void *)
{
    std::cerr << "ptex: protected_pages=" << std::dec << num_prot_pages << "\n";
}

struct PTeXPlugin final : Plugin
{
    const char *name() const override { return "ptex"; }

    int priority() const override { return -10; }

    bool enabled() const override { return enable.Value(); }

    bool
    reg() override
    {
        TRACE_AddInstrumentFunction(instrument_trace, nullptr);
        protstore_tracebuf_id = PIN_DefineTraceBuffer(8, 4096 * 8, protstore_tracebuf_callback, nullptr); // ~128 MiB buffer.
        PIN_AddFiniFunction(finish, nullptr);
        return true;
    }

    std::string
    getState() const override
    {
        std::ostringstream ss;
        for (Addr page : newly_protected_pages)
            ss << std::hex << page << "\n";
        newly_protected_pages.clear();
        return ss.str();
    }
} plugin;

}
