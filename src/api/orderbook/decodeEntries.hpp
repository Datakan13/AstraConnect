#pragma once
#include <simdjson/simdjson.h>
#include "model/entry.hpp"
#include "utils/decimal/fixedPoint.hpp"
#include "api/orderbook/orderbookArrays.hpp"

// Decodes a combined-stream depth diff into a slot of the orderbook's holder.
//
// Runs on the stream thread, so it never throws and never blocks: anything it cannot
// decode comes back as a dropped event, and the orderbook resyncs. That includes the
// empty message the stream holder passes through after a reconnect.
// The holder used to be a function static, shared by every Orderbook instance.
struct DepthDecoder {
    OrderbookArrayHolder* holder;

    OrderbookArrays operator()(simdjson::padded_string& json, simdjson::ondemand::parser& parser) const noexcept {
        const int slot = holder->acquire();
        if(slot < 0) return OrderbookArrays();
        DepthEntries& bids = *holder->bids(slot);
        DepthEntries& asks = *holder->asks(slot);
        try {
            simdjson::ondemand::document doc = parser.iterate(json);
            simdjson::ondemand::object obj = doc["data"].get_object().value();
            int64_t bidCount = 0;
            int64_t askCount = 0;
            if(!readLevels(obj["b"], bids, bidCount) || !readLevels(obj["a"], asks, askCount)) {
                holder->release(slot);
                return OrderbookArrays();
            }
            int64_t u = obj["u"].get_uint64().value();
            int64_t U = obj["U"].get_uint64().value();
            int64_t pu = obj["pu"].get_uint64().value();
            return OrderbookArrays(&bids,&asks,slot,bidCount,askCount,u,U,pu);
        } catch(std::exception&) {
            holder->release(slot);
            return OrderbookArrays();
        }
    }

    private:
    // Each level is a [price, quantity] pair of strings. Returns false if they overflow
    // `out` or a pair is short (which used to leave volume uninitialised).
    template<typename Levels>
    static bool readLevels(Levels&& levels, DepthEntries& out, int64_t& count) {
        for(auto level : levels) {
            if(count >= DEPTH_EVENT_CAPACITY) return false;
            AstraConnect::Utils::Fixed values[2];
            int fields = 0;
            // Must be a named variable: iterating the temporary chain directly crashes
            // at -O2, as the intermediates die before the loop body runs
            auto pair = level.value().get_array().value();
            for(auto field : pair) {
                if(fields < 2) values[fields] = AstraConnect::Utils::readFixed(field.value());
                fields++;
            }
            if(fields < 2) return false;
            out[count++].assignValues(values[0], values[1]);
        }
        return true;
    }
};
