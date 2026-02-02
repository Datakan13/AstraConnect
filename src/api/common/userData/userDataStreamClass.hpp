#pragma once 
#include <string>
#include <vector>
#include "api/common/enums/enumClassesForAllStreams.hpp"
#include "api/common/enums/helperFunctionsForEnumClasses.hpp"
#include "api/common/userData/userDataStreamEvents.hpp"
#include <simdjson/simdjson.h>


class UserDataStream {
    EventType event;
    int64_t timestamp;
    PTRWrapper* ptrWrapper = nullptr;

public:
    auto returnPtr() const -> void* {
        if(!ptrWrapper) return nullptr;
        switch (event) {
            case EventType::ACCOUNT_UPDATE:        return ptrWrapper->accountUpdate;
            case EventType::MARGIN_CALL:           return ptrWrapper->marginCall;
            case EventType::ORDER_UPDATE:    return ptrWrapper->orderUpdate;
            case EventType::TRADE_LITE:            return ptrWrapper->tradeLite;
            case EventType::ACCOUNT_CONFIG_UPDATE: return ptrWrapper->accountConfigUpdate;
            case EventType::STRATEGY_UPDATE:       return ptrWrapper->strategyUpdate;
            case EventType::GRID_UPDATE:           return ptrWrapper->gridUpdate;
            case EventType::CONDITIONAL_ORDER_REJECT: return ptrWrapper->conditionalOrderReject;
            default: return nullptr;
        }
    }

    // Constructor for ACCOUNT_UPDATE event
    UserDataStream(EventType event_, int64_t timestamp_, AccountUpdate* accountUpdate_)
        : event(event_), timestamp(timestamp_), ptrWrapper(new PTRWrapper()) {
        ptrWrapper->accountUpdate = accountUpdate_;
    }

    // Constructor for MARGIN_CALL event
    UserDataStream(EventType event_, int64_t timestamp_, MarginCall* marginCall_)
        : event(event_), timestamp(timestamp_), ptrWrapper(new PTRWrapper()) {
        ptrWrapper->marginCall = marginCall_;
    }

    // Constructor for ORDER_UPDATE event
    UserDataStream(EventType event_, int64_t timestamp_, OrderUpdate* orderUpdate_)
        : event(event_), timestamp(timestamp_), ptrWrapper(new PTRWrapper()) {
        ptrWrapper->orderUpdate = orderUpdate_;
    }

    // Constructor for TRADE_LITE event
    UserDataStream(EventType event_, int64_t timestamp_, TradeLite* tradeLite_)
        : event(event_), timestamp(timestamp_), ptrWrapper(new PTRWrapper()) {
        ptrWrapper->tradeLite = tradeLite_;
    }

    // Constructor for ACCOUNT_CONFIG_UPDATE event
    UserDataStream(EventType event_, int64_t timestamp_, AccountConfigUpdate* accountConfigUpdate_)
        : event(event_), timestamp(timestamp_), ptrWrapper(new PTRWrapper()) {
        ptrWrapper->accountConfigUpdate = accountConfigUpdate_;
    }

    // Constructor for STRATEGY_UPDATE event
    UserDataStream(EventType event_, int64_t timestamp_, StrategyUpdate* strategyUpdate_)
        : event(event_), timestamp(timestamp_), ptrWrapper(new PTRWrapper()) {
        ptrWrapper->strategyUpdate = strategyUpdate_;
    }

    // Constructor for GRID_UPDATE event
    UserDataStream(EventType event_, int64_t timestamp_, GridUpdate* gridUpdate_)
        : event(event_), timestamp(timestamp_), ptrWrapper(new PTRWrapper()) {
        ptrWrapper->gridUpdate = gridUpdate_;
    }

    // Constructor for CONDITIONAL_ORDER_REJECT event
    UserDataStream(EventType event_, int64_t timestamp_, ConditionalOrderReject* conditionalOrderReject_)
        : event(event_), timestamp(timestamp_), ptrWrapper(new PTRWrapper()) {
        ptrWrapper->conditionalOrderReject = conditionalOrderReject_;
    }

    // Constructor for basic or generic events with no payload
    UserDataStream(EventType event_, int64_t timestamp_)
        : event(event_), timestamp(timestamp_),ptrWrapper(new PTRWrapper()) {}

    UserDataStream() {

    }

    ~UserDataStream() {
        if(!ptrWrapper) return;
        delete ptrWrapper->accountUpdate;
        delete ptrWrapper->marginCall;
        delete ptrWrapper->orderUpdate;
        delete ptrWrapper->tradeLite;
        delete ptrWrapper->accountConfigUpdate;
        delete ptrWrapper->strategyUpdate;
        delete ptrWrapper->gridUpdate;
        delete ptrWrapper->conditionalOrderReject;
        delete ptrWrapper;
    }
};



