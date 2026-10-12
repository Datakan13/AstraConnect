#pragma once
#include <simdjson/simdjson.h>
#include "model/tradeEvent.hpp"
#include "utils/decimal/fixedPoint.hpp"

// Wire format: doc/payloads/streams/trade.md
inline auto parseTradeEvent = [](simdjson::padded_string& json,simdjson::ondemand::parser& parser){
    TradeEvent tradeOut;
    
    auto tradeEvent = parser.iterate(json);

    tradeOut.timestamp = tradeEvent["E"].get_int64().value();
    tradeOut.maker = tradeEvent["m"].get_bool().value();
    tradeOut.price = AstraConnect::Utils::readFixed(tradeEvent["p"]);
    tradeOut.volume = AstraConnect::Utils::readFixed(tradeEvent["q"]);
    return tradeOut;
};
