#pragma once
#include "userDataStreamClass.hpp"
#include "helperClassesUserDataStreamClass.hpp"
#include <simdjson.h>
#include "userDataStreamHelperFunctions.hpp"
#include <type_traits>

void accountUpdate(simdjson::ondemand::document& doc , UserDataStream*& stream) {
  stream = new UserDataStream(EventType::ACCOUNT_UPDATE,
    doc["E"].get_int64().value(),new AccountUpdate(doc)
  );
}

void marginCallUpdate(simdjson::ondemand::document& doc , UserDataStream*& stream) {
  stream = new UserDataStream(EventType::MARGIN_CALL, 
    doc["E"].get_int64().value(), new MarginCall(doc)
  );
}

void orderUpdate(simdjson::ondemand::document& doc , UserDataStream*& stream) {
  stream = new UserDataStream(EventType::ORDER_UPDATE, 
    doc["E"].get_int64().value(), 
    new OrderUpdate(doc)
);
}

void tradeLite(simdjson::ondemand::document& doc , UserDataStream*& stream) {
  stream = new UserDataStream(EventType::TRADE_LITE, 
    doc["E"].get_int64().value(), 
    new OrderUpdate(doc)
);
}

void accountConfigUpdate(simdjson::ondemand::document& doc , UserDataStream*& stream) {
  stream = new UserDataStream(EventType::ACCOUNT_CONFIG_UPDATE, 
    doc["E"].get_int64().value(), 
    new OrderUpdate(doc)
);
}

void strategyUpdate(simdjson::ondemand::document& doc , UserDataStream*& stream) {
  stream = new UserDataStream(EventType::STRATEGY_UPDATE, 
    doc["E"].get_int64().value(), 
    new OrderUpdate(doc)
);
}

void gridUpdate(simdjson::ondemand::document& doc , UserDataStream*& stream) {
  stream = new UserDataStream(EventType::GRID_UPDATE, 
    doc["E"].get_int64().value(), 
    new OrderUpdate(doc)
);
}

void conditionalOrderReject(simdjson::ondemand::document& doc , UserDataStream*& stream) {
  stream = new UserDataStream(EventType::CONDITIONAL_ORDER_REJECT, 
    doc["E"].get_int64().value(), 
    new OrderUpdate(doc)
);
}

void castUserDataStreamptr(void* ptr, AccountUpdate& out) {
  if (!ptr) return; // safety check
  auto cast = static_cast<AccountUpdate*>(ptr);
  out.assets = cast->assets;
  out.eventType = cast->eventType;
  out.positions = cast->positions;
}

void castUserDataStreamptr(void* ptr, MarginCall& out) {
  if (!ptr) return; // safety check
  auto cast = static_cast<MarginCall*>(ptr);
  out.crossWalletBalance = cast->crossWalletBalance;
  out.positions = cast->positions;
}

void castUserDataStreamptr(void* ptr, OrderUpdate& out) {
  if (!ptr) return;
  auto* cast = static_cast<OrderUpdate*>(ptr);
  out.pair                       = cast->pair;
  out.clientID                   = cast->clientID;
  out.originalQuantity           = cast->originalQuantity;
  out.originalPrice              = cast->originalPrice;
  out.averagePrice               = cast->averagePrice;
  out.stopPrice                  = cast->stopPrice;
  out.orderID                    = cast->orderID;
  out.lastFilledQuantity         = cast->lastFilledQuantity;
  out.totalFilledQuantity        = cast->totalFilledQuantity;
  out.lastFilledPrice            = cast->lastFilledPrice;
  out.commisionAsset             = cast->commisionAsset;
  out.commision                  = cast->commision;
  out.tradeTimestamp             = cast->tradeTimestamp;
  out.notitionalValueOfOrderB    = cast->notitionalValueOfOrderB;
  out.notitionalValueOfOrderA    = cast->notitionalValueOfOrderA;
  out.maker                      = cast->maker;
  out.reduceOnly                 = cast->reduceOnly;
  out.side                       = cast->side;
  out.orderType                  = cast->orderType;
  out.timeInForce                = cast->timeInForce;
  out.executionType              = cast->executionType;
  out.orderStatus                = cast->orderStatus;
  out.workingType                = cast->workingType;
  out.originalOrderType          = cast->originalOrderType;
  out.positionSide               = cast->positionSide;
  out.stpMode                    = cast->stpMode;
  out.priceMatchMode             = cast->priceMatchMode;
  out.expireReason               = cast->expireReason;
  out.closeAll                   = cast->closeAll;
  out.activationPrice            = cast->activationPrice;
  out.callBackRate               = cast->callBackRate;
  out.priceProtection            = cast->priceProtection;
  out.realizedProfit             = cast->realizedProfit;
  out.GTDExprationTimestamp      = cast->GTDExprationTimestamp;
}

void castUserDataStreamptr(void* ptr, TradeLite& out) {
  if (!ptr) return; // safety check
  auto cast = static_cast<TradeLite*>(ptr);
  out.clientID = cast->clientID;
  out.lastFilledPrice = cast->lastFilledPrice;
  out.lastFilledQuantity = cast->lastFilledQuantity;
  out.maker = cast->maker;
  out.orderID = cast->orderID;
  out.orderSide = cast->orderSide;
  out.originalPrice = cast->originalPrice;
  out.originalQuantity = cast->originalQuantity;
  out.pair = cast->pair;
  out.tradeID = cast->tradeID;
  out.tradeTimestamp = cast->tradeTimestamp;
}

void castUserDataStreamptr(void* ptr, AccountConfigUpdate& out) {
  if (!ptr) return; // safety check
  auto cast = static_cast<AccountConfigUpdate*>(ptr);
  out.leverage = cast->leverage;
  out.multiAssetMode = cast->multiAssetMode;
  out.pair = cast->pair;
  out.timestamp = cast->timestamp;
}

void castUserDataStreamptr(void* ptr, StrategyUpdate& out) {
  if (!ptr) return; // safety check
  auto cast = static_cast<StrategyUpdate*>(ptr);
  out.opCode = cast->opCode;
  out.pair = cast->pair;
  out.strategyID = cast->strategyID;
  out.strategyStatus = cast->strategyStatus;
  out.strategyType = cast->strategyType;
  out.timestamp = cast->timestamp;
  out.updateTimestamp = cast->updateTimestamp;
}

void castUserDataStreamptr(void* ptr, GridUpdate& out) {
  if (!ptr) return; // safety check
  auto cast = static_cast<GridUpdate*>(ptr);
  out.matchedPNL = cast->matchedPNL;
  out.pair = cast->pair;
  out.realizedPNL = cast->realizedPNL;
  out.strategyID = cast->strategyID;
  out.strategyStatus = cast->strategyStatus;
  out.strategyType = cast->strategyType;
  out.timestamp = cast->timestamp;
  out.unmatchedAveragePrice = cast->unmatchedAveragePrice;
  out.unmatchedFee = cast->unmatchedFee;
  out.unmatchedQuantity = cast->unmatchedQuantity;
  out.updateTimestamp = cast->updateTimestamp;
}

void castUserDataStreamptr(void* ptr, ConditionalOrderReject& out) {
  if (!ptr) return; // safety check
  auto cast = static_cast<ConditionalOrderReject*>(ptr);
  out.orderID = cast->orderID;
  out.pair = cast->pair;
  out.rejectReason = cast->rejectReason;
  out.timestamp = cast->timestamp;
}

template<typename Data>
Data getUserDataStreamData(void* ptr, EventType type) {
    if(!ptr) return Data{};

    if constexpr (std::is_same_v<Data, AccountUpdate>) {
        if (type == EventType::ACCOUNT_UPDATE) {
            AccountUpdate out;
            castUserDataStreamptr(ptr, out);
            return out;
        }

    } else if constexpr (std::is_same_v<Data, MarginCall>) {
        if (type == EventType::MARGIN_CALL) {
            MarginCall out;
            castUserDataStreamptr(ptr, out);
            return out;
        }

    } else if constexpr (std::is_same_v<Data, OrderUpdate>) {
        if (type == EventType::ORDER_UPDATE) {
            OrderUpdate out;
            castUserDataStreamptr(ptr, out);
            return out;
        }

    } else if constexpr (std::is_same_v<Data, TradeLite>) {
        if (type == EventType::TRADE_LITE) {
            TradeLite out;
            castUserDataStreamptr(ptr, out);
            return out;
        }

    } else if constexpr (std::is_same_v<Data, AccountConfigUpdate>) {
        if (type == EventType::ACCOUNT_CONFIG_UPDATE) {
            AccountConfigUpdate out;
            castUserDataStreamptr(ptr, out);
            return out;
        }

    } else if constexpr (std::is_same_v<Data, StrategyUpdate>) {
        if (type == EventType::STRATEGY_UPDATE) {
            StrategyUpdate out;
            castUserDataStreamptr(ptr, out);
            return out;
        }

    } else if constexpr (std::is_same_v<Data, GridUpdate>) {
        if (type == EventType::GRID_UPDATE) {
            GridUpdate out;
            castUserDataStreamptr(ptr, out);
            return out;
        }

    } else if constexpr (std::is_same_v<Data, ConditionalOrderReject>) {
        if (type == EventType::CONDITIONAL_ORDER_REJECT) {
            ConditionalOrderReject out;
            castUserDataStreamptr(ptr, out);
            return out;
        }
    }

    // Default case — type didn’t match Data
    return Data{};
}

