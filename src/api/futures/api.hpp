#pragma once
#include "manager/apiManager.hpp"

#include "core/streams/streamHolder.hpp"
#include "core/parsing/threadSafeParser.hpp"

#include "api/common/error/apiError.hpp"
#include "api/common/error/fetchError.hpp"

#include "model/exchangeInfo.hpp"
#include "model/openInterest.hpp"
#include "model/candle.hpp"

#include "api/orderbook/orderbookSnapshotIncoming.hpp"

class APIManager::Futures::API{
        StreamHolder futures;
        ThreadSafeParser parserFutures;
        public:
        // Wire format: doc/payloads/rest/futures-klines.md

        // pair: e.g. "BTCUSDT" timeframe: e.g. "5m", "1h"
        FetchError fetchCandles(AstraLib::Buffers::AtomicRingBuffer<Candle,2048>& outputBuff, const std::string pair,  const std::string timeframe) {

            const std::string target = "/fapi/v1/klines?symbol="+ pair+"&interval=" + timeframe;
            StreamData data = futures.sendRequest(target);
            if(!data) return FetchError(APIError::BOOST_ERROR,data.ec.message());

            ThreadSafeParserRAII parser(parserFutures);
            simdjson::ondemand::document candleArray;
            if(FetchError err = iterateResponse(parser.parser,data.string,candleArray)) return err;
            int index = 0;
            Candle outCandle;
            try {
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
                    outputBuff.enqueue(outCandle);
                    index = 0;
                }
            } catch(std::exception& e) {
                outputBuff.clearBuffer();
                return FetchError(APIError::BAD_FIELD,std::string{"Failed to parse a field. error: " + std::string(e.what())});
            }
            
            return FetchError(APIError::SUCCESS);
        }

        // Wire format: doc/payloads/rest/futures-open-interest-hist.md
    
        // Pair: Must be all capital e.g. "BTCUSDT"
        FetchError fetchOpenInterestHist(AstraLib::Buffers::AtomicRingBuffer<OpenInterest,1024>& outputBuff, const std::string pair, const std::string timeframe) {

            const std::string target = "/futures/data/openInterestHist?symbol=" + pair + "&period=" + timeframe;
            StreamData data = futures.sendRequest(target);
            if(!data) return FetchError(APIError::BOOST_ERROR,data.ec.message());

            ThreadSafeParserRAII parser(parserFutures);
            simdjson::ondemand::document interestArray;
            if(FetchError err = iterateResponse(parser.parser,data.string,interestArray)) return err;
            try {
                OpenInterest interestFrameHolder;
                for(auto interestFrame : interestArray) {
                    interestFrameHolder.totalInterest = interestFrame["sumOpenInterest"].get_double_in_string().value();
                    interestFrameHolder.totalInterestValue = interestFrame["sumOpenInterestValue"].get_double_in_string().value();
                    interestFrameHolder.circulation = interestFrame["CMCCirculatingSupply"].get_double_in_string().value();
                    interestFrameHolder.timestamp = interestFrame["timestamp"].get_int64().value();
                    outputBuff.enqueue(interestFrameHolder);
                }
            } catch(std::exception& e) {
                outputBuff.clearBuffer();
                return FetchError(APIError::BAD_FIELD,std::string{"Failed to parse a field. error: " + std::string(e.what())});
            }
            return FetchError(APIError::SUCCESS);
        }

        // Wire format: doc/payloads/rest/futures-open-interest.md

        // Returns current interest and timeframe 
        FetchError fetchOpenInterestCurrent(CurrentOpenInterest& ref ,const std::string pair) {
            const std::string target = "/fapi/v1/openInterest?symbol=" + pair;
            StreamData data = futures.sendRequest(target);
            if(!data) return FetchError(APIError::BOOST_ERROR,data.ec.message());

            ThreadSafeParserRAII parser(parserFutures);
            simdjson::ondemand::document interestCurrent;
            if(FetchError err = iterateResponse(parser.parser,data.string,interestCurrent)) return err;
            
            try {
                ref.openInterest = interestCurrent.find_field("openInterest").value().get_double_in_string().value();
                ref.timestamp = interestCurrent.find_field("time").value().get_int64().value();
            } catch(std::exception& e) {
                return FetchError(APIError::BAD_FIELD,std::string("Failed to parse a field. error: " + std::string(e.what())));
            }

            return FetchError(APIError::SUCCESS);
        }

        // Wire format: doc/payloads/rest/futures-exchange-info.md

        FetchError fetchExchangeInfoForPair(std::string pair,PairInfo& out) {
            const std::string target = "/fapi/v1/exchangeInfo";
            StreamData data = futures.sendRequest(target);
            if(!data) return FetchError(APIError::BOOST_ERROR,data.ec.message());
            ThreadSafeParserRAII parser(parserFutures);
            simdjson::ondemand::document doc;
            if(FetchError err = iterateResponse(parser.parser,data.string,doc)) return err;
            std::string pair_;
            std::string field_;
            try {
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
                        return FetchError(APIError::SUCCESS);
                    }
                }
            } catch(std::exception& e) {
                return FetchError(APIError::BAD_FIELD,std::string{"Failed to parse a field. error: " + std::string(e.what())});
            }
            return FetchError(APIError::BAD_SYMBOL,"Pair not found in exchange info: " + pair);
        } 

        FetchError fetchOrderbookSnapshot(std::string pair, OrderbookSnapshotIncoming& out) {
            const std::string target = "/fapi/v1/depth?symbol="+pair+"&limit=1000";
            // This fills `out` from scratch; without the reset a second call appends
            // to the previous snapshot and the read cursors never rewind.
            out.reset();
            StreamData data = futures.sendRequest(target);
            if(!data) return FetchError(APIError::BOOST_ERROR,data.ec.message());
            ThreadSafeParserRAII parser(parserFutures);
            simdjson::ondemand::document doc;
            if(FetchError err = iterateResponse(parser.parser,data.string,doc)) return err;
            try {
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
            } catch(std::exception& e) {
                // A half-filled snapshot would be applied as if it were the whole book
                out.reset();
                return FetchError(APIError::BAD_FIELD,std::string{"Failed to parse a field. error: " + std::string(e.what())});
            }
            return FetchError(APIError::SUCCESS);
        }

        API(APIManager& base_) : futures(base_.ioc,base_.ctx,base_.hostFutures){
        }

    };
