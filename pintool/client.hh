#pragma once

#include <pin.H>
#include <fstream>

bool IsKernelCode(ADDRINT addr);
bool IsKernelCode(INS ins);
bool IsKernelCode(TRACE trace);
std::ostream &log();
