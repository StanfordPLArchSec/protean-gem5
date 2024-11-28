#include <pin.H>
#include "plugin.hh"

namespace rtncount {
namespace {

unsigned long 

void
Static_RTN(RTN rtn, void *)
{
    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR) Dynamic_RTN, IARG_END);
    RTN_Close(rtn);
}

class RoutineCountPlugin : public Plugin {
    void
    reg() override
    {
        RTN_AddInstrumentFunction(Static_RTN, nullptr);
    }
};

}
}
