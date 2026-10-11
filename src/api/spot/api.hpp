#pragma once
#include "manager/apiManager.hpp"
#include "core/streams/streamHolder.hpp"
#include "core/parsing/threadSafeParser.hpp"
#include "api/common/error/apiError.hpp"
#include "api/common/error/includeErrors.hpp"
#include "model/candle.hpp"

class APIManager::Spot::API{
        StreamHolder spot;
        ThreadSafeParser parserSpot;
        public:
        // Wire format: doc/payloads/rest/spot-klines.md

        // pair: e.g. "BTCUSDT" timeframe: e.g. "5m", "1h"
        FetchError fetchCandles(AstraLib::Buffers::AtomicRingBuffer<Candle,2048>& outputBuff, const std::string pair,  const std::string timeframe) {

            const std::string target = "/api/v3/klines?symbol="+ pair+"&interval=" + timeframe;
            StreamData data = spot.sendRequest(target);
            if(!data) return FetchError(APIError::BOOST_ERROR,data.ec.message());

            ThreadSafeParserRAII parser(parserSpot);
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

        API(APIManager& base_) : spot(base_.ioc,base_.ctx,base_.hostSpot) {
        }
    };