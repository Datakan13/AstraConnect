#pragma once
#include "utils/decimal/fixedPoint.hpp"
#include <cstdint>
class PairInfo {
    public:
    // 1e-8 units, see utils/decimal/fixedPoint.hpp
    AstraConnect::Utils::Fixed minNotional;
    AstraConnect::Utils::Fixed maxPrice;
    AstraConnect::Utils::Fixed minPrice;
    AstraConnect::Utils::Fixed tickSize;
    AstraConnect::Utils::Fixed maxQuantity;
    AstraConnect::Utils::Fixed minQuantity;
    AstraConnect::Utils::Fixed stepsize;
};