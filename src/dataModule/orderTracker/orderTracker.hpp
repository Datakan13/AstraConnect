#pragma once 
#include <string>
#include "simdjson.h"
#include "../enumClassesForAllStreams.hpp"
#include "../helperFunctionsForEnumClasses.hpp"
#include "orderResponseEvents.hpp"
#include <AstraLib/AstraLib.hpp>
#include <unordered_map>
#include "orderClass.hpp"

class OrderTracker {
    public:
    AstraLib::Atomic::Spinlock lock;
    std::unordered_map<std::string,Order*> map;
    OrderAccessHolder holder;
    AstraLib::Buffers::AtomicRingBuffer<std::string,256> activeOrders;

    void registerOrderSent(std::string& id,int64_t timestamp, double price, double quantity, std::string sym, TimeInForce tif,
          OrderSide s, PositionSide pos, OrderType type) {
        AstraLib::Atomic::SpinlockGuard guard(lock);
        map[id] = new Order(timestamp,pos,s,tif,type,sym,price,quantity);
    }

    void registerOrderResponse(std::string& id,OrderTypeSent type,simdjson::ondemand::document& doc) {
        AstraLib::Atomic::SpinlockGuard mapGuard(lock);
        map[id]->registerResponse(id,type,doc);
    }

    void updateOrderResponse(std::string& id,OrderTypeSent type,simdjson::ondemand::document& doc) {
        AstraLib::Atomic::SpinlockGuard mapGuard(lock);
        if(!(map.contains(id))) {
            map[id] = new Order(id, type, doc);  
        }
        OrderAccessRAII* access = holder.getAccess(map[id]);
        access->orderPtr->registerResponse(id,type,doc);  
        delete access;
    }

    void removeOrderResponse(std::string& id) {
        AstraLib::Atomic::SpinlockGuard mapGuard(lock);
        map.erase(id);
    }
    
    OrderTypeSent getOrderTypeStatus(std::string& id) {
        AstraLib::Atomic::SpinlockGuard mapGuard(lock);
        if(!(map.contains(id))) return OrderTypeSent::NONE; 
        return map[id]->type;
    }

    AstraLib::Atomic::Spinlock& getLock(std::string& id) {
        AstraLib::Atomic::SpinlockGuard mapGuard(lock);
        return map[id]->lock;
    }

    OrderAccessRAII* getOrderPtr(std::string& id) {
        AstraLib::Atomic::SpinlockGuard mapGuard(lock);
        return holder.getAccess(map[id]);
    }

    std::string getActiveOrderId() {
        return activeOrders.dequeue();
    }

    void queueActiveOrderId(std::string& id) {
        activeOrders.noMoveEnqueue(id);
    }
};

