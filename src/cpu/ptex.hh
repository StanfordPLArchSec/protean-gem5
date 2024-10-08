#pragma once

namespace gem5
{

enum Protection : unsigned char {
    // Skip 0 to catch bugs.
    Unprotected = 1,
    Protected = 2,
};

}
