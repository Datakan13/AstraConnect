#pragma once
#include <simdjson/simdjson.h>
#include "model/tradeEvent.hpp"

// Wire format: doc/payloads/streams/trade.md
auto parseTradeEvent = [](simdjson::padded_string& json,simdjson::ondemand::parser& parser){
    TradeEvent tradeOut;
    
    auto tradeEvent = parser.iterate(json);

    tradeOut.timestamp = tradeEvent["E"].get_int64().value();
    tradeOut.maker = tradeEvent["m"].get_bool().value();
    tradeOut.price = tradeEvent["p"].get_double_in_string().value();
    tradeOut.volume = tradeEvent["q"].get_double_in_string().value();
    return tradeOut;
};
