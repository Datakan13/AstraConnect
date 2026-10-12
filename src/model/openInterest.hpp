#pragma once 
#include <cstdint>
class OpenInterest {
    public:
    uint64_t timestamp;
    // 1e-8 units, see utils/decimal/fixedPoint.hpp. Market-wide totals, so
    // 128-bit: circulating supply alone is ~4e11 for 1000PEPEUSDT
    AstraConnect::Utils::Fixed128 totalInterest;
    AstraConnect::Utils::Fixed128 totalInterestValue;
    AstraConnect::Utils::Fixed128 circulation;
};

class CurrentOpenInterest {
    public:
    uint64_t timestamp;
    // 1e-8 units, 128-bit like the totals above
    AstraConnect::Utils::Fixed128 openInterest;
};