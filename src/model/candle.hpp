#pragma once
#include "utils/decimal/fixedPoint.hpp"
#include <cstdint>
class Candle {
    public:
    int64_t timestampOpen;
    // 1e-8 units, see utils/decimal/fixedPoint.hpp
    AstraConnect::Utils::Fixed open;
    AstraConnect::Utils::Fixed high;
    AstraConnect::Utils::Fixed close;
    AstraConnect::Utils::Fixed low;
    // Volumes sum a whole interval and can pass int64 at 1e-8 (1000PEPEUSDT
    // trades ~3.6e10 per day), so they are 128-bit
    AstraConnect::Utils::Fixed128 volume;
    int64_t timestampClose;
    AstraConnect::Utils::Fixed128 quoteVolume;
};

