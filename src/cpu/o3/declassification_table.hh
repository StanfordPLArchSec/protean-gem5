#pragma once

#include <memory>
#include <array>
#include <algorithm>
#include <type_traits>
#include <set>
#include <vector>
#include <list>
#include <vector>
#include <unordered_map>

#include "base/types.hh"
#include "base/statistics.hh"
#include "cpu/o3/dyn_inst_ptr.hh"

namespace gem5 {

class BaseO3CPUParams;

}


namespace gem5::o3 {

class CPU;

class DeclassificationTable {
  public:
    virtual void init(CPU *cpu, const BaseO3CPUParams& params, const std::string& name);
    virtual void clear() = 0;
    virtual void tick() = 0;
    virtual bool checkDeclassified(const DynInstPtr& inst) = 0;
    virtual void setDeclassified(const DynInstPtr& inst) = 0;
    [[nodiscard]] virtual bool setClassified(const DynInstPtr& inst) = 0;
    virtual size_t publicBytes() const = 0;
    virtual size_t totalBytes() const = 0;
    virtual bool evictRange(Addr base, size_t size, std::vector<bool>& taint) = 0;
    virtual bool contains(Addr base, size_t size) = 0;
    virtual void take(Addr base, size_t size, const std::vector<bool>& taint) {}
};

class DummyDeclassificationTable final : public DeclassificationTable {
  public:
    void clear() override {}
    void tick() override {}
    bool checkDeclassified(const DynInstPtr& inst) override { return false; }
    void setDeclassified(const DynInstPtr& inst) override {}
    bool setClassified(const DynInstPtr& inst) override { return true; }
    size_t publicBytes() const override { return 0UL; }
    size_t totalBytes() const override { return 0UL; }
    bool evictRange(Addr base, size_t size, std::vector<bool>& taint) override { return false; }
    bool contains(Addr base, size_t size) override { return false; }
};

class ShadowDeclassificationTable final : public DeclassificationTable {
  public:
    ShadowDeclassificationTable(bool shadowL1);
    void clear() override;
    void tick() override {}
    bool checkDeclassified(const DynInstPtr& inst) override;
    void setDeclassified(const DynInstPtr& inst) override { setDeclassified(inst, true); }
    bool setClassified(const DynInstPtr& inst) override { return setDeclassified(inst, false); }
    size_t publicBytes() const override;
    size_t totalBytes() const override;
    bool evictRange(Addr base, size_t size, std::vector<bool>& taint) override;
    bool contains(Addr base, size_t size) override;
    void take(Addr base, size_t size, const std::vector<bool>& taint) override;
  private:
    bool shadowL1;
    bool setDeclassified(const DynInstPtr& inst, bool isDeclassified);

    using Mem = std::unordered_map<Addr, bool>;
    Mem mem;

    bool checkDeclassifiedOne(Addr addr);
};

}
