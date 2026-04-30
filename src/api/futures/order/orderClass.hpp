#pragma once
#include "api/common/enums/commonTypes.hpp"
#include "orderResponseEvents.hpp"
#include <AstraLib/AstraLib.hpp>
#include "api/futures/order/orderSentEvents.hpp"
class Order {
    public:
    AstraLib::Atomic::Spinlock lock;
    std::string id;
    OrderTypeSent type;
    OrderResponsePtrWrapper* wrapper = nullptr;
    OrderSent sent;
    int64_t status;
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
        status = doc["status"].get_int64().value();
        wrapper = new OrderResponsePtrWrapper(type_,doc);
    }


    // Data functions
    int64_t getOrderId() {
    if(wrapper) return wrapper->getOrderId(type);
    return -1;
    }

    std::string getSymbol() {
        if(wrapper) return wrapper->getSymbol(type);
        return {};
    }

    double getPrice() {
        if(wrapper) return wrapper->getPrice(type);
        return 0.0;
    }

    double getOrigQty() {
        if(wrapper) return wrapper->getOrigQty(type);
        return 0.0;
    }

    double getExecutedQty() {
        if(wrapper) return wrapper->getExecutedQty(type);
        return 0.0;
    }

    OrderStatus getOrderStatus() {
        if(wrapper) return wrapper->getOrderStatus(type);
        return OrderStatus::NONE;
    }

    TimeInForce getTimeInForce() {
        if(wrapper) return wrapper->getTimeInForce(type);
        return TimeInForce::NONE;
    }

    OrderSide getOrderSide() {
        if(wrapper) return wrapper->getOrderSide(type);
        return OrderSide::NONE;
    }

    PositionSide getPositionSide() {
        if(wrapper) return wrapper->getPositionSide(type);
        return PositionSide::NONE;
    }

    double getAvgPrice() { return wrapper ? wrapper->getAvgPrice(type) : 0.0; }
    double getCumQty() { return wrapper ? wrapper->getCumQty(type) : 0.0; }
    double getCumQuote() { return wrapper ? wrapper->getCumQuote(type) : 0.0; }
    bool getReduceOnly() { return wrapper ? wrapper->getReduceOnly(type) : false; }
    bool getClosePosition() { return wrapper ? wrapper->getClosePosition(type) : false; }
    WorkingType getWorkingType() { return wrapper ? wrapper->getWorkingType(type) : WorkingType::NONE; }
    bool getPriceProtect() { return wrapper ? wrapper->getPriceProtect(type) : false; }
    OrderType getOrigType() { return wrapper ? wrapper->getOrigType(type) : OrderType::NONE; }
    PriceMatchMode getPriceMatchMode() { return wrapper ? wrapper->getPriceMatchMode(type) : PriceMatchMode::NONE; }
    STPMode getSelfTradePreventionMode() { return wrapper ? wrapper->getSelfTradePreventionMode(type) : STPMode::NONE; }
    int64_t getGoodTillDate() { return wrapper ? wrapper->getGoodTillDate(type) : 0; }
    int64_t getUpdateTime() { return wrapper ? wrapper->getUpdateTime(type) : 0; }


    Order(std::string& id_, OrderTypeSent type_, simdjson::ondemand::document& doc) :id(id_), type(type_), wrapper(new OrderResponsePtrWrapper(type_,doc)) {
        status = doc["status"].get_int64().value();
    }

    Order(int64_t timestamp_,PositionSide posSide,
                  OrderSide orderSide,
                  TimeInForce tif,
                  OrderType orderType,
                  const std::string& pair,
                  const double& px,
                  const double& qty) : sent(timestamp_,px,qty,pair,tif,orderSide,posSide,orderType){
                    lock.unlock();
    }

    ~Order() {
        if(wrapper) delete wrapper;
    }
};

class OrderAccessRAII {
public:
    Order* orderPtr;
    AstraLib::Pools::ThreadSafeIndexPool<1024>& pool;
    int index;
    AstraLib::Atomic::SpinlockGuard guard; 

    Order* getOrderPtr() {
        return orderPtr;
    }

    Order& getOrderRef() {
        return *orderPtr;
    }

    explicit OrderAccessRAII(Order* order_, AstraLib::Pools::ThreadSafeIndexPool<1024>& pool_,int index_)
        : orderPtr(order_), guard(order_->lock),pool(pool_), index(index_) {
        }  

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
