#pragma once
#include "utils/decimal/fixedPoint.hpp"
#include <cstdint>
class TradeEvent {
    public:
    uint64_t timestamp;
    // 1e-8 units, see utils/decimal/fixedPoint.hpp
    AstraConnect::Utils::Fixed price;
    AstraConnect::Utils::Fixed volume;
    bool maker;
};