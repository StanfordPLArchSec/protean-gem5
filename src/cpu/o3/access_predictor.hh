#pragma once

#include <cstddef>
#include <vector>

#include "cpu/o3/dyn_inst_ptr.hh"
#include "cpu/ptex.hh"

namespace gem5::o3
{

class AccessPredictor
{
  public:
    AccessPredictor(std::size_t num_entries, bool predict_protected);

    Protection predict(const DynInst &inst) const;
    void update(const DynInst &inst, Protection prot);

  private:
    std::vector<bool> pred;
    const bool predictProtected;
    void validate(const DynInst &inst) const;
    std::size_t hash(const DynInst &inst) const;
    std::size_t size() const;
};


}
