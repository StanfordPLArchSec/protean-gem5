#include "declassification_table.hh"

#include "cpu/o3/cpu.hh"
#include "cpu/o3/dyn_inst.hh"

namespace gem5::o3 {

void DeclassificationTable::init(CPU *cpu, const BaseO3CPUParams& params, const std::string& name) {
    clear();
}

ShadowDeclassificationTable::ShadowDeclassificationTable(bool shadowL1):
    shadowL1(shadowL1)
{
}

void ShadowDeclassificationTable::clear() {
    mem.clear();
}

bool ShadowDeclassificationTable::checkDeclassifiedOne(Addr addr) {
  const auto it = mem.find(addr);
  if (it == mem.end()) {
    return false;
  } else {
    return it->second;
  }
}

bool ShadowDeclassificationTable::checkDeclassified(const DynInstPtr& inst) {
    assert(inst->effAddrValid());
    const Addr base = inst->physEffAddr;
    const unsigned size = inst->effSize;
    assert(base != 0 && size > 0);
    for (Addr addr = base; addr < base + size; ++addr) {
        const bool check = checkDeclassifiedOne(addr);
        if (!check) {
            return false;
        }
    }
    return true;
}

bool ShadowDeclassificationTable::setDeclassified(const DynInstPtr& inst, bool isDeclassified) {
    assert(inst->isMemRef());
    if (!inst->effAddrValid()) {
        warn("setDeclassified called on instruction with !inst->effAddrValid(); ignoring\n");
        return true;
    }
    const Addr base = inst->physEffAddr;
    const unsigned size = inst->effSize;
    assert(base != 0 && size > 0);
    for (Addr addr = base; addr < base + size; ++addr) {
        if (isDeclassified) {
            mem.insert_or_assign(addr, true);
        } else {
            mem.insert_or_assign(addr, false);
        }
    }
    return true;
}


size_t
ShadowDeclassificationTable::publicBytes() const
{
    return std::count_if(mem.begin(), mem.end(), [] (const auto& p) {
        return p.second;
    });
}

size_t
ShadowDeclassificationTable::totalBytes() const
{
    return mem.size();
}

bool
ShadowDeclassificationTable::evictRange(Addr base, size_t size, std::vector<bool>& taint)
{
    if (!shadowL1)
        return false;
    taint.resize(size);
    bool evicted = false;
    for (size_t i = 0; i < size; ++i) {
        const auto it = mem.find(base + i);
        bool pub;
        if (it == mem.end()) {
            pub = false;
        } else {
            pub = it->second;
            mem.erase(it);
            evicted = true;
        }
        taint[i] = pub;
    }
    return evicted;
}

bool
ShadowDeclassificationTable::contains(Addr base, size_t size)
{
    for (Addr addr = base; addr < base + size; ++addr) {
        if (mem.find(addr) != mem.end())
            return true;
    }
    return false;
}

void
ShadowDeclassificationTable::take(Addr base, size_t size, const std::vector<bool>& taint)
{
    assert(size == taint.size());
    for (size_t i = 0; i < size; ++i) {
        assert(mem.find(base + i) == mem.end());
        mem[base + i] = taint[i];
    }
}

}
