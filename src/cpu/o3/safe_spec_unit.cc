#include "safe_spec_unit.hh"

#include <cstdlib>
#include <cstring>
#include <cassert>
#include "cpu/o3/cpu.hh"
#include "cpu/o3/dyn_inst.hh"
#include "enums/DeclassifyMode.hh"

namespace gem5::o3 {

SafeSpeculationUnit::Stats::Stats(statistics::Group *parent):
    statistics::Group(parent),
    ADD_STAT(hits, "[TPE] Number of decltab hits"),
    ADD_STAT(misses, "[TPE] Number of decltab misses"),
    ADD_STAT(averagePubBytes, "[TPE] Average public bytes"),
    ADD_STAT(averageBytes, "[TPE] Average total bytes"),
    ADD_STAT(averageSamples, "[TPE] Number of samples for the averages")
{
}

static DeclassificationTable *makeDeclassificationTable(DeclassifyMode decl_mode) {
    switch (decl_mode) {
      case DeclassifyMode::None:
        return new DummyDeclassificationTable();
      case DeclassifyMode::ShadowL1:
        return new ShadowDeclassificationTable(/*shadowL1*/true);
      case DeclassifyMode::ShadowMem:
        return new ShadowDeclassificationTable(/*shadowL1*/false);
    default: panic("unreachable");
    }
}


void SafeSpeculationUnit::init(CPU *cpu, const BaseO3CPUParams& params, unsigned id) {
    this->cpu = cpu;

    cpu->addStatGroup(csprintf("decltab%i", id).c_str(), &stats);

    const DeclassifyMode decl_mode = params.ptexMem;

    // PTeX: the decltab should be disabled if PTeX is disabled.
    assert(cpu->ptex || decl_mode == DeclassifyMode::None);

    declassified_addresses.reset(makeDeclassificationTable(decl_mode));
    declassified_addresses->init(cpu, params, csprintf("decltab%i", id));
    shadow.init(cpu, params, csprintf("shadow%i", id));
    clear();
}

bool SafeSpeculationUnit::checkDeclassified(const DynInstPtr& inst) {
    const bool is_declassified = declassified_addresses->checkDeclassified(inst);
    [[maybe_unused]] const bool shadow_declassified = shadow.checkDeclassified(inst);
    panic_if(is_declassified && !shadow_declassified, "Address should have been declassified: PC %s [sn:%lli]",
	     inst->pcState(), inst->seqNum);
    if (is_declassified) {
        ++stats.hits;
    } else {
        ++stats.misses;
    }
    return is_declassified;
}

void SafeSpeculationUnit::setDeclassified(const DynInstPtr& inst) {
    assert(inst->effAddrValid() && inst->physEffAddr != 0 && inst->effSize >= 1 && inst->effSize <= 8 && ((inst->effSize - 1) & inst->effSize) == 0);
    declassified_addresses->setDeclassified(inst);
    shadow.setDeclassified(inst);
}

bool SafeSpeculationUnit::setClassified(const DynInstPtr& inst) {
    const bool success = declassified_addresses->setClassified(inst);
    if (success) {
        assert(!declassified_addresses->checkDeclassified(inst));
    }
    shadow.setClassified(inst);
    return success;
}

void SafeSpeculationUnit::clear() {
    declassified_addresses->clear();
    shadow.clear();
    occupancy_counter = 0;
}

void SafeSpeculationUnit::tick() {
    declassified_addresses->tick();
    shadow.tick();
    if (++occupancy_counter == 100000UL) {
        occupancy_counter = 0;
        const auto pub_bytes = declassified_addresses->publicBytes();
        stats.averagePubBytes = pub_bytes;
        stats.averageBytes = declassified_addresses->totalBytes();
        ++stats.averageSamples;
    }
}

void
SafeSpeculationUnit::evictRange(Addr base, size_t size)
{
    std::vector<bool> taint;
    declassified_addresses->evictRange(base, size, taint);
}

}
