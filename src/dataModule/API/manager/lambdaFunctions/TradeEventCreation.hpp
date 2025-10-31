#pragma once
#include "simdjson.h"
#include "dataModule/dataTypes/tradeEvent.hpp"

/*
    Payload example for trade event stream
    Update rate: Event based
    {
        "e": "trade",       // Event type
        "E": 1672515782136, // Event time
        "s": "BNBBTC",      // Symbol
        "t": 12345,         // Trade ID
        "p": "0.001",       // Price
        "q": "100",         // Quantity
        "T": 1672515782136, // Trade time
        "m": true,          // Is the buyer the market maker?
        "M": true           // Ignore
    }
*/
auto TradeEventCreation = [](simdjson::padded_string& json,simdjson::ondemand::parser& parser){
    TradeEvent tradeOut;
    
    auto tradeEvent = parser.iterate(json);

    tradeOut.timestamp = tradeEvent["E"].get_int64().value();
    tradeOut.maker = tradeEvent["m"].get_bool().value();
    tradeOut.price = tradeEvent["p"].get_double_in_string().value();
    tradeOut.volume = tradeEvent["q"].get_double_in_string().value();
    return tradeOut;
};
