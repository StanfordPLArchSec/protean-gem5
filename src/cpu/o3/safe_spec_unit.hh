#pragma once

#include "base/types.hh"
#include "cpu/inst_seq.hh"
#include "base/statistics.hh"

#include <map>
#include <set>
#include <cstdint>
#include <memory>

#include "cpu/o3/declassification_table.hh"

namespace gem5::o3 {

class CPU;

class SafeSpeculationUnit {
  public:
    SafeSpeculationUnit(): shadow(/*shadowL1*/false), stats(nullptr) {}
    void init(CPU *cpu, const BaseO3CPUParams& params, unsigned id);
    bool checkDeclassified(const DynInstPtr& inst);

    /**
     * Returns whether the declassification requires a retry.
     */
    void setDeclassified(const DynInstPtr& inst);
    [[nodiscard]] bool setClassified(const DynInstPtr& inst);
    void clear();
    void tick();
    void evictRange(Addr base, size_t size);

  private:
    CPU *cpu;
    // FIXME: Rename to decltab.
    std::unique_ptr<DeclassificationTable> declassified_addresses;
    ShadowDeclassificationTable shadow;
    unsigned long occupancy_counter;

    struct Stats : public statistics::Group {
        Stats(statistics::Group *parent);

        /** Total hits. */
        statistics::Scalar hits;

        /** Total misses. */
        statistics::Scalar misses;

        /** Average number of public byte addresses. */
        statistics::Scalar averagePubBytes;

        /** Averate total bytes tracked (including both public and private). */
        statistics::Scalar averageBytes;

        /** Samples for average. */
        statistics::Scalar averageSamples;
    } stats;
};

}
