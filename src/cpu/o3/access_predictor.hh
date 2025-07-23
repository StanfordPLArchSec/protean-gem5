#pragma once

#include <cstddef>
#include <unordered_map>

#include "cpu/o3/dyn_inst_ptr.hh"
#include "cpu/ptex.hh"

namespace gem5::o3
{

class AccessPredictor
{
  public:
    // If num_entries == 0, then infinitely sized.
    AccessPredictor(std::size_t num_entries, bool predict_protected);

    Protection predict(const DynInst &inst);
    void update(const DynInst &inst, Protection prot);

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


}
