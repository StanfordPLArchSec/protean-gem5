#pragma once

#include <pin.H>
#include <fstream>
#include <string>

bool IsKernelCode(ADDRINT addr);
bool IsKernelCode(INS ins);
bool IsKernelCode(TRACE trace);
std::ostream &log();

const std::string *GetSymbol(ADDRINT addr);
