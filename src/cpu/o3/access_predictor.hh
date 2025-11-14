#pragma once

#include <cstddef>
#include <unordered_map>

#include "cpu/o3/dyn_inst_ptr.hh"
#include "cpu/protean.hh"

namespace gem5
{

class BaseO3CPUParams;

namespace o3
{

class BaseAccessPredictor
{
  public:
    static BaseAccessPredictor *makePredictor(const BaseO3CPUParams &params);

    virtual Protection predict(const DynInst &inst) = 0;
    virtual void update(const DynInst &inst, Protection prot) = 0;
};

class AccessPredictor final : public BaseAccessPredictor
{
  public:
    // If num_entries == 0, then infinitely sized.
    AccessPredictor(std::size_t size, bool predict_protected);

    Protection predict(const DynInst &inst) override;
    void update(const DynInst &inst, Protection prot) override;

  private:
    // Use a hash map over a vector just so we can also model an
    // infinitely sized predictor as well.
    std::unordered_map<std::size_t, bool> pred;
    const std::size_t numEntries;
    const bool predictProtected;
    void validate(const DynInst &inst) const;
    std::size_t hash(const DynInst &inst) const;
    std::size_t size() const;
};

class DummyAccessPredictor final : public BaseAccessPredictor
{
  public:
    DummyAccessPredictor(Protection prot)
        : prot(prot)
    {
    }

    Protection predict(const DynInst &inst) override { return prot; }
    void update(const DynInst &inst, Protection prot) override {}

  private:
    const Protection prot;
};


}

}
