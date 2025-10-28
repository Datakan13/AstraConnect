#pragma once
#include "../enumClassesForAllStreams.hpp"
#include "orderResponseEvents.hpp"
#include <AstraLib/AstraLib.hpp>
#include "orderSentEvents.hpp"
class Order {
    public:
    AstraLib::Atomic::Spinlock lock;
    std::string id;
    OrderTypeSent type;
    OrderResponsePtrWrapper* wrapper = nullptr;
    OrderSent sent;
    auto returnPtr() const -> void* {
        if(!wrapper) return nullptr;
        switch(type) {
            case OrderTypeSent::NEW:
            return wrapper->newOrder;

            case OrderTypeSent::CANCEL:
            return wrapper->cancelOrder;

            case OrderTypeSent::MODIFY:
            return wrapper->modifyOrder;

            default:
            return nullptr;
        }
    }

    void registerResponse(std::string& id_, OrderTypeSent type_, simdjson::ondemand::document& doc) {
        id = id_ ;
        type = type_;
        wrapper = new OrderResponsePtrWrapper(type_,doc);
    }

    Order(std::string& id_, OrderTypeSent type_, simdjson::ondemand::document& doc) :id(id_), type(type_), wrapper(new OrderResponsePtrWrapper(type_,doc)) {

    }

    Order(int64_t timestamp_,PositionSide posSide,
                  OrderSide orderSide,
                  TimeInForce tif,
                  OrderType orderType,
                  const std::string& pair,
                  const double& px,
                  const double& qty) : sent(timestamp_,px,qty,pair,tif,orderSide,posSide,orderType){

    }

    ~Order() {
        if(wrapper) delete wrapper;
    }
};

class OrderAccessRAII {
public:
    Order* orderPtr;
    AstraLib::Atomic::SpinlockGuard guard; 
    AstraLib::Pools::ThreadSafeIndexPool<1024>& pool;
    int index;
    explicit OrderAccessRAII(Order* order_, AstraLib::Pools::ThreadSafeIndexPool<1024>& pool_,int index_)
        : orderPtr(order_), guard(order_->lock),pool(pool_), index(index_) {}  

        ~OrderAccessRAII() {
            pool.returnIndex(index);
        }
    Order* operator->() noexcept { return orderPtr; }
    Order& operator*() noexcept { return *orderPtr; }
};

class OrderAccessHolder {
    std::array<OrderAccessRAII*,1024> array;
    AstraLib::Pools::ThreadSafeIndexPool<1024> pool;

    public:
    OrderAccessRAII* getAccess(Order* order) {
        int index = pool.getIndex();
        array[index] = new OrderAccessRAII(order,pool,index);
        return array[index];
    }


};
