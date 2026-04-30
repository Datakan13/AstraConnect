#pragma once
#include "api/common/enums/commonTypes.hpp"
#include <stdexcept>
// helperFunctionsForEnumClasses.hpp
//
// Maps exchange strings to internal enums.
//
// Error handling:
// - Most functions throw on unknown values
// - Some return defaults instead:
//     - TimeInForce → GTC
//     - RateLimitType / IntervalRateLimit → UNKNOWN

inline EventType returnEventType(const std::string& requested) {
    if (requested == "ACCOUNT_UPDATE")           return EventType::ACCOUNT_UPDATE;
    if (requested == "MARGIN_CALL")              return EventType::MARGIN_CALL;
    if (requested == "ORDER_TRADE_UPDATE")       return EventType::ORDER_UPDATE;
    if (requested == "TRADE_LITE")               return EventType::TRADE_LITE;
    if (requested == "ACCOUNT_CONFIG_UPDATE")    return EventType::ACCOUNT_CONFIG_UPDATE;
    if (requested == "STRATEGY_UPDATE")          return EventType::STRATEGY_UPDATE;
    if (requested == "GRID_UPDATE")              return EventType::GRID_UPDATE;
    if (requested == "CONDITIONAL_ORDER_REJECT") return EventType::CONDITIONAL_ORDER_REJECT;

    // Default fallback if unknown event type
    throw std::invalid_argument("Unknown event type: " + requested);
}

inline AccountUpdateType returnAccountUpdateType(const std::string& reason) {
    if (reason == "DEPOSIT")               return AccountUpdateType::DEPOSIT;
    if (reason == "WITHDRAW")              return AccountUpdateType::WITHDRAW;
    if (reason == "ORDER")                 return AccountUpdateType::ORDER;
    if (reason == "FUNDING_FEE")           return AccountUpdateType::FUNDING_FEE;
    if (reason == "WITHDRAW_REJECT")       return AccountUpdateType::WITHDRAW_REJECT;
    if (reason == "ADJUSTMENT")            return AccountUpdateType::ADJUSTMENT;
    if (reason == "INSURANCE_CLEAR")       return AccountUpdateType::INSURANCE_CLEAR;
    if (reason == "ADMIN_DEPOSIT")         return AccountUpdateType::ADMIN_DEPOSIT;
    if (reason == "ADMIN_WITHDRAW")        return AccountUpdateType::ADMIN_WITHDRAW;
    if (reason == "MARGIN_TRANSFER")       return AccountUpdateType::MARGIN_TRANSFER;
    if (reason == "MARGIN_TYPE_CHANGE")    return AccountUpdateType::MARGIN_TYPE_CHANGE;
    if (reason == "ASSET_TRANSFER")        return AccountUpdateType::ASSET_TRANSFER;
    if (reason == "OPTIONS_PREMIUM_FEE")   return AccountUpdateType::OPTIONS_PREMIUM_FEE;
    if (reason == "OPTIONS_SETTLE_PROFIT") return AccountUpdateType::OPTIONS_SETTLE_PROFIT;
    if (reason == "AUTO_EXCHANGE")         return AccountUpdateType::AUTO_EXCHANGE;
    if (reason == "COIN_SWAP_DEPOSIT")     return AccountUpdateType::COIN_SWAP_DEPOSIT;
    if (reason == "COIN_SWAP_WITHDRAW")    return AccountUpdateType::COIN_SWAP_WITHDRAW;

    throw std::invalid_argument("Unknown AccountUpdateType: " + reason);
}

inline MarginType returnMarginType(const std::string& type) {
    if (type == "isolated" || type == "ISOLATED")
        return MarginType::ISOLATED;
    if (type == "cross" || type == "CROSS")
        return MarginType::CROSS;

    throw std::invalid_argument("Unknown MarginType: " + type);
}

inline PositionSide returnPositionSide(const std::string& side) {
    if (side == "BOTH" || side == "both")
        return PositionSide::BOTH;
    if (side == "LONG" || side == "long")
        return PositionSide::LONG;
    if (side == "SHORT" || side == "short")
        return PositionSide::SHORT;

    throw std::invalid_argument("Unknown PositionSide: " + side);
}

inline OrderSide returnOrderSide(const std::string& side) {
    if (side == "SELL" || side == "sell")
        return OrderSide::SELL;
    if (side == "BUY" || side == "buy")
        return OrderSide::BUY;
    throw std::invalid_argument("Unknown OrderSide: " + side);
}

inline OrderType returnOrderType(const std::string& type) {
    if (type == "LIMIT" || type == "limit") return OrderType::LIMIT;
    if (type == "MARKET" || type == "market") return OrderType::MARKET;
    if (type == "STOP" || type == "stop") return OrderType::STOP;
    if (type == "STOP_MARKET" || type == "stop_market") return OrderType::STOP_MARKET;
    if (type == "TAKE_PROFIT" || type == "take_profit") return OrderType::TAKE_PROFIT;
    if (type == "TAKE_PROFIT_MARKET" || type == "take_profit_market") return OrderType::TAKE_PROFIT_MARKET;
    if (type == "TRAILING_STOP_MARKET" || type == "trailing_stop_market") return OrderType::TRAILING_STOP_MARKET;
    if (type == "LIQUIDATION" || type == "liquidation") return OrderType::LIQUIDATION;
    throw std::invalid_argument("Unknown OrderType: " + type);
}

inline TimeInForce returnTimeInForce(const std::string& tif) noexcept {
    if (tif == "GTC" || tif == "gtc") return TimeInForce::GTC;
    if (tif == "IOC" || tif == "ioc") return TimeInForce::IOC;
    if (tif == "FOK" || tif == "fok") return TimeInForce::FOK;
    if (tif == "GTX" || tif == "gtx") return TimeInForce::GTX;
    return TimeInForce::GTC; // default fallback (most common)
}

inline ExecutionType returnExecutionType(const std::string& exec) {
    if (exec == "NEW" || exec == "new") return ExecutionType::NEW;
    if (exec == "CANCELED" || exec == "canceled") return ExecutionType::CANCELED;
    if (exec == "CALCULATED" || exec == "calculated") return ExecutionType::CALCULATED;
    if (exec == "EXPIRED" || exec == "expired") return ExecutionType::EXPIRED;
    if (exec == "TRADE" || exec == "trade") return ExecutionType::TRADE;
    if (exec == "AMENDMENT" || exec == "amendment") return ExecutionType::AMENDMENT;
    throw std::invalid_argument("Unknown ExecutionType: " + exec);
}

inline OrderStatus returnOrderStatus(const std::string& status) {
    if (status == "NEW" || status == "new") return OrderStatus::NEW;
    if (status == "PARTIALLY_FILLED" || status == "partially_filled") return OrderStatus::PARTIALLY_FILLED;
    if (status == "FILLED" || status == "filled") return OrderStatus::FILLED;
    if (status == "CANCELED" || status == "canceled") return OrderStatus::CANCELED;
    if (status == "EXPIRED" || status == "expired") return OrderStatus::EXPIRED;
    if (status == "EXPIRED_IN_MATCH" || status == "expired_in_match") return OrderStatus::EXPIRED_IN_MATCH;
    throw std::invalid_argument("Unknown OrderStatus: " + status);
}

inline WorkingType returnWorkingType(const std::string& wt) {
    if (wt == "MARK_PRICE" || wt == "mark_price") return WorkingType::MARK_PRICE;
    if (wt == "CONTRACT_PRICE" || wt == "contract_price") return WorkingType::CONTRACT_PRICE;
    throw std::invalid_argument("Unknown WorkingType: " + wt);
}

inline STPMode returnSTPMode(const std::string& v) {
    if (v == "NONE" || v == "none") return STPMode::NONE;
    if (v == "EXPIRE_TAKER" || v == "expire_taker") return STPMode::EXPIRE_TAKER;
    if (v == "EXPIRE_MAKER" || v == "expire_maker") return STPMode::EXPIRE_MAKER;
    if (v == "EXPIRE_BOTH"  || v == "expire_both")  return STPMode::EXPIRE_BOTH;
    throw std::invalid_argument("Unknown STPMode: " + v);
}

inline PriceMatchMode returnPriceMatchMode(const std::string& pm) {
    if (pm == "NONE" || pm == "none") return PriceMatchMode::NONE;
    if (pm == "OPPONENT" || pm == "opponent") return PriceMatchMode::OPPONENT;
    if (pm == "QUEUE" || pm == "queue") return PriceMatchMode::QUEUE;
    if (pm == "MAKER" || pm == "maker") return PriceMatchMode::MAKER;
    if (pm == "BIDDER" || pm == "bidder") return PriceMatchMode::BIDDER;
    throw std::invalid_argument("Unknown PriceMatchMode: " + pm);
}

inline ExpireReason returnExpireReason(int reason) {
    switch (reason) {
        case 0: return ExpireReason::NONE;
        case 1: return ExpireReason::EXPIRED_SAVE;
        case 2: return ExpireReason::IOC_FAIL;
        case 3: return ExpireReason::IOC_FAIL_SAVE;
        case 4: return ExpireReason::REVERSED;
        case 5: return ExpireReason::LIQUIDATED;
        case 6: return ExpireReason::GTE_FAILED;
        case 7: return ExpireReason::SYMBOL_INVALID;
        case 8: return ExpireReason::STOP_TRIGGERED;
        case 9: return ExpireReason::MARKET_ORDER_FAIL;
        default:
            throw std::invalid_argument("Unknown ExpireReason code: " + std::to_string(reason));
    }
}

inline StrategyType returnStrategyType(const std::string& type) {
    if (type == "STOP") return StrategyType::STOP;
    if (type == "TAKE_PROFIT") return StrategyType::TAKE_PROFIT;
    if (type == "STOP_MARKET") return StrategyType::STOP_MARKET;
    if (type == "TAKE_PROFIT_MARKET") return StrategyType::TAKE_PROFIT_MARKET;
    if (type == "TRAILING_STOP_MARKET") return StrategyType::TRAILING_STOP_MARKET;
    if (type == "GRID") return StrategyType::GRID;
    if (type == "TWAP") return StrategyType::TWAP;
    if (type == "VP") return StrategyType::VP;
    if (type == "SIGNAL") return StrategyType::SIGNAL;
    if (type == "CUSTOM") return StrategyType::CUSTOM;
    throw std::invalid_argument("Unknown StrategyType string: " + type);
}

inline StrategyStatus returnStrategyStatus(const std::string& status) {
    if (status == "NEW") return StrategyStatus::NEW;
    if (status == "WORKING") return StrategyStatus::WORKING;
    if (status == "CANCELED") return StrategyStatus::CANCELED;
    if (status == "EXPIRED") return StrategyStatus::EXPIRED;

    throw std::invalid_argument("Unknown StrategyStatus string: " + status);
}

inline OpCode returnOpCode(int code) {
    switch (code) {
        case 8001: return OpCode::UPDATED;
        case 8002: return OpCode::CANCELED;
        case 8003: return OpCode::PLACED_OR_CANCELED;
        case 8004: return OpCode::STOP_REACHED;
        case 8005: return OpCode::LIQUIDATED;
        case 8006: return OpCode::MAX_ORDER_REACHED;
        case 8007: return OpCode::NEW_GRID_ORDER;
        case 8008: return OpCode::MARGIN_NOT_ENOUGH;
        case 8009: return OpCode::PRICE_OUT_OF_BOUNDS;
        case 8010: return OpCode::MARKET_CLOSED_OR_PAUSED;
        case 8011: return OpCode::CLOSE_FAILED;
        case 8012: return OpCode::MAX_NOTITIONAL_VALUE_EXCEEDED;
        case 8013: return OpCode::GRID_EXPIRED_KYC_OR_RESTRICTED;
        case 8014: return OpCode::RULES_VIOLATED_STOPPED;
        case 8015: return OpCode::POSITION_EMPTY_OR_LIQUIDATED;
        default:
            throw std::invalid_argument("Unknown OpCode: " + std::to_string(code));
    }
}


inline RateLimitType returnRateLimitType(const std::string& s) {
    if (s == "ORDERS") return RateLimitType::ORDER;
    if (s == "REQUEST_WEIGHT") return RateLimitType::REQUEST_WEIGHT;
    return RateLimitType::UNKNOWN;
}

inline IntervalRateLimit returnIntervalRateLimit(const std::string& s) {
    if (s == "SECOND") return IntervalRateLimit::SECOND;
    if (s == "MINUTE") return IntervalRateLimit::MINUTE;
    return IntervalRateLimit::UNKNOWN;
}
