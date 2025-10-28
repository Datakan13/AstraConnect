#pragma once
#include "../enumClassesForAllStreams.hpp"
#include "../helperFunctionsForEnumClasses.hpp"
#include "simdjson.h"
#include <AstraLib/AstraLib.hpp>

class RateLimit {
    public:
    RateLimitType type;
    IntervalRateLimit interval;
    int64_t intervalNum;
    int64_t limit;
    int64_t count;

    RateLimit(std::string& type_, std::string& interval_, int64_t& intervalNum_,int64_t& limit_, int64_t& count_) {
        type = returnRateLimitType(type_);
        interval = returnIntervalRateLimit(interval_);
        intervalNum = intervalNum_;
        limit = limit_;
        count = count_;
    }

    RateLimit(int64_t& intervalNum_,int64_t& limit_, int64_t& count_) {
        intervalNum = intervalNum_;
        limit = limit_;
        count = count_;
    }

    RateLimit(RateLimitType& type_,IntervalRateLimit interval_,int64_t& intervalNum_,int64_t& limit_, int64_t& count_) {
        type = type_;
        interval = interval_;
        intervalNum = intervalNum_;
        limit = limit_;
        count = count_;
    }

    RateLimit() {

    }
};

class RateLimitHolder {
    public:
    AstraLib::Buffers::AtomicRingBuffer<RateLimit,8> rateLimitBuffer;
    AstraLib::Atomic::PaddedAtomic<int> limitCount;
    void addRateLimit(std::string& type, std::string& interval, int64_t& intervalNum,int64_t& limit, int64_t& count) {
        limitCount.value.fetch_add(1,std::memory_order_acq_rel);
        rateLimitBuffer.emplaceEnqueue(type,interval,intervalNum,limit,count);
    }

    RateLimit getRateLimit() {
        return rateLimitBuffer.dequeue();
    }
    
};

/*
payload example for new order response 
{
    "id": "3f7df6e3-2df4-44b9-9919-d2f38f90a99a",
    "status": 200,
    "result": {
        "orderId": 325078477,
        "symbol": "BTCUSDT",
        "status": "NEW",
        "clientOrderId": "iCXL1BywlBaf2sesNUrVl3",
        "price": "43187.00",
        "avgPrice": "0.00",
        "origQty": "0.100",
        "executedQty": "0.000",
        "cumQty": "0.000",
        "cumQuote": "0.00000",
        "timeInForce": "GTC",
        "type": "LIMIT",
        "reduceOnly": false,
        "closePosition": false,
        "side": "BUY",
        "positionSide": "BOTH",
        "stopPrice": "0.00",
        "workingType": "CONTRACT_PRICE",
        "priceProtect": false,
        "origType": "LIMIT",
        "priceMatch": "NONE",
        "selfTradePreventionMode": "NONE",
        "goodTillDate": 0,
        "updateTime": 1702555534435
    },
    "rateLimits": [
        {
            "rateLimitType": "ORDERS",
            "interval": "SECOND",
            "intervalNum": 10,
            "limit": 300,
            "count": 1
        },
        {
            "rateLimitType": "ORDERS",
            "interval": "MINUTE",
            "intervalNum": 1,
            "limit": 1200,
            "count": 1
        },
        {
            "rateLimitType": "REQUEST_WEIGHT",
            "interval": "MINUTE",
            "intervalNum": 1,
            "limit": 2400,
            "count": 1
        }
    ]
}
*/
class OrderNewResponse {
public:
    // Top-level fields
    std::string id;           // "3f7df6e3-2df4-44b9-9919-d2f38f90a99a"
    int status;               // 200

    // Result fields (flattened)
    int64_t orderId;        // 325078477
    std::string symbol;       // "BTCUSDT"
    OrderStatus orderStatus;  // "NEW"  (renamed to avoid shadowing)
    std::string clientOrderId;// "iCXL1BywlBaf2sesNUrVl3"
    double price;        // "43187.00"
    double avgPrice;     // "0.00"
    double origQty;      // "0.100"
    double executedQty;  // "0.000"
    double cumQty;       // "0.000"
    double cumQuote;     // "0.00000"
    TimeInForce timeInForce;  // "GTC"
    OrderType type;         // "LIMIT"
    bool reduceOnly;          // false
    bool closePosition;       // false
    OrderSide side;         // "BUY"
    PositionSide positionSide; // "BOTH"
    double stopPrice;    // "0.00"
    WorkingType workingType;  // "CONTRACT_PRICE"
    bool priceProtect;        // false
    OrderType origType;     // "LIMIT"
    PriceMatchMode priceMatch;   // "NONE"
    STPMode selfTradePreventionMode; // "NONE"
    int64_t goodTillDate;   // 0
    int64_t updateTime;     // 1702555534435

    RateLimitHolder rateLimits;

    void parseFields(simdjson::ondemand::document& doc) {
        doc["id"].get_string(id);
        status = doc["status"].get_int64().value();

        auto obj = doc["result"].get_object().value();

        orderId = obj["orderId"].get_int64().value();
        obj["symbol"].get_string(symbol);

        std::string intermediateString;

        obj["status"].get_string(intermediateString);
        orderStatus = returnOrderStatus(intermediateString);

        obj["clientOrderId"].get_string(clientOrderId);
        price = obj["price"].get_double_in_string().value();
        avgPrice = obj["avgPrice"].get_double_in_string().value();
        origQty = obj["origQty"].get_double_in_string().value();
        executedQty = obj["executedQty"].get_double_in_string().value();
        cumQty = obj["cumQty"].get_double_in_string().value();
        cumQuote = obj["cumQuote"].get_double_in_string().value();

        obj["timeInForce"].get_string(intermediateString);
        timeInForce = returnTimeInForce(intermediateString);

        obj["type"].get_string(intermediateString);
        type = returnOrderType(intermediateString);

        reduceOnly = obj["reduceOnly"].get_bool().value();
        obj["side"].get_string(intermediateString);

        side = returnOrderSide(intermediateString);
        obj["positionSide"].get_string(intermediateString);

        positionSide = returnPositionSide(intermediateString);
        stopPrice = obj["stopPrice"].get_double_in_string().value();

        obj["workingType"].get_string(intermediateString);
        workingType = returnWorkingType(intermediateString);

        priceProtect = obj["priceProtect"].get_bool().value();

        obj["origType"].get_string(intermediateString);
        origType = returnOrderType(intermediateString);

        obj["priceMatch"].get_string(intermediateString);
        priceMatch = returnPriceMatchMode(intermediateString);

        obj["selfTradePreventionMode"].get_string(intermediateString);
        selfTradePreventionMode = returnSTPMode(intermediateString);
        goodTillDate = obj["goodTillDate"].get_int64().value();
        updateTime = obj["updateTime"].get_int64().value();

        auto rateArray = doc["rateLimits"].get_array().value();
        std::string rateLimitType;
        std::string rateLimitInterval;
        int64_t intervalNum;
        int64_t limit;
        int64_t count;
        for(auto rate : rateArray) {
            auto obj = rate.get_object().value();
            obj["rateLimitType"].get_string(rateLimitType);
            obj["interval"].get_string(rateLimitInterval);
            intervalNum = obj["intervalNum"].get_int64().value();
            limit = obj["limit"].get_int64().value();
            count = obj["count"].get_int64().value();
            rateLimits.addRateLimit(rateLimitType,rateLimitInterval,intervalNum,limit,count);
        }
    }

    OrderNewResponse(simdjson::ondemand::document& doc) {
        parseFields(doc);
    }

};

/*
payload example for order modify response
{
    "id": "c8c271ba-de70-479e-870c-e64951c753d9",
    "status": 200,
    "result": {
        "orderId": 328971409,
        "symbol": "BTCUSDT",
        "status": "NEW",
        "clientOrderId": "xGHfltUMExx0TbQstQQfRX",
        "price": "43769.10",
        "avgPrice": "0.00",
        "origQty": "0.110",
        "executedQty": "0.000",
        "cumQty": "0.000",
        "cumQuote": "0.00000",
        "timeInForce": "GTC",
        "type": "LIMIT",
        "reduceOnly": false,
        "closePosition": false,
        "side": "SELL",
        "positionSide": "SHORT",
        "stopPrice": "0.00",
        "workingType": "CONTRACT_PRICE",
        "priceProtect": false,
        "origType": "LIMIT",
        "priceMatch": "NONE",
        "selfTradePreventionMode": "NONE",
        "goodTillDate": 0,
        "updateTime": 1703426756190
    },
    "rateLimits": [
        {
            "rateLimitType": "ORDERS",
            "interval": "SECOND",
            "intervalNum": 10,
            "limit": 300,
            "count": 1
        },
        {
            "rateLimitType": "ORDERS",
            "interval": "MINUTE",
            "intervalNum": 1,
            "limit": 1200,
            "count": 1
        },
        {
            "rateLimitType": "REQUEST_WEIGHT",
            "interval": "MINUTE",
            "intervalNum": 1,
            "limit": 2400,
            "count": 1
        }
    ]
}
*/
class OrderModifyResponse {
public:
    // Top-level fields
    std::string id;           // "3f7df6e3-2df4-44b9-9919-d2f38f90a99a"
    int status;               // 200

    // Result fields (flattened)
    int64_t orderId;        // 325078477
    std::string symbol;       // "BTCUSDT"
    OrderStatus orderStatus;  // "NEW"  (renamed to avoid shadowing)
    std::string clientOrderId;// "iCXL1BywlBaf2sesNUrVl3"
    double price;        // "43187.00"
    double avgPrice;     // "0.00"
    double origQty;      // "0.100"
    double executedQty;  // "0.000"
    double cumQty;       // "0.000"
    double cumQuote;     // "0.00000"
    TimeInForce timeInForce;  // "GTC"
    OrderType type;         // "LIMIT"
    bool reduceOnly;          // false
    bool closePosition;       // false
    OrderSide side;         // "BUY"
    PositionSide positionSide; // "BOTH"
    double stopPrice;    // "0.00"
    WorkingType workingType;  // "CONTRACT_PRICE"
    bool priceProtect;        // false
    OrderType origType;     // "LIMIT"
    PriceMatchMode priceMatch;   // "NONE"
    STPMode selfTradePreventionMode; // "NONE"
    int64_t goodTillDate;   // 0
    int64_t updateTime;     // 1702555534435

    RateLimitHolder rateLimits;

    void parseFields(simdjson::ondemand::document& doc) {
        doc["id"].get_string(id);
        status = doc["status"].get_int64().value();

        auto obj = doc["result"].get_object().value();

        orderId = obj["orderId"].get_int64().value();
        obj["symbol"].get_string(symbol);

        std::string intermediateString;

        obj["status"].get_string(intermediateString);
        orderStatus = returnOrderStatus(intermediateString);

        obj["clientOrderId"].get_string(clientOrderId);
        price = obj["price"].get_double_in_string().value();
        avgPrice = obj["avgPrice"].get_double_in_string().value();
        origQty = obj["origQty"].get_double_in_string().value();
        executedQty = obj["executedQty"].get_double_in_string().value();
        cumQty = obj["cumQty"].get_double_in_string().value();
        cumQuote = obj["cumQuote"].get_double_in_string().value();

        obj["timeInForce"].get_string(intermediateString);
        timeInForce = returnTimeInForce(intermediateString);

        obj["type"].get_string(intermediateString);
        type = returnOrderType(intermediateString);

        reduceOnly = obj["reduceOnly"].get_bool().value();
        obj["side"].get_string(intermediateString);

        side = returnOrderSide(intermediateString);
        obj["positionSide"].get_string(intermediateString);

        positionSide = returnPositionSide(intermediateString);
        stopPrice = obj["stopPrice"].get_double_in_string().value();

        obj["workingType"].get_string(intermediateString);
        workingType = returnWorkingType(intermediateString);

        priceProtect = obj["priceProtect"].get_bool().value();

        obj["origType"].get_string(intermediateString);
        origType = returnOrderType(intermediateString);

        obj["priceMatch"].get_string(intermediateString);
        priceMatch = returnPriceMatchMode(intermediateString);

        obj["selfTradePreventionMode"].get_string(intermediateString);
        selfTradePreventionMode = returnSTPMode(intermediateString);
        goodTillDate = obj["goodTillDate"].get_int64().value();
        updateTime = obj["updateTime"].get_int64().value();

        auto rateArray = doc["rateLimits"].get_array().value();
        std::string rateLimitType;
        std::string rateLimitInterval;
        int64_t intervalNum;
        int64_t limit;
        int64_t count;
        for(auto rate : rateArray) {
            auto obj = rate.get_object().value();
            obj["rateLimitType"].get_string(rateLimitType);
            obj["interval"].get_string(rateLimitInterval);
            intervalNum = obj["intervalNum"].get_int64().value();
            limit = obj["limit"].get_int64().value();
            count = obj["count"].get_int64().value();
            rateLimits.addRateLimit(rateLimitType,rateLimitInterval,intervalNum,limit,count);
        }
    }

    OrderModifyResponse(simdjson::ondemand::document& doc) {
        parseFields(doc);
    }
};

/*
payload example for order cancel response 
{
  "id": "5633b6a2-90a9-4192-83e7-925c90b6a2fd",
  "status": 200,
  "result": {
    "clientOrderId": "myOrder1",
    "cumQty": "0",
    "cumQuote": "0",
    "executedQty": "0",
    "orderId": 283194212,
    "origQty": "11",
    "origType": "TRAILING_STOP_MARKET",
    "price": "0",
    "reduceOnly": false,
    "side": "BUY",
    "positionSide": "SHORT",
    "status": "CANCELED",
    "stopPrice": "9300",                
    "closePosition": false,  
    "symbol": "BTCUSDT",
    "timeInForce": "GTC",
    "type": "TRAILING_STOP_MARKET",
    "activatePrice": "9020",            
    "priceRate": "0.3",                
    "updateTime": 1571110484038,
    "workingType": "CONTRACT_PRICE",
    "priceProtect": false,           
    "priceMatch": "NONE",              
    "selfTradePreventionMode": "NONE",
    "goodTillDate": 0                 
  },
  "rateLimits": [
    {
      "rateLimitType": "REQUEST_WEIGHT",
      "interval": "MINUTE",
      "intervalNum": 1,
      "limit": 2400,
      "count": 1
    }
  ]
}
*/
class OrderCancelResponse {
public:
    std::string id;
    int status;

    std::string clientOrderId;   // "myOrder1"
    double cumQty;               // "0"
    double cumQuote;             // "0"
    double executedQty;          // "0"
    int64_t orderId;             // 283194212
    double origQty;              // "11"
    OrderType origType;          // "TRAILING_STOP_MARKET"
    double price;                // "0"
    bool reduceOnly;             // false
    OrderSide side;              // "BUY"
    PositionSide positionSide;   // "SHORT"
    OrderStatus orderStatus;     // "CANCELED"
    double stopPrice;            // "9300"
    bool closePosition;          // false
    std::string symbol;          // "BTCUSDT"
    TimeInForce timeInForce;     // "GTC"
    OrderType type;              // "TRAILING_STOP_MARKET"
    double activatePrice;        // "9020"
    double priceRate;            // "0.3"
    int64_t updateTime;          // 1571110484038
    WorkingType workingType;     // "CONTRACT_PRICE"
    bool priceProtect;           // false
    PriceMatchMode priceMatch;   // "NONE"
    STPMode selfTradePreventionMode; // "NONE"
    int64_t goodTillDate;        // 0

    RateLimitHolder rateLimits;

    void parseFields(simdjson::ondemand::document& doc) {
        doc["id"].get_string(id);
        status = doc["status"].get_int64().value();
        std::string intermediateString;

        auto obj = doc["result"].get_object().value();

        obj["clientOrderId"].get_string(clientOrderId);

        cumQty = obj["cumQty"].get_double_in_string().value();

        cumQuote = obj["cumQuote"].get_double_in_string().value();        

        executedQty = obj["executedQty"].get_double_in_string().value();

        orderId = obj["orderId"].get_int64().value();

        origQty = obj["origQty"].get_double_in_string().value();

        obj["origType"].get_string(intermediateString);
        origType = returnOrderType(intermediateString);

        price = obj["price"].get_double_in_string().value();

        reduceOnly = obj["reduceOnly"].get_bool().value();

        obj["side"].get_string(intermediateString);
        side = returnOrderSide(intermediateString);

        obj["positionSide"].get_string(intermediateString);
        positionSide = returnPositionSide(intermediateString);

        obj["status"].get_string(intermediateString);
        orderStatus = returnOrderStatus(intermediateString);

        stopPrice = obj["stopPrice"].get_double_in_string().value();

        closePosition = obj["closePosition"].get_bool().value();

        obj["symbol"].get_string(symbol);

        obj["timeInForce"].get_string(intermediateString);
        timeInForce = returnTimeInForce(intermediateString);

        obj["type"].get_string(intermediateString);
        type = returnOrderType(intermediateString);

        activatePrice = obj["activatePrice"].get_double_in_string().value();

        priceRate = obj["priceRate"].get_double_in_string().value();

        updateTime = obj["updateTime"].get_int64().value();

        obj["workingType"].get_string(intermediateString);
        workingType = returnWorkingType(intermediateString);

        priceProtect = obj["priceProtect"].get_bool().value();

        obj["priceMatch"].get_string(intermediateString);
        priceMatch = returnPriceMatchMode(intermediateString);

        obj["selfTradePreventionMode"].get_string(intermediateString);
        selfTradePreventionMode = returnSTPMode(intermediateString);
        goodTillDate = obj["goodTillDate"].get_int64().value();

        auto rateArray = doc["rateLimits"].get_array().value();
        std::string rateLimitType;
        std::string rateLimitInterval;
        int64_t intervalNum;
        int64_t limit;
        int64_t count;
        for(auto rate : rateArray) {
            auto obj = rate.get_object().value();
            obj["rateLimitType"].get_string(rateLimitType);
            obj["interval"].get_string(rateLimitInterval);
            intervalNum = obj["intervalNum"].get_int64().value();
            limit = obj["limit"].get_int64().value();
            count = obj["count"].get_int64().value();
            rateLimits.addRateLimit(rateLimitType,rateLimitInterval,intervalNum,limit,count);
        }
    }

    OrderCancelResponse(simdjson::ondemand::document& doc) {
        parseFields(doc);
    }
};

class OrderResponsePtrWrapper {
    public:
    OrderNewResponse* newOrder = nullptr;
    OrderCancelResponse* cancelOrder = nullptr;
    OrderModifyResponse* modifyOrder = nullptr;

    OrderResponsePtrWrapper(OrderTypeSent type,simdjson::ondemand::document& doc) {
        switch(type) {
            case OrderTypeSent::NEW:
            newOrder = new OrderNewResponse(doc);
            break;

            case OrderTypeSent::CANCEL:
            cancelOrder = new OrderCancelResponse(doc);
            break;

            case OrderTypeSent::MODIFY:
            modifyOrder = new OrderModifyResponse(doc);
            break;

            default:
            break;
        }
    }

    OrderResponsePtrWrapper(OrderResponsePtrWrapper& ref) {
        ref.newOrder = newOrder;
        ref.cancelOrder = cancelOrder;
        ref.modifyOrder = modifyOrder;
    }

    ~OrderResponsePtrWrapper() {
        delete newOrder;
        delete cancelOrder;
        delete modifyOrder;
    }
};
