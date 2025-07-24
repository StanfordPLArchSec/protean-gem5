#include "cpu/o3/access_predictor.hh"

#include "debug/AccessPredictor.hh"
#include "base/logging.hh"
#include "cpu/o3/dyn_inst.hh"

namespace gem5::o3
{

AccessPredictor::AccessPredictor(std::size_t num_entries, bool predict_protected)
    : numEntries(num_entries),
      predictProtected(predict_protected)
{
    fatal_if(size() > 0 && !isPowerOf2(size()), "AccessPredictor's number of entries must be power of 2 (got %u)\n",
             size());
    DPRINTF(AccessPredictor, "Initialized access predictor with %u entries\n",
            size());
}

// TODO: Get rid of this. Redundant with numEntries.
std::size_t
AccessPredictor::size() const
{
    return numEntries;
}

void
AccessPredictor::validate(const DynInst &inst) const
{
    assert(inst.isLoad());
    assert(!size() || pred.size() <= size());
}

std::size_t
AccessPredictor::hash(const DynInst &inst) const
{
    std::size_t hash = inst.pcState().instAddr() & (size() - 1);
    assert(size() == 0 || hash < size());
    return hash;
}

Protection
AccessPredictor::predict(const DynInst &inst)
{
    validate(inst);
    if (!predictProtected && inst.hasProtPrefix())
        return Protected;
    const bool prediction = pred[hash(inst)];
    return prediction ? Protected : Unprotected;
}

void
AccessPredictor::update(const DynInst &inst, Protection prot)
{
    validate(inst);
    if (!predictProtected && inst.loadProtection() == Protected)
        return;
    bool value;
    switch (prot) {
      case Protected:
        value = true;
        break;
      case Unprotected:
        value = false;
        break;
      default:
        panic("Bad protection\n");
    }

    pred[hash(inst)] = value;
}


}
