#pragma once
#include "dataModule/API/manager/apiManager.hpp"
#include "dataModule/API/helperClasses/StreamHolder.hpp"
#include "dataModule/threadSafeParser.hpp"
#include "dataModule/API/APIError.hpp"
#include "dataModule/dataTypes/exchangeInfo.hpp"
#include "dataModule/API/helperClasses/orderbookSnapshotIncoming.hpp"
// error handling DONE

class APIManager::FuturesAPI{
        StreamHolder futures;
        ThreadSafeParser parserFutures;
        public:
        /*
            Payload example for open interest data
            [
                { 
                    "symbol":"BTCUSDT",
                    "sumOpenInterest":"20403.63700000",  // total open interest 
                    "sumOpenInterestValue": "150570784.07809979",   // total open interest value
                    "CMCCirculatingSupply": "165880.538", // circulating supply provided by CMC
                    "timestamp":"1583127900000" // WARNING: document claims this is a string however the actual payload is : "timestamp":1761051300000
                },     
                { 
                    "symbol":"BTCUSDT",
                    "sumOpenInterest":"20401.36700000",
                    "sumOpenInterestValue":"149940752.14464448",
                    "CMCCirculatingSupply": "165900.14853",
                    "timestamp":"1583128200000"   // WARNING: document claims this is a string however the actual payload is : "timestamp":1761051300000
                },   
            ]
        */
    
        // Pair: Must be all capital e.g. "BTCUSDT"
        FetchError fetchOpenInterestHist(AstraLib::Buffers::AtomicRingBuffer<OpenInterest,1024>& outputBuff, const std::string pair, const std::string timeframe) {

            const std::string target = "/futures/data/openInterestHist?symbol=" + pair + "&period=" + timeframe;
            simdjson::padded_string json;
            try{
                json = simdjson::padded_string{futures.sendRequest(target)};
            } catch(std::runtime_error& e) {
                return FetchError(APIError::BOOST_ERROR,std::string(e.what()));
            } catch(std::exception& e) {
                return FetchError(APIError::UNKNOWN,std::string(e.what()));
            }

            ThreadSafeParserRAII parser(parserFutures);
            auto interestArray = parser.parser.iterate(json);
            try {
                OpenInterest interestFrameHolder;
                for(auto interestFrame : interestArray) {
                    interestFrameHolder.totalInterest = interestFrame["sumOpenInterest"].get_double_in_string().value();
                    interestFrameHolder.totalInterestValue = interestFrame["sumOpenInterestValue"].get_double_in_string().value();
                    interestFrameHolder.circulation = interestFrame["CMCCirculatingSupply"].get_double_in_string().value();
                    interestFrameHolder.timestamp = interestFrame["timestamp"].get_int64().value();
                    outputBuff.noMoveEnqueue(interestFrameHolder);
                }
            } catch(std::exception& e) {
                outputBuff.clearBuffer();
                try {
                    if(interestArray.find_field("error")) {
                        auto obj = interestArray["error"].get_object().value();
                        return getErrorFromSimdjson(obj);
                    }
                } catch(std::exception& e) {
                    return FetchError(APIError::BAD_FIELD,std::string{"Failed to parse a field. error: " + std::string(e.what())});
                }
            }
            return FetchError(APIError::SUCCESS);
        }

        /*
        Payload example for Open Interest current 
        {
            "openInterest": "10659.509", 
            "symbol": "BTCUSDT",
            "time": 1589437530011   // Transaction time
        }
        */

        // Returns current interest and timeframe 
        FetchError fetchOpenInterestCurrent(CurrentOpenInterest& ref ,const std::string pair) {
            const std::string target = "/fapi/v1/openInterest?symbol=" + pair;
            simdjson::padded_string json;
            try{
                json = simdjson::padded_string{futures.sendRequest(target)};
            } catch(std::runtime_error& e) {
               return  FetchError(APIError::BOOST_ERROR,std::string(e.what()));
            } catch(std::exception& e) {
                return FetchError(APIError::UNKNOWN,std::string(e.what()));
            }

            ThreadSafeParserRAII parser(parserFutures);
            auto interestCurrent = parser.parser.iterate(json);
            
            try {
                ref.openInterest = interestCurrent.find_field("openInterest").value().get_double_in_string().value();
                ref.timestamp = interestCurrent.find_field("time").value().get_int64().value();
            } catch(std::exception& e) {
                return FetchError(APIError::BAD_FIELD,std::string("Failed to parse a field. error: " + std::string(e.what())));
            }

            return FetchError(APIError::SUCCESS);
        }

        /*
        payload example for exchange info
        {
        "exchangeFilters": [],
        "rateLimits": [
            {
                "interval": "MINUTE",
                "intervalNum": 1,
                "limit": 2400,
                "rateLimitType": "REQUEST_WEIGHT" 
            },
            {
                "interval": "MINUTE",
                "intervalNum": 1,
                "limit": 1200,
                "rateLimitType": "ORDERS"
            }
        ],
        "serverTime": 1565613908500,    // Ignore please. If you want to check current server time, please check via "GET /fapi/v1/time"
        "assets": [ // assets information
            {
                "asset": "BTC",
                "marginAvailable": true, // whether the asset can be used as margin in Multi-Assets mode
                "autoAssetExchange": "-0.10" // auto-exchange threshold in Multi-Assets margin mode
            },
            {
                "asset": "USDT",
                "marginAvailable": true,
                "autoAssetExchange": "0"
            },
            {
                "asset": "BNB",
                "marginAvailable": false,
                "autoAssetExchange": null
            }
        ],
        "symbols": [
            {
                "symbol": "BLZUSDT",
                "pair": "BLZUSDT",
                "contractType": "PERPETUAL",
                "deliveryDate": 4133404800000,
                "onboardDate": 1598252400000,
                "status": "TRADING",
                "maintMarginPercent": "2.5000",   // ignore
                "requiredMarginPercent": "5.0000",  // ignore
                "baseAsset": "BLZ", 
                "quoteAsset": "USDT",
                "marginAsset": "USDT",
                "pricePrecision": 5,	// please do not use it as tickSize
                "quantityPrecision": 0, // please do not use it as stepSize
                "baseAssetPrecision": 8,
                "quotePrecision": 8, 
                "underlyingType": "COIN",
                "underlyingSubType": ["STORAGE"],
                "settlePlan": 0,
                "triggerProtect": "0.15", // threshold for algo order with "priceProtect"
                "filters": [
                    {
                        "filterType": "PRICE_FILTER",
                        "maxPrice": "300",
                        "minPrice": "0.0001", 
                        "tickSize": "0.0001"
                    },
                    {
                        "filterType": "LOT_SIZE", 
                        "maxQty": "10000000",
                        "minQty": "1",
                        "stepSize": "1"
                    },
                    {
                        "filterType": "MARKET_LOT_SIZE",
                        "maxQty": "590119",
                        "minQty": "1",
                        "stepSize": "1"
                    },
                    {
                        "filterType": "MAX_NUM_ORDERS",
                        "limit": 200
                    },
                    {
                        "filterType": "MAX_NUM_ALGO_ORDERS",
                        "limit": 10
                    },
                    {
                        "filterType": "MIN_NOTIONAL",
                        "notional": "5.0", 
                    },
                    {
                        "filterType": "PERCENT_PRICE",
                        "multiplierUp": "1.1500",
                        "multiplierDown": "0.8500",
                        "multiplierDecimal": "4"
                    }
                ],
                "OrderType": [
                    "LIMIT",
                    "MARKET",
                    "STOP",
                    "STOP_MARKET",
                    "TAKE_PROFIT",
                    "TAKE_PROFIT_MARKET",
                    "TRAILING_STOP_MARKET" 
                ],
                "timeInForce": [
                    "GTC", 
                    "IOC", 
                    "FOK", 
                    "GTX" 
                ],
                "liquidationFee": "0.010000",	// liquidation fee rate
                "marketTakeBound": "0.30",	// the max price difference rate( from mark price) a market order can make
            }
        ],
        "timezone": "UTC" 
        }
        */

        FetchError fetchExchangeInfoForPair(std::string pair,PairInfo& out) {
            const std::string target = "/fapi/v1/exchangeInfo";
            simdjson::padded_string json;
            try{
                json = simdjson::padded_string{futures.sendRequest(target)};
            } catch(std::runtime_error& e) {
               return  FetchError(APIError::BOOST_ERROR,std::string(e.what()));
            } catch(std::exception& e) {
                return FetchError(APIError::UNKNOWN,std::string(e.what()));
            }
            ThreadSafeParserRAII parser(parserFutures);
            auto doc = parser.parser.iterate(json);
            std::string pair_;
            std::string field_;
            for(auto symbol : doc["symbols"]) {
                auto obj = symbol.get_object().value();
                obj["symbol"].get_string(pair_);
                if(pair_ == pair) {
                    for(auto field : obj["filters"]) {
                        field["filterType"].get_string(field_);
                        if(field_ == "PRICE_FILTER") {
                            out.maxPrice = field["maxPrice"].get_double_in_string().value();
                            out.minPrice = field["minPrice"].get_double_in_string().value();
                            out.tickSize = field["tickSize"].get_double_in_string().value();
                        }
                        if(field_ == "LOT_SIZE") {
                            out.maxQuantity = field["maxQty"].get_double_in_string().value();
                            out.minQuantity = field["minQty"].get_double_in_string().value();
                            out.stepsize = field["stepSize"].get_double_in_string().value();
                        }
                        if(field_ == "MIN_NOTIONAL") {
                            out.minNotional = field["notional"].get_double_in_string().value();
                        }
                    }
                }
            }
            return FetchError(APIError::UNKNOWN);
        } 

        FetchError fetchOrderbookSnapshot(std::string pair, OrderbookSnapshotIncoming& out) {
            std::cout << "snapshot ordered" << std::endl;
            const std::string target = "/fapi/v1/depth?symbol="+pair+"&limit=1000";
            simdjson::padded_string json;
            try{
                json = simdjson::padded_string{futures.sendRequest(target)};
            } catch(std::runtime_error& e) {
               return  FetchError(APIError::BOOST_ERROR,std::string(e.what()));
            } catch(std::exception& e) {
                return FetchError(APIError::UNKNOWN,std::string(e.what()));
            }
            ThreadSafeParserRAII parser(parserFutures);
            auto doc = parser.parser.iterate(json);
            out.updateId = doc["lastUpdateId"].get_int64();
            for(auto bid : doc["bids"]){
                auto ary = bid.value().get_array();
                double price = 0;
                double volume;
                for(auto val : ary){
                    if(price != 0){
                        volume = val.value().get_double_in_string().value();
                    } else {
                        price = val.value().get_double_in_string().value();
                    }
                }
                out.registerEntry(price,volume,true);
            }
            for(auto ask :  doc["asks"]){
                auto ary = ask.value().get_array();
                double price = 0;
                double volume;
                for(auto val : ary){
                    if(price != 0){
                        volume = val.value().get_double_in_string().value();
                    } else {
                        price = val.value().get_double_in_string().value();
                    }
                }
                out.registerEntry(price,volume,false);
            }
        }

        FuturesAPI(APIManager& base_) : futures(base_.ioc,base_.ctx,base_.hostFutures){
        }

    };
