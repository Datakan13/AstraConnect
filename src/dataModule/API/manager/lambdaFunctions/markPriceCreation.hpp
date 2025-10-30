#pragma once
#include "simdjson.h"
#include "markPrice.hpp"


/*
    Payload example for mark price stream 
    Update rate: 1000ms
  {
    "e": "markPriceUpdate",  	// Event type
    "E": 1562305380000,      	// Event time
    "s": "BTCUSDT",          	// Symbol
    "p": "11794.15000000",   	// Mark price
    "i": "11784.62659091",		// Index price
    "P": "11784.25641265",		// Estimated Settle Price, only useful in the last hour before the settlement starts
    "r": "0.00038167",       	// Funding rate
    "T": 1562306400000       	// Next funding time
  }
*/
auto MarkPriceCreation = [](simdjson::padded_string& json, simdjson::ondemand::parser& parser) {
    MarkPrice out;

    auto iterate = parser.iterate(json);

    out.timestamp = iterate["E"].get_int64().value();
    out.markPrice = iterate["p"].get_double_in_string().value();
    out.indexPrice = iterate["i"].get_double_in_string().value();
    out.fundingRate = iterate["r"].get_double_in_string().value();
    out.fundingTime = iterate["T"].get_int64().value();

    return out;
};  
