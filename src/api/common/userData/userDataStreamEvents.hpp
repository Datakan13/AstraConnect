#pragma once
#include "utils/decimal/fixedPoint.hpp"
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
            asset.walletBalance = AstraConnect::Utils::readFixed(field["wb"]);
            asset.crossWalletBalance = AstraConnect::Utils::readFixed(field["cw"]);
            asset.balanceChange = AstraConnect::Utils::readFixed(field["bc"]);
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
            position.positionAmount = AstraConnect::Utils::readFixed(field["pa"]);
            position.entryPrice = AstraConnect::Utils::readFixed(field["ep"]);
            position.breakEvenPrice = AstraConnect::Utils::readFixed(field["bep"]);
            position.unrealizedPNL = AstraConnect::Utils::readFixed(field["up"]);
            field["mt"].get_string(marginType);
            position.marginType = returnMarginType(marginType);
            position.isolatedWalletBalance = AstraConnect::Utils::readFixed(field["iw"]);
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
    AstraConnect::Utils::Fixed crossWalletBalance;
    std::vector<MarginCallPosition> positions;

    void parseFields(simdjson::ondemand::document& doc) {
        crossWalletBalance = AstraConnect::Utils::readFixed(doc["cw"]);
        MarginCallPosition position;
        std::string positionSide;
        std::string margintype;
        auto positionsArray = doc["p"].get_array().value();
        for (auto field : positionsArray) {
            field["s"].get_string(position.pair);
            field["ps"].get_string(positionSide);
            position.positionSide = returnPositionSide(positionSide);
            position.positionAmount = AstraConnect::Utils::readFixed(field["pa"]);
            field["mt"].get_string(margintype);
            position.marginType = returnMarginType(margintype);
            position.isolatedWalletBalance = AstraConnect::Utils::readFixed(field["iw"]);
            position.markPrice = AstraConnect::Utils::readFixed(field["mp"]);
            position.unrealizedPNL = AstraConnect::Utils::readFixed(field["up"]);
            position.maintenanceMargin = AstraConnect::Utils::readFixed(field["mm"]);
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
    AstraConnect::Utils::Fixed originalQuantity;
    AstraConnect::Utils::Fixed originalPrice;   // price requested on limit orders 0 for market
    AstraConnect::Utils::Fixed averagePrice;        // average price of executed fills 
    AstraConnect::Utils::Fixed stopPrice;
    int64_t orderID;
    AstraConnect::Utils::Fixed lastFilledQuantity;
    AstraConnect::Utils::Fixed totalFilledQuantity;
    AstraConnect::Utils::Fixed lastFilledPrice;
    std::string commisionAsset;
    AstraConnect::Utils::Fixed commision;
    int64_t tradeTimestamp;
    AstraConnect::Utils::Fixed notitionalValueOfOrderB;
    AstraConnect::Utils::Fixed notitionalValueOfOrderA;
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
    AstraConnect::Utils::Fixed activationPrice = 0;
    double callBackRate = 0;
    bool priceProtection;
    AstraConnect::Utils::Fixed realizedProfit;
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
        originalQuantity = AstraConnect::Utils::readFixed(obj["q"]);
        originalPrice = AstraConnect::Utils::readFixed(obj["p"]);
        averagePrice = AstraConnect::Utils::readFixed(obj["ap"]);
        stopPrice = AstraConnect::Utils::readFixed(obj["sp"]);
        obj["x"].get_string(intermediateString);
        executionType = returnExecutionType(intermediateString);
        obj["X"].get_string(intermediateString);
        orderStatus = returnOrderStatus(intermediateString);
        orderID = obj["i"].get_int64().value();
        lastFilledQuantity = AstraConnect::Utils::readFixed(obj["l"]);
        totalFilledQuantity = AstraConnect::Utils::readFixed(obj["z"]);
        lastFilledPrice = AstraConnect::Utils::readFixed(obj["L"]);
        obj["N"].get_string(commisionAsset);
        commision = AstraConnect::Utils::readFixed(obj["n"]);
        tradeTimestamp = obj["T"].get_int64().value();
        notitionalValueOfOrderB = AstraConnect::Utils::readFixed(obj["b"]);
        notitionalValueOfOrderA = AstraConnect::Utils::readFixed(obj["a"]);
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
            activationPrice = AstraConnect::Utils::readFixed(obj["AP"]);
            callBackRate = obj["cr"].get_double_in_string().value();
        }
        priceProtection = obj["pP"].get_bool().value();
        realizedProfit = AstraConnect::Utils::readFixed(obj["rp"]);
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
    AstraConnect::Utils::Fixed originalQuantity;
    AstraConnect::Utils::Fixed originalPrice;
    bool maker;
    OrderSide orderSide;
    AstraConnect::Utils::Fixed lastFilledPrice;
    AstraConnect::Utils::Fixed lastFilledQuantity;
    int64_t tradeID;
    int64_t orderID;

    void parseFields(simdjson::ondemand::document& doc) {
        std::string intermediateString;
        tradeTimestamp = doc["T"].get_int64().value();
        doc["s"].get_string(pair);
        originalQuantity = AstraConnect::Utils::readFixed(doc["q"]);
        originalPrice = AstraConnect::Utils::readFixed(doc["p"]);
        maker = doc["m"].get_bool().value();
        doc["c"].get_string(clientID);
        doc["S"].get_string(intermediateString);
        orderSide = returnOrderSide(intermediateString);
        lastFilledPrice = AstraConnect::Utils::readFixed(doc["L"]);
        lastFilledQuantity = AstraConnect::Utils::readFixed(doc["l"]);
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
    AstraConnect::Utils::Fixed realizedPNL;
    AstraConnect::Utils::Fixed unmatchedAveragePrice;
    AstraConnect::Utils::Fixed unmatchedQuantity;
    AstraConnect::Utils::Fixed unmatchedFee;
    AstraConnect::Utils::Fixed matchedPNL;
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
        realizedPNL = AstraConnect::Utils::readFixed(obj["r"]);
        unmatchedAveragePrice = AstraConnect::Utils::readFixed(obj["up"]);
        unmatchedQuantity = AstraConnect::Utils::readFixed(obj["uq"]);
        unmatchedFee = AstraConnect::Utils::readFixed(obj["uf"]);
        matchedPNL = AstraConnect::Utils::readFixed(obj["mp"]);
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
