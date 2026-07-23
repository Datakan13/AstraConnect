#pragma once
#include <simdjson/simdjson.h>
#include "model/entry.hpp"
#include "api/orderbook/orderbookArrays.hpp"
#include <AstraLib/AstraLib.hpp>

auto DecodeEntries = [](simdjson::padded_string& json, simdjson::ondemand::parser& parser) {
    static AstraLib::Pools::ThreadSafeIndexPool<128> indexPool;
    static OrderbookArrayHolder holder(indexPool);
    std::pair<std::array<Entry,8192>*,std::array<Entry,8192>*> arrays = holder.getArrays();
    std::array<Entry,8192>* bidArrayptr = arrays.first;
    std::array<Entry,8192>* askArrayptr = arrays.second;
    int64_t bidCount = 0;
    int64_t askCount = 0;
    int64_t u;
    int64_t U;
    int64_t pu;
    simdjson::ondemand::document doc = parser.iterate(json);
    simdjson::ondemand::object obj = doc["data"].get_object().value();
    for(auto bid : obj["b"]){
        auto ary = bid.value().get_array().value();
        double price = 0;
        double volume;
        for(auto val : ary){
            if(price != 0){
                volume = val.value().get_double_in_string().value();
            } else {
                price = val.value().get_double_in_string().value();
            }
        }
        (*bidArrayptr)[bidCount].assignValues(price,volume);
        bidCount++;
    }
    for(auto ask : obj["a"]){
        auto ary = ask.value().get_array().value();
        double price = 0;
        double volume;
        for(auto val : ary){
            if(price != 0){
                volume = val.value().get_double_in_string().value();
            } else {
                price = val.value().get_double_in_string().value();
            }
        }
        (*askArrayptr)[askCount].assignValues(price,volume);
        askCount++;
    }
    u = obj["u"].get_uint64().value();
    U = obj["U"].get_uint64().value();
    pu = obj["pu"].get_uint64().value();

    holder.returnIndex();
    return OrderbookArrays(bidArrayptr,askArrayptr,bidCount,askCount,u,U,pu);

};