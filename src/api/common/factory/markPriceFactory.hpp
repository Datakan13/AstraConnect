#pragma once
#include <simdjson/simdjson.h>
#include "model/markPrice.hpp"

// Wire format: doc/payloads/streams/mark-price.md
auto parseMarkPrice = [](simdjson::padded_string& json, simdjson::ondemand::parser& parser) {
    MarkPrice out;

    auto iterate = parser.iterate(json);

    out.timestamp = iterate["E"].get_int64().value();
    out.markPrice = iterate["p"].get_double_in_string().value();
    out.indexPrice = iterate["i"].get_double_in_string().value();
    out.fundingRate = iterate["r"].get_double_in_string().value();
    out.fundingTime = iterate["T"].get_int64().value();

    return out;
};  
