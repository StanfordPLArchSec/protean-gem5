#include <pin.H>
#include <iostream>
#include <unordered_map>
#include "plugin.hh"
#include "client.hh"

using Addr = ADDRINT;

namespace {

KNOB<bool> enable(KNOB_MODE_WRITEONCE, "pintool", "ptex", "0", "Enable PTeX fast-forwarding");
BUFFER_ID protstore_tracebuf_id;
std::unordered_map<REG, int> protregs;

constexpr uint8_t prot_prefix = 0x36;

bool
has_prot_prefix(INS ins)
{
    uint8_t prefix = prot_prefix;
    PIN_SafeCopy(&prefix, reinterpret_cast<const void *>(INS_Address(ins)), 1);
    return prefix == prot_prefix;
}

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

int
protstore_check_reg_prot(int *prot)
{
    return *prot;
}

void
protstore_instrument(INS ins, void *)
{
    if (IsKernelCode(ins))
        return;

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
            std::cerr << "PTeX: protstore: " << std::hex << INS_Address(ins) << ": "
                      << INS_Disassemble(ins) << ": memop=" << std::dec << memop
                      << " reg=" << REG_StringShort(reg) << "\n";
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) protstore_check_reg_prot,
                             IARG_PTR, &protregs[reg],
                             IARG_END);
            INS_InsertFillBufferThen(ins, IPOINT_BEFORE, protstore_tracebuf_id,
                                     IARG_MEMORYOP_EA, memop, 0,
                                     IARG_END);
        }
    }
}

void
protregs_protreg(int *x)
{
    *x = 1;
}

void
protregs_unprotreg(int *x)
{
    *x = 0;
}

void
protregs_instrument(INS ins, void *)
{
    if (IsKernelCode(ins))
        return;

    // Is this instruction PROT-prefixed?
    const bool prot = has_prot_prefix(ins);

    for (uint32_t op = 0; op < INS_OperandCount(ins); ++op) {
        if (!(INS_OperandIsReg(ins, op) && INS_OperandWritten(ins, op)))
            continue;
        const REG reg_orig = INS_OperandReg(ins, op);
        const REG reg = REG_FullRegName(reg_orig);
        if (reg == REG_RIP)
            continue;
        if (prot) {
            std::cerr << "PTeX: protregs: " << std::hex << INS_Address(ins) << ": "
                      << INS_Disassemble(ins) << ": protect reg "
                      << REG_StringShort(reg) << "\n";
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) protregs_protreg,
                           IARG_PTR, &protregs[reg],
                           IARG_END);
        } else if (REG_is_gr8(reg) || REG_is_gr16(reg)) {
            // Ignore these, since we have partial overwrites.
        } else {
            std::cerr << "PTeX: protregs: " << std::hex << INS_Address(ins) << ": "
                      << INS_Disassemble(ins) << ": unprotect reg "
                      << REG_StringShort(reg) << "\n";
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) protregs_unprotreg,
                           IARG_PTR, &protregs[reg],
                           IARG_END);
        }
    }
}

struct PTeXPlugin final : Plugin
{
    const char *name() const override { return "ptex"; }

    int priority() const override { return -10; }

    bool enabled() const override { return enable.Value(); }

    bool
    reg() override
    {
        INS_AddInstrumentFunction(protstore_instrument, nullptr);
        INS_AddInstrumentFunction(protregs_instrument, nullptr);
        protstore_tracebuf_id = PIN_DefineTraceBuffer(8, 4096 * 8, protstore_tracebuf_callback, nullptr); // ~128 MiB buffer.
        return true;
    }
} plugin;

}
