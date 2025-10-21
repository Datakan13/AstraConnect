#pragma once
#include <cstdint>
class TradeEvent {
    public:
    uint64_t timestamp;
    double price;
    double volume;
    bool maker;
};