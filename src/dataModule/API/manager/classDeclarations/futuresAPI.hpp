#pragma once
#include "dataModule/API/manager/apiManager.hpp"
#include "dataModule/API/helperClasses/StreamHolder.hpp"
#include "dataModule/threadSafeParser.hpp"

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

        FuturesAPI(APIManager& base_) : futures(base_.ioc,base_.ctx,base_.hostFutures){
        }

    };
