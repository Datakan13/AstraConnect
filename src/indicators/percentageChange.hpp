#pragma once 
#include "candle.hpp"
#include <vector>
// if negative negative if possitive possitive
double percentageChange(const std::vector<Candle>& candles) {
    const Candle& first = candles.front();  // Reference
    const Candle& last = candles.back();    // Reference
    return ((last.close - first.close) / first.close);
}