#pragma once
#include "boost_include_helpers/includeBoost.hpp"
#include <vector>
#include "candle.hpp"
#include "simdjson.h"
#include "openInterest.hpp"
#include "APIHelpers.hpp"
#include "threadSafeParser.hpp"
#include <utility>
#include <AstraLib/Buffers/atomicRingBuffer.hpp>
class APIManager {
    const std::string hostSpot = "api.binance.com";
    const std::string hostFutures = "fapi.binance.com";

    boost::asio::io_context& ioc;
    boost::asio::ssl::context& ctx;

    StreamHolder futures;
    StreamHolder spot;

    ThreadSafeParser parserSpot;
    ThreadSafeParser parserFutures;


    /*
    Payload example for kline candle stick data
    [
        [
            1499040000000,      // Kline open time [index: 0]
            "0.01634790",       // Open price [index: 1]
            "0.80000000",       // High price [index: 2]
            "0.01575800",       // Low price [index: 3]
            "0.01577100",       // Close price [index: 4]
            "148976.11427815",  // Volume [index: 5]
            1499644799999,      // Kline Close time [index: 6]
            "2434.19055334",    // Quote asset volume [index: 7]
            308,                // Number of trades [index: 8]
            "1756.87402397",    // Taker buy base asset volume [index: 9]
            "28.46694368",      // Taker buy quote asset volume [index: 10]
            "0"                 // Unused field, ignore. [index: 11]
        ]
    ]
    */

    public:
    // pair: e.g. "BTCUSDT" timeframe: e.g. "5m", "1h"
    void fetchCandles(AstraLib::Buffers::AtomicRingBuffer<Candle,2048>& outputBuff, const std::string pair,  const std::string timeframe) {

        const std::string target = "/api/v3/klines?symbol="+ pair+"&interval=" + timeframe;
        auto json = spot.sendRequest(target);

        ThreadSafeParserRAII parser(parserSpot);
        auto candleArray = parser.parser.iterate(json);
        int index = 0;
        Candle outCandle;
        for(auto candle : candleArray) {
            auto iterate = candle.value().get_array().value();
            for ( auto field : iterate) {
                switch (index)
                {
                case 0:
                    outCandle.timestampOpen = field.value().get_int64().value();
                    break;
                case 1:
                    outCandle.open = field.value().get_double_in_string().value();
                    break;
                case 2:
                    outCandle.high = field.value().get_double_in_string().value();
                    break;
                case 3:
                    outCandle.low = field.value().get_double_in_string().value();
                    break;
                case 4:
                    outCandle.close = field.value().get_double_in_string().value();
                    break;
                case 5:
                    outCandle.volume = field.value().get_double_in_string().value();
                    break;
                case 6:
                    outCandle.timestampClose = field.value().get_int64().value();
                    break;
                case 7:
                    outCandle.quoteVolume = field.value().get_double_in_string().value();
                    break;
                default:
                    break;
                }
                index++;
            }
            outputBuff.noMoveEnqueue(outCandle);
            index = 0;
        }
    }


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
    void fetchOpenInterestHist(AstraLib::Buffers::AtomicRingBuffer<OpenInterest,1024>& outputBuff, const std::string pair, const std::string timeframe) {

        const std::string target = "/futures/data/openInterestHist?symbol=" + pair + "&period=" + timeframe ;
        auto json = futures.sendRequest(target);

        ThreadSafeParserRAII parser(parserFutures);
        auto interestArray = parser.parser.iterate(json);

        OpenInterest interestFrameHolder;
        for(auto interestFrame : interestArray) {
            interestFrameHolder.totalInterest = interestFrame["sumOpenInterest"].get_double_in_string().value();
            interestFrameHolder.totalInterestValue = interestFrame["sumOpenInterestValue"].get_double_in_string().value();
            interestFrameHolder.circulation = interestFrame["CMCCirculatingSupply"].get_double_in_string().value();
            interestFrameHolder.timestamp = interestFrame["timestamp"].get_int64().value();
            outputBuff.noMoveEnqueue(interestFrameHolder);
        }

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
    CurrentOpenInterest fetchOpenInterestCurrent( const std::string pair) {
        const std::string target = "/fapi/v1/openInterest?symbol=" + pair ;
        auto json = futures.sendRequest(target);

        ThreadSafeParserRAII parser(parserFutures);
        auto interestCurrent = parser.parser.iterate(json);
        
        int index = 0;
        CurrentOpenInterest interestHolder;
        interestHolder.openInterest = interestCurrent.find_field("openInterest").value().get_double_in_string().value();
        interestHolder.timestamp = interestCurrent.find_field("time").value().get_int64().value();
        return interestHolder;
    }

    public:
    APIManager(boost::asio::io_context& ioc_, boost::asio::ssl::context& ctx_) :
    ioc(ioc_), ctx(ctx_),
    futures(ioc,ctx,hostFutures),spot(ioc,ctx,hostSpot) {

    
    }


};