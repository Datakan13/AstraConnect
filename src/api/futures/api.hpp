#pragma once
#include "manager/apiManager.hpp"

#include "core/streams/streamHolder.hpp"
#include "core/parsing/threadSafeParser.hpp"

#include "api/common/error/apiError.hpp"
#include "api/common/error/fetchError.hpp"

#include "model/exchangeInfo.hpp"
#include "model/openInterest.hpp"
#include "model/candle.hpp"
#include "utils/decimal/fixedPoint.hpp"

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
            if(FetchError err = iterateResponse(parser.parser,data.string,candleArray,data.httpStatus)) return err;
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
                            outCandle.open = AstraConnect::Utils::readFixed(field.value());
                            break;
                        case 2:
                            outCandle.high = AstraConnect::Utils::readFixed(field.value());
                            break;
                        case 3:
                            outCandle.low = AstraConnect::Utils::readFixed(field.value());
                            break;
                        case 4:
                            outCandle.close = AstraConnect::Utils::readFixed(field.value());
                            break;
                        case 5:
                            outCandle.volume = AstraConnect::Utils::readFixed128(field.value());
                            break;
                        case 6:
                            outCandle.timestampClose = field.value().get_int64().value();
                            break;
                        case 7:
                            outCandle.quoteVolume = AstraConnect::Utils::readFixed128(field.value());
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
            if(FetchError err = iterateResponse(parser.parser,data.string,interestArray,data.httpStatus)) return err;
            try {
                OpenInterest interestFrameHolder;
                for(auto interestFrame : interestArray) {
                    interestFrameHolder.totalInterest = AstraConnect::Utils::readFixed128(interestFrame["sumOpenInterest"]);
                    interestFrameHolder.totalInterestValue = AstraConnect::Utils::readFixed128(interestFrame["sumOpenInterestValue"]);
                    interestFrameHolder.circulation = AstraConnect::Utils::readFixed128(interestFrame["CMCCirculatingSupply"]);
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
            if(FetchError err = iterateResponse(parser.parser,data.string,interestCurrent,data.httpStatus)) return err;
            
            try {
                ref.openInterest = AstraConnect::Utils::readFixed128(interestCurrent.find_field("openInterest"));
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
            if(FetchError err = iterateResponse(parser.parser,data.string,doc,data.httpStatus)) return err;
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
                                out.maxPrice = AstraConnect::Utils::readFixed(field["maxPrice"]);
                                out.minPrice = AstraConnect::Utils::readFixed(field["minPrice"]);
                                out.tickSize = AstraConnect::Utils::readFixed(field["tickSize"]);
                            }
                            if(field_ == "LOT_SIZE") {
                                out.maxQuantity = AstraConnect::Utils::readFixed(field["maxQty"]);
                                out.minQuantity = AstraConnect::Utils::readFixed(field["minQty"]);
                                out.stepsize = AstraConnect::Utils::readFixed(field["stepSize"]);
                            }
                            if(field_ == "MIN_NOTIONAL") {
                                out.minNotional = AstraConnect::Utils::readFixed(field["notional"]);
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
            if(FetchError err = iterateResponse(parser.parser,data.string,doc,data.httpStatus)) return err;
            try {
                out.updateId = doc["lastUpdateId"].get_int64();
                for(auto bid : doc["bids"]){
                    auto ary = bid.value().get_array();
                    AstraConnect::Utils::Fixed price = 0;
                    AstraConnect::Utils::Fixed volume;
                    for(auto val : ary){
                        if(price != 0){
                            volume = AstraConnect::Utils::readFixed(val.value());
                        } else {
                            price = AstraConnect::Utils::readFixed(val.value());
                        }
                    }
                    out.registerEntry(price,volume,true);
                }
                for(auto ask :  doc["asks"]){
                    auto ary = ask.value().get_array();
                    AstraConnect::Utils::Fixed price = 0;
                    AstraConnect::Utils::Fixed volume;
                    for(auto val : ary){
                        if(price != 0){
                            volume = AstraConnect::Utils::readFixed(val.value());
                        } else {
                            price = AstraConnect::Utils::readFixed(val.value());
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

        // Raw GET on fapi.binance.com, e.g. "/fapi/v1/time" or "/fapi/v1/exchangeInfo".
        // SUCCESS for any HTTP response, including 4xx/5xx: body and httpStatus are
        // filled and the caller judges them. BOOST_ERROR (with message) only when no
        // response could be obtained, after one reconnect and retry.
        FetchError fetchRaw(const std::string& target, std::string& body, int& httpStatus) {
            StreamData data = futures.sendRequest(target);
            if(!data) return FetchError(APIError::BOOST_ERROR,data.ec.message());
            body.assign(data.string.data(), data.string.size());
            httpStatus = data.httpStatus;
            return FetchError(APIError::SUCCESS);
        }

        API(APIManager& base_) : futures(base_.ioc,base_.ctx,base_.hostFutures){
        }

    };
