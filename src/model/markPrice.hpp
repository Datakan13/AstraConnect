#pragma once
#include "utils/decimal/fixedPoint.hpp"
#include <cstdint>
class MarkPrice{
    public:
    int64_t timestamp;
    // 1e-8 units, see utils/decimal/fixedPoint.hpp
    AstraConnect::Utils::Fixed indexPrice;
    AstraConnect::Utils::Fixed markPrice;
    // A rate, not an amount: stays floating point
    double fundingRate;
    int64_t fundingTime;
};

