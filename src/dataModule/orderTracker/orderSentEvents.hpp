#pragma once
#include "enums/enumClassesForAllStreams.hpp"
class OrderSent {
    public:
    int64_t timestamp;
    double price;
    double quantity;
    std::string pair;
    TimeInForce timeInForce;
    OrderSide side;
    PositionSide positionSide;
    OrderType type;

    OrderSent(int64_t ts, double p, double q, std::string sym, TimeInForce tif,
          OrderSide s, PositionSide pos, OrderType t)
    : timestamp(ts), price(p), quantity(q),
      pair(std::move(sym)), timeInForce(tif),
      side(s), positionSide(pos), type(t) {}

    OrderSent() {
        
    }
};