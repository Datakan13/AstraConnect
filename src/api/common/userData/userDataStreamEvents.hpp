#pragma once
#include <memory>
#include <simdjson/simdjson.h>
#include "api/common/enums/commonTypes.hpp"
#include "api/common/enums/toEnum.hpp"

// Wire format: doc/payloads/streams/user-account-update.md
class AccountUpdate {
    public:
    AccountUpdateType eventType;
    std::vector<AssetBalance> assets;
    std::vector<PositionBalance> positions;

    inline void parseFields(simdjson::ondemand::document& doc) {
        std::string eventType_;
        auto obj = doc["a"].get_object().value();
        obj["m"].get_string(eventType_);
        eventType = returnAccountUpdateType(eventType_);
        auto balanceArray = obj["B"].get_array().value();
        AssetBalance asset;
        for(auto field : balanceArray) {
            field["a"].get_string(asset.asset);
            asset.walletBalance = field["wb"].get_double_in_string().value();
            asset.crossWalletBalance = field["cw"].get_double_in_string().value();
            asset.balanceChange = field["bc"].get_double_in_string().value();
            assets.push_back(asset);
        }
        // "P" is omitted when FUNDING FEE lands on a crossed position: the docs say
        // only "B" is pushed then. Absent positions is normal, not an error.
        auto positionField = obj["P"];
        if(positionField.error()) return;
        auto positionArray = positionField.get_array().value();
        PositionBalance position;
        std::string marginType;
        std::string positionSide;
        for(auto field : positionArray) {
            field["s"].get_string(position.pair);
            position.positionAmount = field["pa"].get_double_in_string().value();
            position.entryPrice = field["ep"].get_double_in_string().value();
            position.breakEvenPrice = field["bep"].get_double_in_string().value();
            position.unrealizedPNL = field["up"].get_double_in_string().value();
            field["mt"].get_string(marginType);
            position.marginType = returnMarginType(marginType);
            position.isolatedWalletBalance = field["iw"].get_double_in_string().value();
            field["ps"].get_string(positionSide);
            position.positionSide = returnPositionSide(positionSide);
            positions.push_back(position);
        }

    }
    
    AccountUpdate(simdjson::ondemand::document& doc) {
        parseFields(doc);
    };
};
// Wire format: doc/payloads/streams/user-margin-call.md
class MarginCall {
    public:
    double crossWalletBalance;
    std::vector<MarginCallPosition> positions;

    void parseFields(simdjson::ondemand::document& doc) {
        crossWalletBalance = doc["cw"].get_double_in_string().value();
        MarginCallPosition position;
        std::string positionSide;
        std::string margintype;
        auto positionsArray = doc["p"].get_array().value();
        for (auto field : positionsArray) {
            field["s"].get_string(position.pair);
            field["ps"].get_string(positionSide);
            position.positionSide = returnPositionSide(positionSide);
            position.positionAmount = field["pa"].get_double_in_string().value();
            field["mt"].get_string(margintype);
            position.marginType = returnMarginType(margintype);
            position.isolatedWalletBalance = field["iw"].get_double_in_string().value();
            position.markPrice = field["mp"].get_double_in_string().value();
            position.unrealizedPNL = field["up"].get_double_in_string().value();
            position.maintenanceMargin = field["mm"].get_double_in_string().value();
            positions.push_back(position);
        }
    }
    MarginCall(simdjson::ondemand::document& doc) {
        parseFields(doc);
    }
};
// Wire format: doc/payloads/streams/user-order-update.md
class OrderUpdate {
    public:
    std::string pair;
    std::string clientID;
    double originalQuantity;
    double originalPrice;   // price requested on limit orders 0 for market
    double averagePrice;        // average price of executed fills 
    double stopPrice;
    int64_t orderID;
    double lastFilledQuantity;
    double totalFilledQuantity;
    double lastFilledPrice;
    std::string commisionAsset;
    double commision;
    int64_t tradeTimestamp;
    double notitionalValueOfOrderB;
    double notitionalValueOfOrderA;
    bool maker;
    bool reduceOnly;
    OrderSide side;
    OrderType orderType;
    TimeInForce timeInForce;
    ExecutionType executionType;
    OrderStatus orderStatus;
    WorkingType workingType;
    OrderType originalOrderType;
    PositionSide positionSide;
    STPMode stpMode;
    PriceMatchMode priceMatchMode;
    ExpireReason expireReason;
    bool closeAll;
    double activationPrice = 0;
    double callBackRate = 0;
    bool priceProtection;
    double realizedProfit;
    int64_t GTDExprationTimestamp;

    void parseFields(simdjson::ondemand::document& doc) {
        auto obj = doc["o"].get_object().value();
        obj["s"].get_string(pair);
        obj["c"].get_string(clientID);
        std::string intermediateString;
        obj["S"].get_string(intermediateString);
        side = returnOrderSide(intermediateString);
        obj["o"].get_string(intermediateString);
        orderType = returnOrderType(intermediateString);
        obj["f"].get_string(intermediateString);
        timeInForce = returnTimeInForce(intermediateString);
        originalQuantity = obj["q"].get_double_in_string().value();
        originalPrice = obj["p"].get_double_in_string().value();
        averagePrice = obj["ap"].get_double_in_string().value();
        stopPrice = obj["sp"].get_double_in_string().value();
        obj["x"].get_string(intermediateString);
        executionType = returnExecutionType(intermediateString);
        obj["X"].get_string(intermediateString);
        orderStatus = returnOrderStatus(intermediateString);
        orderID = obj["i"].get_int64().value();
        lastFilledQuantity = obj["l"].get_double_in_string().value();
        totalFilledQuantity = obj["z"].get_double_in_string().value();
        lastFilledPrice = obj["L"].get_double_in_string().value();
        obj["N"].get_string(commisionAsset);
        commision = obj["n"].get_double_in_string().value();
        tradeTimestamp = obj["T"].get_int64().value();
        notitionalValueOfOrderB = obj["b"].get_double_in_string().value();
        notitionalValueOfOrderA = obj["a"].get_double_in_string().value();
        maker = obj["m"].get_bool().value();
        reduceOnly = obj["R"].get_bool().value();
        obj["wt"].get_string(intermediateString);
        workingType = returnWorkingType(intermediateString);
        obj["ot"].get_string(intermediateString);
        originalOrderType = returnOrderType(intermediateString);
        obj["ps"].get_string(intermediateString);
        positionSide = returnPositionSide(intermediateString);
        closeAll = obj["cp"].get_bool().value();
        if(orderType == OrderType::TRAILING_STOP_MARKET) {
            activationPrice = obj["AP"].get_double_in_string().value();
            callBackRate = obj["cr"].get_double_in_string().value();
        }
        priceProtection = obj["pP"].get_bool().value();
        realizedProfit = obj["rp"].get_double_in_string().value();
        obj["V"].get_string(intermediateString);
        stpMode = returnSTPMode(intermediateString);
        obj["pm"].get_string(intermediateString);
        priceMatchMode = returnPriceMatchMode(intermediateString);
        GTDExprationTimestamp = obj["gtd"].get_int64().value();
        expireReason = returnExpireReason(obj["er"].get_int64_in_string().value());

    }   

    OrderUpdate(simdjson::ondemand::document& doc) {
        parseFields(doc);
    }
};
// Wire format: doc/payloads/streams/user-trade-lite.md
class TradeLite {
    public:
    int64_t tradeTimestamp;
    std::string clientID;
    std::string pair;
    double originalQuantity;
    double originalPrice;
    bool maker;
    OrderSide orderSide;
    double lastFilledPrice;
    double lastFilledQuantity;
    int64_t tradeID;
    int64_t orderID;

    void parseFields(simdjson::ondemand::document& doc) {
        std::string intermediateString;
        tradeTimestamp = doc["T"].get_int64().value();
        doc["s"].get_string(pair);
        originalQuantity = doc["q"].get_double_in_string().value();
        originalPrice = doc["p"].get_double_in_string().value();
        maker = doc["m"].get_bool().value();
        doc["c"].get_string(clientID);
        doc["S"].get_string(intermediateString);
        orderSide = returnOrderSide(intermediateString);
        lastFilledPrice = doc["L"].get_double_in_string().value();
        lastFilledQuantity = doc["l"].get_double_in_string().value();
        tradeID = doc["t"].get_int64().value();
        orderID = doc["i"].get_int64().value();
    }

    TradeLite(simdjson::ondemand::document& doc) {
        parseFields(doc);
    }
};
// Wire format: doc/payloads/streams/user-account-config-update.md
class AccountConfigUpdate {
    public:
    int64_t timestamp;
    std::string pair = "";
    int64_t leverage = 0;
    bool multiAssetMode = false;

    void parseFields(simdjson::ondemand::document& doc) {
        timestamp = doc["T"].get_int64().value();
        auto testIfLeverage = doc["ac"];
        if(testIfLeverage.error() != simdjson::NO_SUCH_FIELD){
            auto obj = testIfLeverage.get_object().value();
            obj["s"].get_string(pair);
            leverage = obj["l"].get_int64().value();
        } else {
            auto obj = doc["ai"].get_object().value();
            multiAssetMode = obj["j"].get_bool().value();
        }
    }

    AccountConfigUpdate(simdjson::ondemand::document& doc) {
        parseFields(doc);
    }
};
// Wire format: doc/payloads/streams/user-strategy-update.md
class StrategyUpdate {
    public:
    int64_t timestamp;
    std::string pair;
    int64_t strategyID;
    StrategyType strategyType;
    StrategyStatus strategyStatus;
    int64_t updateTimestamp;
    OpCode opCode;

    void parseFields(simdjson::ondemand::document& doc) {
        std::string intermediateString;
        timestamp = doc["T"].get_int64().value();
        auto obj = doc["su"].get_object().value();
        strategyID = obj["si"].get_int64().value();
        doc["st"].get_string(intermediateString);
        strategyType = returnStrategyType(intermediateString);
        doc["ss"].get_string(intermediateString);
        strategyStatus = returnStrategyStatus(intermediateString);
        doc["s"].get_string(pair);
        updateTimestamp = doc["ut"].get_int64().value();
        opCode = returnOpCode(doc["c"].get_int64().value());
    }

    StrategyUpdate(simdjson::ondemand::document& doc) {
        parseFields(doc);
    }
};
// Wire format: doc/payloads/streams/user-grid-update.md
class GridUpdate {
    public:
    int64_t timestamp;
    int64_t strategyID;
    StrategyType strategyType;
    StrategyStatus strategyStatus;
    std::string pair;
    double realizedPNL;
    double unmatchedAveragePrice;
    double unmatchedQuantity;
    double unmatchedFee;
    double matchedPNL;
    int64_t updateTimestamp;

    void parseFields(simdjson::ondemand::document& doc) {
        timestamp = doc["T"].get_int64().value();
        auto obj = doc["gu"].get_object().value();
        strategyID = obj["si"].get_int64().value();
        std::string intermediateString;
        obj["st"].get_string(intermediateString);
        strategyType = returnStrategyType(intermediateString);
        obj["ss"].get_string(intermediateString);
        strategyStatus = returnStrategyStatus(intermediateString);
        obj["s"].get_string(pair);
        realizedPNL = obj["r"].get_double_in_string().value();
        unmatchedAveragePrice = obj["up"].get_double_in_string().value();
        unmatchedQuantity = obj["uq"].get_double_in_string().value();
        unmatchedFee = obj["uf"].get_double_in_string().value();
        matchedPNL = obj["mp"].get_double_in_string().value();
        updateTimestamp = obj["ut"].get_int64().value();
    }

    GridUpdate(simdjson::ondemand::document& doc) {
        parseFields(doc);
    }
};
// Wire format: doc/payloads/streams/user-conditional-order-reject.md
class ConditionalOrderReject {
    public:
    int64_t timestamp;
    std::string pair;
    int64_t orderID;
    std::string rejectReason;

    void parseFields(simdjson::ondemand::document& doc) {
        timestamp = doc["T"].get_int64().value();
        auto obj = doc["or"].get_object().value();
        obj["s"].get_string(pair);
        orderID = obj["i"].get_int64().value();
        obj["r"].get_string(rejectReason);
    }

    ConditionalOrderReject(simdjson::ondemand::document& doc) {
        parseFields(doc);
    }
};

class PTRWrapper {
    public:
    std::unique_ptr<AccountUpdate> accountUpdate;
    std::unique_ptr<MarginCall> marginCall;
    std::unique_ptr<OrderUpdate> orderUpdate;
    std::unique_ptr<TradeLite> tradeLite;
    std::unique_ptr<AccountConfigUpdate> accountConfigUpdate;
    std::unique_ptr<StrategyUpdate> strategyUpdate;
    std::unique_ptr<GridUpdate> gridUpdate;
    std::unique_ptr<ConditionalOrderReject> conditionalOrderReject;
};
