#pragma once
#include "../enumClassesForAllStreams.hpp"
#include "../helperFunctionsForEnumClasses.hpp"
#include "simdjson.h"
#include <AstraLib/AstraLib.hpp>
#include "../error.hpp"
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
    Error* error = nullptr;

    auto returnPtr(OrderTypeSent event) const -> void*{
        switch(event) {
            case OrderTypeSent::NEW:
            return newOrder;
            case OrderTypeSent::CANCEL:
            return cancelOrder;
            case OrderTypeSent::MODIFY:
            return modifyOrder;
            default:
            return nullptr;
        }
    }

    int64_t getOrderId(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->orderId;
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->orderId;
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->orderId;
            default:return -1;
        }
        
    }

    std::string getSymbol(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->symbol;
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->symbol;
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->symbol;
            default:return "";
        }
        
    }

    double getPrice(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->price;
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->price;
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->price;
            default:return 0.0;
        }
        
    }

    double getOrigQty(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->origQty;
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->origQty;
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->origQty;
            default:return 0.0;
        }
        
    }

    double getExecutedQty(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->executedQty;
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->executedQty;
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->executedQty;
            default:return 0.0;
        }
        
    }

    OrderStatus getOrderStatus(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->orderStatus;
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->orderStatus;
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->orderStatus;
            default:return OrderStatus::NONE;
        }
        
    }

    TimeInForce getTimeInForce(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->timeInForce;
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->timeInForce;
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->timeInForce;
            default:return TimeInForce::NONE;
        }
        
    }

    OrderSide getOrderSide(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->side;
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->side;
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->side;
            default:return OrderSide::NONE;
        }
        
    }

    PositionSide getPositionSide(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->positionSide;
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->positionSide;
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->positionSide;
            default: return PositionSide::NONE;
        }
    }

    double getAvgPrice(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->avgPrice; 
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->avgPrice; 
            default: return 0.0;
        }
    }

    double getCumQty(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->cumQty; 
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->cumQty; 
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->cumQty; 
            default: return 0.0;
        }
    }

    double getCumQuote(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->cumQuote; 
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->cumQuote; 
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->cumQuote; 
            default: return 0.0;
        }
    }

    bool getReduceOnly(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->reduceOnly; 
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->reduceOnly; 
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->reduceOnly; 
            default: return false;
        }
    }

    bool getClosePosition(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->closePosition; 
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->closePosition; 
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->closePosition; 
            default: return false;
        }
    }

    WorkingType getWorkingType(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->workingType; 
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->workingType; 
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->workingType; 
            default: return WorkingType::NONE;
        }
    }

    bool getPriceProtect(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->priceProtect; 
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->priceProtect; 
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->priceProtect; 
            default: return false;
        }
    }

    OrderType getOrigType(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->origType; 
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->origType; 
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->origType; 
            default: return OrderType::NONE;
        }
    }

    PriceMatchMode getPriceMatchMode(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->priceMatch; 
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->priceMatch; 
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->priceMatch; 
            default: return PriceMatchMode::NONE;
        }
    }

    STPMode getSelfTradePreventionMode(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->selfTradePreventionMode; 
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->selfTradePreventionMode; 
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->selfTradePreventionMode; 
            default: return STPMode::NONE;
        }
    }

    int64_t getGoodTillDate(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->goodTillDate; 
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->goodTillDate; 
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->goodTillDate; 
            default: return 0;
        }
    }

    int64_t getUpdateTime(OrderTypeSent event) {
        switch(event) {
            case OrderTypeSent::NEW: if(newOrder) return newOrder->updateTime; 
            case OrderTypeSent::CANCEL: if(cancelOrder) return cancelOrder->updateTime; 
            case OrderTypeSent::MODIFY: if(modifyOrder) return modifyOrder->updateTime; 
            default: return 0;
        }
    }


    OrderResponsePtrWrapper(OrderTypeSent type,simdjson::ondemand::document& doc) {
        if(doc["status"].get_int64().value() == 200) {
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
        } else {
            auto obj = doc["error"].get_object().value();
            error = new Error(obj["code"].get_int64().value());
            obj["msg"].get_string(error->message);
            std::cout << "Error: " << error->message << std::endl;
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


