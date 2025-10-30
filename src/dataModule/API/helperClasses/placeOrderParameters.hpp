#pragma once
#include "apiManager.hpp"


class APIManager::PlaceOrderParameters{
    public:
    RequestParameter<std::string> positionSide;
    RequestParameter<std::string> price;
    RequestParameter<std::string> quantity;
    RequestParameter<std::string> side;
    RequestParameter<std::string> symbol;
    RequestParameter<std::string> timeInForce;
    RequestParameter<std::string> timestamp;
    RequestParameter<std::string> type;

    void prepareOrder(PositionSide posSide,
        OrderSide orderSide,
        TimeInForce tif,
        OrderType orderType,
        const std::string& pair,
        const std::string& px,
        const std::string& qty)
    {
        // Position side (BOTH, LONG, SHORT)
        switch (posSide) {
            case PositionSide::BOTH:  positionSide.request = "BOTH"; break;
            case PositionSide::LONG:  positionSide.request = "LONG"; break;
            case PositionSide::SHORT: positionSide.request = "SHORT"; break;
        }

        // Side (BUY or SELL)
        side.request = (orderSide == OrderSide::BUY) ? "BUY" : "SELL";

        // Symbol
        symbol.request = pair;

        // Price & quantity
        price.request    = px;
        quantity.request = qty;

        // Order type
        switch (orderType) {
            case OrderType::LIMIT:                type.request = "LIMIT"; break;
            case OrderType::MARKET:               type.request = "MARKET"; break;
            case OrderType::STOP:                 type.request = "STOP"; break;
            case OrderType::STOP_MARKET:          type.request = "STOP_MARKET"; break;
            case OrderType::TAKE_PROFIT:          type.request = "TAKE_PROFIT"; break;
            case OrderType::TAKE_PROFIT_MARKET:   type.request = "TAKE_PROFIT_MARKET"; break;
            case OrderType::TRAILING_STOP_MARKET: type.request = "TRAILING_STOP_MARKET"; break;
            case OrderType::LIQUIDATION:          type.request = "LIQUIDATION"; break;
        }

        // Time in force
        switch (tif) {
            case TimeInForce::GTC: timeInForce.request = "GTC"; break;
            case TimeInForce::IOC: timeInForce.request = "IOC"; break;
            case TimeInForce::FOK: timeInForce.request = "FOK"; break;
            case TimeInForce::GTX: timeInForce.request = "GTX"; break;
        }

        // Timestamp (milliseconds)
        timestamp.request = std::to_string(AstraLib::Time::unixTimestampMS());
    }

    PlaceOrderParameters() : positionSide("positionSide"), 
    price("price"), quantity("quantity"), 
    side("side"), symbol("symbol"), 
    timeInForce("timeInForce"), timestamp("timestamp"),
    type("type") {

    }
};
