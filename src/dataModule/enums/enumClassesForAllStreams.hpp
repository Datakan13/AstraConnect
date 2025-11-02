#pragma once
#include <string>

enum class OrderTypeSent {
    NEW,
    MODIFY,
    CANCEL,
    QUERY,
    ACCOUNT_INFO,
    NONE
};

enum class RateLimitType {
    ORDER,
    REQUEST_WEIGHT,
    UNKNOWN
};

enum class IntervalRateLimit {
    SECOND,
    MINUTE,
    UNKNOWN
};

enum class MarginType {
    ISOLATED,
    CROSS
};

enum class PositionSide {
    BOTH,
    LONG,
    SHORT,
    NONE
};

enum class OrderSide {
    SELL,
    BUY,
    NONE
};

enum class AccountUpdateType {
    DEPOSIT,               // Deposit into account
    WITHDRAW,              // Withdrawal from account
    ORDER,                 // Result of an order execution or cancellation
    FUNDING_FEE,           // Funding fee payment or receipt
    WITHDRAW_REJECT,       // Withdrawal rejected
    ADJUSTMENT,            // Manual adjustment by system/admin
    INSURANCE_CLEAR,       // Insurance fund operation
    ADMIN_DEPOSIT,         // Admin-initiated deposit
    ADMIN_WITHDRAW,        // Admin-initiated withdrawal
    MARGIN_TRANSFER,       // Transfer between cross/isolated margin
    MARGIN_TYPE_CHANGE,    // Change of margin type (cross ↔ isolated)
    ASSET_TRANSFER,        // Transfer between accounts (e.g. spot ↔ futures)
    OPTIONS_PREMIUM_FEE,   // Options premium fee
    OPTIONS_SETTLE_PROFIT, // Options settlement profit/loss
    AUTO_EXCHANGE,         // Auto exchange between assets (e.g. for liquidation)
    COIN_SWAP_DEPOSIT,     // Coin-margined to USDT-margined conversion deposit
    COIN_SWAP_WITHDRAW     // USDT-margined to coin-margined conversion withdraw
};

enum class OrderType {
    LIMIT,
    MARKET,
    STOP,
    STOP_MARKET,
    TAKE_PROFIT,
    TAKE_PROFIT_MARKET,
    TRAILING_STOP_MARKET,
    LIQUIDATION,
    NONE
};

enum class ExecutionType {
    NEW,
    CANCELED,
    CALCULATED,
    EXPIRED,
    TRADE,
    AMENDMENT
};

enum class OrderStatus {
    NEW,
    PARTIALLY_FILLED,
    FILLED,
    CANCELED,
    EXPIRED,
    EXPIRED_IN_MATCH,
    NONE
};

enum class TimeInForce {
    GTC,    // Good till cancel
    IOC,    // Immediate or cancel
    FOK,    // Fill or kill
    GTX,     // Good till time/date
    NONE
};

enum class WorkingType {
    MARK_PRICE,
    CONTRACT_PRICE,
    NONE
};

enum class STPMode {
    NONE,
    EXPIRE_TAKER,
    EXPIRE_MAKER,
    EXPIRE_BOTH
};

enum class PriceMatchMode {
    NONE,
    OPPONENT,
    QUEUE,
    MAKER,
    BIDDER
};

enum class StrategyStatus {
    NEW,
    WORKING,
    CANCELED,
    EXPIRED
};

enum class OpCode : int {
    UPDATED = 8001,                        // 8001: The strategy params have been updated
    CANCELED = 8002,                       // 8002: User cancelled the strategy
    PLACED_OR_CANCELED = 8003,             // 8003: User manually placed or cancelled an order
    STOP_REACHED = 8004,                   // 8004: The stop limit of this order reached
    LIQUIDATED = 8005,                     // 8005: User position liquidated
    MAX_ORDER_REACHED = 8006,              // 8006: Max open order limit reached
    NEW_GRID_ORDER = 8007,                 // 8007: New grid order
    MARGIN_NOT_ENOUGH = 8008,              // 8008: Margin not enough
    PRICE_OUT_OF_BOUNDS = 8009,            // 8009: Price out of bounds
    MARKET_CLOSED_OR_PAUSED = 8010,        // 8010: Market is closed or paused
    CLOSE_FAILED = 8011,                   // 8011: Close position failed, unable to fill
    MAX_NOTITIONAL_VALUE_EXCEEDED = 8012,  // 8012: Exceeded the maximum allowable notional value at current leverage
    GRID_EXPIRED_KYC_OR_RESTRICTED = 8013, // 8013: Grid expired due to incomplete KYC verification or access from a restricted jurisdiction
    RULES_VIOLATED_STOPPED = 8014,         // 8014: Violated Futures Trading Quantitative Rules. Strategy stopped
    POSITION_EMPTY_OR_LIQUIDATED = 8015    // 8015: User position empty or liquidated
};



enum class ExpireReason {
    NONE,           //None, the default value
    EXPIRED_SAVE,    //Order has expired to prevent users from inadvertently trading against themselves
    IOC_FAIL,        //IOC order could not be filled completely, remaining quantity is canceled
    IOC_FAIL_SAVE,    //IOC order could not be filled completely to prevent users from inadvertently trading against themselves, remaining quantity is canceled
    REVERSED,       //Order has been canceled, as it's knocked out by another higher priority RO (market) order or reversed positions would be opened
    LIQUIDATED,     //Order has expired when the account was liquidated
    GTE_FAILED,      //Order has expired as GTE condition unsatisfied
    SYMBOL_INVALID,  //Order has been canceled as the symbol is delisted
    STOP_TRIGGERED,  //The initial order has expired after the stop order is triggered
    MARKET_ORDER_FAIL //Market order could not be filled completely, remaining quantity is canceled
};

enum class StrategyType {
    STOP,                 // Stop strategy
    TAKE_PROFIT,          // Take-profit strategy
    STOP_MARKET,          // Stop-market strategy
    TAKE_PROFIT_MARKET,   // Take-profit-market strategy
    TRAILING_STOP_MARKET, // Trailing-stop-market strategy
    GRID,                 // Grid trading strategy
    TWAP,                 // Time-Weighted Average Price strategy
    VP,                   // Volume Participation strategy
    SIGNAL,               // Signal-based (external signal) strategy
    CUSTOM                // Custom or user-defined strategy
};



enum class EventType {
    ACCOUNT_UPDATE,
    MARGIN_CALL,
    ORDER_UPDATE,
    TRADE_LITE,
    ACCOUNT_CONFIG_UPDATE,
    STRATEGY_UPDATE,
    GRID_UPDATE,
    CONDITIONAL_ORDER_REJECT
};

class AssetBalance {
    public:
    std::string asset;
    double walletBalance;
    double crossWalletBalance;
    double balanceChange;
};

class PositionBalance {
    public:
    std::string pair;
    double positionAmount;
    double entryPrice;
    double breakEvenPrice;
    double realizedPNL;
    double unrealizedPNL;
    double isolatedWalletBalance;
    MarginType marginType;
    PositionSide positionSide;
};

class MarginCallPosition {
    public:
    std::string pair;
    PositionSide positionSide;
    double positionAmount;
    MarginType marginType;
    double isolatedWalletBalance;
    double markPrice;
    double unrealizedPNL;
    double maintanceMargin;
};
