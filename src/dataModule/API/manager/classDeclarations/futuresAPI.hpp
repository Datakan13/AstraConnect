#pragma once
#include "dataModule/API/manager/apiManager.hpp"
#include "dataModule/API/helperClasses/StreamHolder.hpp"
#include "dataModule/threadSafeParser.hpp"
#include "dataModule/API/APIError.hpp"

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

        FuturesAPI(APIManager& base_) : futures(base_.ioc,base_.ctx,base_.hostFutures){
        }

    };
