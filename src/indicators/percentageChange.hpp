#pragma once 
#include "dataModule/dataTypes/candle.hpp"
#include "dataModule/dataTypes/tradeEvent.hpp"
#include <vector>
// if negative negative if possitive possitive
double percentageChange(const std::vector<Candle>& candles) {
    const Candle& first = candles.front();  // Reference
    const Candle& last = candles.back();    // Reference
    return ((last.close - first.close) / first.close);
}

double percentageChange(const TradeEvent& event1, const TradeEvent& event2) {
    return (event2.price-event1.price) / event1.price;
}

double percentageChangeWeighted(const TradeEvent& event1, const TradeEvent& event2) {
    return (event1.price*event1.volume - event2.price*event2.volume) / event1.price*event1.volume;
}

double percentageVolumeChange(const TradeEvent& event1, const TradeEvent& event2) {
    return (event1.volume-event2.volume) / event1.volume;
}


