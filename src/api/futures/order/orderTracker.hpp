#pragma once 
#include <string>
#include "simdjson.h"
#include "dataModule/enums/enumClassesForAllStreams.hpp"
#include "dataModule/enums/helperFunctionsForEnumClasses.hpp"
#include "orderResponseEvents.hpp"
#include <AstraLib/AstraLib.hpp>
#include <unordered_map>
#include "orderClass.hpp"
#include "dataModule/dataTypes/accountInfoLite.hpp"
enum class GotLock{
    GOT
};

class OrderTracker {
    public:
    AstraLib::Atomic::Spinlock lock;
    std::unordered_map<std::string,Order*> map;
    OrderAccessHolder holder;
    AstraLib::Buffers::AtomicRingBuffer<int64_t,256> activeOrders;

    void registerOrderSent(std::string& id,int64_t timestamp, double price, double quantity, std::string sym, TimeInForce tif,
        OrderSide s, PositionSide pos, OrderType type,simdjson::ondemand::document& doc) {
        AstraLib::Atomic::SpinlockGuard guard(lock);
        map[id] = new Order(timestamp,pos,s,tif,type,sym,price,quantity);
        registerOrderResponse(GotLock::GOT,id,OrderTypeSent::NEW,doc);
    }

    // This will create a new order object
    void registerOrderResponse(std::string& id,OrderTypeSent type,simdjson::ondemand::document& doc) {
        {
            AstraLib::Atomic::SpinlockGuard mapGuard(lock);
            map[id] = new Order(id,type,doc);
        }
    }

    void registerOrderResponse(GotLock,std::string& id,OrderTypeSent type,simdjson::ondemand::document& doc) {
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

    int64_t getOrderStatusint(std::string& id) {
        AstraLib::Atomic::SpinlockGuard mapGuard(lock);
        if(!(map.contains(id))) return 0;
        return map[id]->status;
    }

    AstraLib::Atomic::Spinlock& getLock(std::string& id) {
        AstraLib::Atomic::SpinlockGuard mapGuard(lock);
        return map[id]->lock;
    }

    OrderAccessRAII* getOrderPtr(std::string& id) {
        OrderAccessRAII* orderPtr;
        {
            AstraLib::Atomic::SpinlockGuard mapGuard(lock);
            orderPtr = holder.getAccess(map[id]);
            mapGuard.~SpinlockGuard();
        }
        return orderPtr;
    }

    int64_t getActiveOrderId() {
        return activeOrders.dequeue();
    }

    void queueActiveOrderId(int64_t id) {
        activeOrders.noMoveEnqueue(id);
    }

};

class OrderAccess {
    public:
    OrderAccessRAII* access;
    Order& order;
    OrderAccess(OrderTracker& tracker_, std::string& id) : access(tracker_.getOrderPtr(id)),order(*access->orderPtr) {
    }
    ~OrderAccess() {
        delete access;
    }
};

// To get information about the sent order
class OrderInfo {
public:
    OrderTracker& tracker;

    explicit OrderInfo(OrderTracker& tracker_) : tracker(tracker_) {}

    class OrderSent {
        OrderTracker& tracker;
        public:
        explicit OrderSent(OrderTracker& tracker_) : tracker(tracker_) {}

        inline int64_t getTimestamp(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.sent.timestamp;
        }

        inline double getPrice(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.sent.price;
        }

        inline double getQuantity(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.sent.quantity;
        }

        inline std::string getPair(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.sent.pair;
        }

        inline TimeInForce getTimeInForce(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.sent.timeInForce;
        }

        inline OrderSide getOrderSide(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.sent.side;
        }

        inline PositionSide getPositionSide(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.sent.positionSide;
        }

        inline OrderType getOrderType(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.sent.type;
        }
    };

    inline int64_t getOrderStatusCode(std::string& id) {
        return tracker.getOrderStatusint(id);
    }

    class OrderResponse {
        OrderTracker& tracker;
    public:
        explicit OrderResponse(OrderTracker& tracker_) : tracker(tracker_) {}

        inline static OrderStatus getOrderStatus(std::string& id, OrderTracker& tracker) {
            OrderAccess access(tracker, id);
            return access.order.getOrderStatus();
        }

        inline int64_t getOrderId(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.getOrderId();
        }

        inline std::string getSymbol(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.getSymbol();
        }

        inline double getPrice(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.getPrice();
        }

        inline double getOrigQty(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.getOrigQty();
        }

        inline double getExecutedQty(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.getExecutedQty();
        }

        inline double getCumQty(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.getCumQty();
        }

        inline double getCumQuote(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.getCumQuote();
        }

        inline TimeInForce getTimeInForce(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.getTimeInForce();
        }

        inline OrderSide getOrderSide(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.getOrderSide();
        }

        inline PositionSide getPositionSide(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.getPositionSide();
        }

        inline WorkingType getWorkingType(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.getWorkingType();
        }

        inline bool getPriceProtect(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.getPriceProtect();
        }

        inline OrderType getOrigType(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.getOrigType();
        }

        inline PriceMatchMode getPriceMatchMode(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.getPriceMatchMode();
        }

        inline STPMode getSelfTradePreventionMode(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.getSelfTradePreventionMode();
        }

        inline int64_t getGoodTillDate(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.getGoodTillDate();
        }

        inline int64_t getUpdateTime(std::string& id) {
            OrderAccess access(tracker, id);
            return access.order.getUpdateTime();
        }
    };

    class AccountInfo {
        OrderTracker& tracker;
        public:
        explicit AccountInfo(OrderTracker& tracker_) : tracker(tracker_) {}

        void getAccountInfo(std::string& id,AccountInfoLite& out) {
            OrderAccess access(tracker,id);
            out.balance = access.order.wrapper->getBalance();
            out.availableBalance = access.order.wrapper->getAvailableBalance();
            out.asset = access.order.wrapper->getAsset();
            out.crossWalletBalance = access.order.wrapper->getCrossWalletBalance();
            out.updateTime = access.order.wrapper->getUpdateTimeAccount();
        }
    };
};

class OrderContext {
    public:
    OrderInfo orderInfo;
    OrderInfo::OrderSent orderInfoSent;
    OrderInfo::OrderResponse orderInfoResponse;

    OrderContext(OrderTracker& tracker_) : orderInfo(tracker_), orderInfoSent(tracker_), orderInfoResponse(tracker_) {

    }
};
