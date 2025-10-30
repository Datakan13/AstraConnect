#pragma once
#include "apiManager.hpp"
#include "StreamHolder.hpp"
#include "threadSafeParser.hpp"


class APIManager::SpotAPI{
        StreamHolder spot;
        ThreadSafeParser parserSpot;
        public:
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
        SpotAPI(APIManager& base_) : spot(base_.ioc,base_.ctx,base_.hostSpot) {
        }
    };