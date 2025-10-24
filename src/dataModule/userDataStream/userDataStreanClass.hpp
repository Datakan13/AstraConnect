#pragma once 
#include <string>
#include <vector>
#include "helperClassesUserDataStreamClass.hpp"
#include "userDataStreamHelperFunctions.hpp"
/*
Payload example for account update 
{
  "e": "ACCOUNT_UPDATE",				// Event Type
  "E": 1564745798939,            		// Event Time
  "T": 1564745798938 ,           		// Transaction
  "a":                          		// Update Data
    {
      "m":"ORDER",						// Event reason type
      "B":[                     		// Balances
        {
          "a":"USDT",           		// Asset
          "wb":"122624.12345678",    	// Wallet Balance
          "cw":"100.12345678",			// Cross Wallet Balance
          "bc":"50.12345678"			// Balance Change except PnL and Commission
        },
        {
          "a":"BUSD",           
          "wb":"1.00000000",
          "cw":"0.00000000",         
          "bc":"-49.12345678"
        }
      ],
      "P":[
        {
          "s":"BTCUSDT",          	// Symbol
          "pa":"0",               	// Position Amount
          "ep":"0.00000",            // Entry Price
          "bep":"0",                // breakeven price 
		  "cr":"200",             	// (Pre-fee) Accumulated Realized
          "up":"0",						// Unrealized PnL
          "mt":"isolated",				// Margin Type
          "iw":"0.00000000",			// Isolated Wallet (if isolated position)
          "ps":"BOTH"					// Position Side
        }，
        {
        	"s":"BTCUSDT",
        	"pa":"20",
        	"ep":"6563.66500",
        	"bep":"0",                // breakeven price
        	"cr":"0",
        	"up":"2850.21200",
        	"mt":"isolated",
        	"iw":"13200.70726908",
        	"ps":"LONG"
      	 },
        {
        	"s":"BTCUSDT",
        	"pa":"-10",
        	"ep":"6563.86000",
        	"bep":"6563.6",          // breakeven price
        	"cr":"-45.04000000",
        	"up":"-1423.15600",
        	"mt":"isolated",
        	"iw":"6570.42511771",
        	"ps":"SHORT"
        }
      ]
    }
}
*/
class AccountUpdate {
    public:
    AccountUpdateType eventType;
    std::vector<AssetBalance> assets;
    std::vector<PositionBalance> positions;

    inline void parseFields(simdjson::fallback::ondemand::document_stream::iterator::value_type& doc) {
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
        auto positionArray = obj["p"].get_array().value();
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
    
    AccountUpdate(simdjson::fallback::ondemand::document_stream::iterator::value_type& doc) {
        parseFields(doc);
    };
};
/*
payload for margin call 
{
    "e":"MARGIN_CALL",    	// Event Type
    "E":1587727187525,		// Event Time
    "cw":"3.16812045",		// Cross Wallet Balance. Only pushed with crossed position margin call
    "p":[					// Position(s) of Margin Call
      {
        "s":"ETHUSDT",		// Symbol
        "ps":"LONG",		// Position Side
        "pa":"1.327",		// Position Amount
        "mt":"CROSSED",		// Margin Type
        "iw":"0",			// Isolated Wallet (if isolated position)
        "mp":"187.17127",	// Mark Price
        "up":"-1.166074",	// Unrealized PnL
        "mm":"1.614445"		// Maintenance Margin Required
      }
    ]
}  
*/
class MarginCall {
    public:
    double crossWalletBalance;
    std::vector<MarginCallPosition> positions;

    void parseFields(simdjson::fallback::ondemand::document_stream::iterator::value_type& doc) {
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
            position.maintanceMargin = field["mm"].get_double_in_string().value();
            positions.push_back(position);
        }
    }
    MarginCall(simdjson::fallback::ondemand::document_stream::iterator::value_type& doc) {
        parseFields(doc);
    }
};
/*
payload example for order update
{
  "e":"ORDER_TRADE_UPDATE",		   // Event Type
  "E":1568879465651,			       // Event Time
  "T":1568879465650,			       // Transaction Time
  "o":{								
    "s":"BTCUSDT",			         // Symbol
    "c":"TEST",				           // Client Order Id
      // special client order id:
      // starts with "autoclose-": liquidation order
      // "adl_autoclose": ADL auto close order
      // "settlement_autoclose-": settlement order for delisting or delivery
    "S":"SELL",					         // Side
    "o":"TRAILING_STOP_MARKET",	 // Order Type
    "f":"GTC",					         // Time in Force
    "q":"0.001",				         // Original Quantity
    "p":"0",					           // Original Price
    "ap":"0",					           // Average Price
    "sp":"7103.04",				       // Stop Price. Please ignore with TRAILING_STOP_MARKET order
    "x":"NEW",					         // Execution Type
    "X":"NEW",					         // Order Status
    "i":8886774,				         // Order Id
    "l":"0",					           // Order Last Filled Quantity
    "z":"0",					           // Order Filled Accumulated Quantity
    "L":"0",					           // Last Filled Price
    "N":"USDT",            	     // Commission Asset
    "n":"0",               	     // Commission
    "T":1568879465650,			     // Order Trade Time
    "t":0,			        	       // Trade Id
    "b":"0",			    	         // Bids Notional
    "a":"9.91",					         // Ask Notional
    "m":false,					         // Is this trade the maker side?
    "R":false,					         // Is this reduce only
    "wt":"CONTRACT_PRICE", 		   // Stop Price Working Type
    "ot":"TRAILING_STOP_MARKET", // Original Order Type
    "ps":"LONG",					       // Position Side
    "cp":false,						       // If Close-All, pushed with conditional order
    "AP":"7476.89",				       // Activation Price, only puhed with TRAILING_STOP_MARKET order
    "cr":"5.0",					         // Callback Rate, only puhed with TRAILING_STOP_MARKET order
    "pP": false,                 // If price protection is turned on
    "si": 0,                     // ignore
    "ss": 0,                     // ignore
    "rp":"0",	   					       // Realized Profit of the trade
    "V":"EXPIRE_TAKER",          // STP mode
    "pm":"OPPONENT",             // Price match mode
    "gtd":0,                     // TIF GTD order auto cancel time
    "er":"0"                     // Expire Reason 
  }
}
*/
class OrderUpdate {
    public:
    std::string pair;
    std::string clientID;
    double originalQuantity;
    double originalPrice = 0;   // price requested on limit orders 0 for market
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
    double activationPrice;
    double callBackRate;
    bool priceProtection;
    double realizedProfit;
    int64_t GTDExprationTimestamp;

    void parseFields(simdjson::fallback::ondemand::document_stream::iterator::value_type& doc) {
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

    OrderUpdate(simdjson::fallback::ondemand::document_stream::iterator::value_type& doc) {
        parseFields(doc);
    }
};
/*
payload example for trade lite
{
  "e":"TRADE_LITE",             // Event Type
  "E":1721895408092,            // Event Time
  "T":1721895408214,            // Transaction Time                          
  "s":"BTCUSDT",                // Symbol
  "q":"0.001",                  // Original Quantity
  "p":"0",                      // Original Price
  "m":false,                    // Is this trade the maker side?
  "c":"z8hcUoOsqEdKMeKPSABslD", // Client Order Id
      // special client order id:
      // starts with "autoclose-": liquidation order
      // "adl_autoclose": ADL auto close order
      // "settlement_autoclose-": settlement order for delisting or delivery
  "S":"BUY",                   // Side
  "L":"64089.20",              // Last Filled Price
  "l":"0.040",                 // Order Last Filled Quantity
  "t":109100866,               // Trade Id
  "i":8886774,                // Order Id
}
*/
class TradeLite {
    int64_t tradeTimestamp;
    int64_t clientID;
    std::string pair;
    double originalQuantity;
    double originalPrice;
    bool maker;
    OrderSide orderSide;
    double lastFilledPrice;
    double lastFilledQuantity;
    int64_t tradeID;
    int64_t orderID;
};
/*
payload example for account configuration update
first:
{
    "e":"ACCOUNT_CONFIG_UPDATE",       // Event Type
    "E":1611646737479,		           // Event Time
    "T":1611646737476,		           // Transaction Time
    "ac":{								
    "s":"BTCUSDT",					   // symbol
    "l":25						       // leverage
     
    }
}  
 
second:
{
    "e":"ACCOUNT_CONFIG_UPDATE",       // Event Type
    "E":1611646737479,		           // Event Time
    "T":1611646737476,		           // Transaction Time
    "ai":{							   // User's Account Configuration
    "j":true						   // Multi-Assets Mode
    }
}  

*/
class AccountConfigUpdate {
    int64_t timestamp;
    std::string pair;
    int64_t leverage;
    bool multiAssetMode;
};
/*
payload example for strategy update
{
	"e": "STRATEGY_UPDATE", // Event Type
	"T": 1669261797627, // Transaction Time
	"E": 1669261797628, // Event Time
	"su": {
			"si": 176054594, // Strategy ID
			"st": "GRID", // Strategy Type
			"ss": "NEW", // Strategy Status
			"s": "BTCUSDT", // Symbol
			"ut": 1669261797627, // Update Time
			"c": 8007 // opCode
		}
}
*/
class StrategyUpdate {
    int64_t timestamp;
    std::string pair;
    StrategyType strategyType;
    StrategyStatus strategyStatus;
    int64_t updateTimestamp;
    OpCode opCode;
};
/*
payload example for grid update
{
	"e": "GRID_UPDATE", // Event Type
	"T": 1669262908216, // Transaction Time
	"E": 1669262908218, // Event Time
	"gu": { 
			"si": 176057039, // Strategy ID
			"st": "GRID", // Strategy Type
			"ss": "WORKING", // Strategy Status
			"s": "BTCUSDT", // Symbol
			"r": "-0.00300716", // Realized PNL
			"up": "16720", // Unmatched Average Price
			"uq": "-0.001", // Unmatched Qty
			"uf": "-0.00300716", // Unmatched Fee
			"mp": "0.0", // Matched PNL
			"ut": 1669262908197 // Update Time
		   }
}
*/
class GridUpdate {
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
};
/*
payload example for Conditional order rejection
{
    "e":"CONDITIONAL_ORDER_TRIGGER_REJECT",      // Event Type
    "E":1685517224945,      // Event Time
    "T":1685517224955,      // me message send Time
    "or":{
      "s":"ETHUSDT",      // Symbol   
      "i":155618472834,      // orderId
      "r":"Due to the order could not be filled immediately, the FOK order has been rejected. The order will not be recorded in the order history",      // reject reason
     }
}  
*/
class ConditionalOrderReject {
    int64_t timestamp;
    std::string pair;
    int64_t orderID;
    std::string rejectReason;
};

class PTRWrapper {
    public:
    AccountUpdate* accountUpdate = nullptr;
    MarginCall* marginCall = nullptr;
    OrderUpdate* orderUpdate = nullptr;
    TradeLite* tradeLite = nullptr;
    AccountConfigUpdate* accountConfigUpdate = nullptr;
    StrategyUpdate* strategyUpdate = nullptr;
    GridUpdate* gridUpdate = nullptr;
    ConditionalOrderReject* conditionalOrderReject = nullptr;
};

class UserDataStream {
    EventType event;
    int64_t timestamp;
    PTRWrapper* ptrWrapper;

public:
    auto returnPtr() const -> void* {
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
        : event(event_), timestamp(timestamp_) {}

    ~UserDataStream() {
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



