#pragma once 
#include <memory>
#include <string>
#include <vector>
#include "api/common/enums/commonTypes.hpp"
#include "api/common/enums/toEnum.hpp"
#include "api/common/userData/userDataStreamEvents.hpp"
#include <simdjson/simdjson.h>


class UserDataStream {
    EventType event = EventType::UNKNOWN;
    int64_t timestamp = 0;
    // shared, not unique: UserDataStream is copied by value through the
    // ring buffer, so every copy has to refer to the same payload rather
    // than each claiming ownership of it.
    std::shared_ptr<PTRWrapper> ptrWrapper;

public:
    auto returnPtr() const -> void* {
        if(!ptrWrapper) return nullptr;
        switch (event) {
            case EventType::ACCOUNT_UPDATE:        return ptrWrapper->accountUpdate.get();
            case EventType::MARGIN_CALL:           return ptrWrapper->marginCall.get();
            case EventType::ORDER_UPDATE:    return ptrWrapper->orderUpdate.get();
            case EventType::TRADE_LITE:            return ptrWrapper->tradeLite.get();
            case EventType::ACCOUNT_CONFIG_UPDATE: return ptrWrapper->accountConfigUpdate.get();
            case EventType::STRATEGY_UPDATE:       return ptrWrapper->strategyUpdate.get();
            case EventType::GRID_UPDATE:           return ptrWrapper->gridUpdate.get();
            case EventType::CONDITIONAL_ORDER_REJECT: return ptrWrapper->conditionalOrderReject.get();
            default: return nullptr;
        }
    }

    // Constructor for ACCOUNT_UPDATE event
    UserDataStream(EventType event_, int64_t timestamp_, std::unique_ptr<AccountUpdate> accountUpdate_)
        : event(event_), timestamp(timestamp_), ptrWrapper(std::make_shared<PTRWrapper>()) {
        ptrWrapper->accountUpdate = std::move(accountUpdate_);
    }

    // Constructor for MARGIN_CALL event
    UserDataStream(EventType event_, int64_t timestamp_, std::unique_ptr<MarginCall> marginCall_)
        : event(event_), timestamp(timestamp_), ptrWrapper(std::make_shared<PTRWrapper>()) {
        ptrWrapper->marginCall = std::move(marginCall_);
    }

    // Constructor for ORDER_UPDATE event
    UserDataStream(EventType event_, int64_t timestamp_, std::unique_ptr<OrderUpdate> orderUpdate_)
        : event(event_), timestamp(timestamp_), ptrWrapper(std::make_shared<PTRWrapper>()) {
        ptrWrapper->orderUpdate = std::move(orderUpdate_);
    }

    // Constructor for TRADE_LITE event
    UserDataStream(EventType event_, int64_t timestamp_, std::unique_ptr<TradeLite> tradeLite_)
        : event(event_), timestamp(timestamp_), ptrWrapper(std::make_shared<PTRWrapper>()) {
        ptrWrapper->tradeLite = std::move(tradeLite_);
    }

    // Constructor for ACCOUNT_CONFIG_UPDATE event
    UserDataStream(EventType event_, int64_t timestamp_, std::unique_ptr<AccountConfigUpdate> accountConfigUpdate_)
        : event(event_), timestamp(timestamp_), ptrWrapper(std::make_shared<PTRWrapper>()) {
        ptrWrapper->accountConfigUpdate = std::move(accountConfigUpdate_);
    }

    // Constructor for STRATEGY_UPDATE event
    UserDataStream(EventType event_, int64_t timestamp_, std::unique_ptr<StrategyUpdate> strategyUpdate_)
        : event(event_), timestamp(timestamp_), ptrWrapper(std::make_shared<PTRWrapper>()) {
        ptrWrapper->strategyUpdate = std::move(strategyUpdate_);
    }

    // Constructor for GRID_UPDATE event
    UserDataStream(EventType event_, int64_t timestamp_, std::unique_ptr<GridUpdate> gridUpdate_)
        : event(event_), timestamp(timestamp_), ptrWrapper(std::make_shared<PTRWrapper>()) {
        ptrWrapper->gridUpdate = std::move(gridUpdate_);
    }

    // Constructor for CONDITIONAL_ORDER_REJECT event
    UserDataStream(EventType event_, int64_t timestamp_, std::unique_ptr<ConditionalOrderReject> conditionalOrderReject_)
        : event(event_), timestamp(timestamp_), ptrWrapper(std::make_shared<PTRWrapper>()) {
        ptrWrapper->conditionalOrderReject = std::move(conditionalOrderReject_);
    }

    // Constructor for basic or generic events with no payload
    UserDataStream(EventType event_, int64_t timestamp_)
        : event(event_), timestamp(timestamp_),ptrWrapper(std::make_shared<PTRWrapper>()) {}

    UserDataStream() {

    }

};



