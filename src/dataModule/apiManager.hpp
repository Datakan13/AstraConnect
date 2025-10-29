#pragma once
#include "boost_include_helpers/includeBoost.hpp"
#include <vector>
#include "candle.hpp"
#include "simdjson.h"
#include "openInterest.hpp"
#include "APIHelpers.hpp"
#include "threadSafeParser.hpp"
#include "tradeEvent.hpp"
#include "markPrice.hpp"
#include "userDataStream/userDataStreamClass.hpp"
#include "userDataStream/userDataStreamFunctions.hpp"
#include <utility>
#include <AstraLib/AstraLib.hpp>
#include <iostream>
#include <fstream>
#include "requestParameter.hpp"
#include "orderTracker/orderTracker.hpp"
#include <charconv>


/*
    Payload example for trade event stream
    Update rate: Event based
    {
        "e": "trade",       // Event type
        "E": 1672515782136, // Event time
        "s": "BNBBTC",      // Symbol
        "t": 12345,         // Trade ID
        "p": "0.001",       // Price
        "q": "100",         // Quantity
        "T": 1672515782136, // Trade time
        "m": true,          // Is the buyer the market maker?
        "M": true           // Ignore
    }
*/
auto TradeEventCreation = [](simdjson::padded_string& json,simdjson::ondemand::parser& parser){
    TradeEvent tradeOut;
    
    auto tradeEvent = parser.iterate(json);

    tradeOut.timestamp = tradeEvent["E"].get_int64().value();
    tradeOut.maker = tradeEvent["m"].get_bool().value();
    tradeOut.price = tradeEvent["p"].get_double_in_string().value();
    tradeOut.volume = tradeEvent["q"].get_double_in_string().value();
    return tradeOut;
};

/*
    Payload example for mark price stream 
    Update rate: 1000ms
  {
    "e": "markPriceUpdate",  	// Event type
    "E": 1562305380000,      	// Event time
    "s": "BTCUSDT",          	// Symbol
    "p": "11794.15000000",   	// Mark price
    "i": "11784.62659091",		// Index price
    "P": "11784.25641265",		// Estimated Settle Price, only useful in the last hour before the settlement starts
    "r": "0.00038167",       	// Funding rate
    "T": 1562306400000       	// Next funding time
  }
*/
auto MarkPriceCreation = [](simdjson::padded_string& json, simdjson::ondemand::parser& parser) {
    MarkPrice out;

    auto iterate = parser.iterate(json);

    out.timestamp = iterate["E"].get_int64().value();
    out.markPrice = iterate["p"].get_double_in_string().value();
    out.indexPrice = iterate["i"].get_double_in_string().value();
    out.fundingRate = iterate["r"].get_double_in_string().value();
    out.fundingTime = iterate["T"].get_int64().value();

    return out;
};  

// Check userDataStreamClass.hpp for request payloads and implementation functions
auto UserDataStreamClassCreation = [](simdjson::padded_string& json, simdjson::ondemand::parser& parser) {
    auto doc = parser.iterate(json);
    std::string type;
    doc["e"].get_string(type);
    EventType eventType = returnEventType(type);
    UserDataStream* userDataStream;
    switch (eventType) {
        case EventType::ACCOUNT_UPDATE:
            accountUpdate(doc.value(),userDataStream);
            break;
            
        case EventType::MARGIN_CALL:
            marginCallUpdate(doc.value(),userDataStream);
            break;

        case EventType::ORDER_UPDATE:
            orderUpdate(doc.value(),userDataStream);
            break;

        case EventType::TRADE_LITE:
            tradeLite(doc.value(),userDataStream);
            break;

        case EventType::ACCOUNT_CONFIG_UPDATE:
            accountConfigUpdate(doc.value(),userDataStream);
            break;

        case EventType::STRATEGY_UPDATE:
            strategyUpdate(doc.value(),userDataStream);
            break;

        case EventType::GRID_UPDATE:
            gridUpdate(doc.value(),userDataStream);
            break;

        case EventType::CONDITIONAL_ORDER_REJECT:
            conditionalOrderReject(doc.value(),userDataStream);
            break;

        default:
            // Unknown or unsupported event
            userDataStream = new UserDataStream();
            std::cerr << "Unknown event type received in user data stream.\n";
            break;
    }
    return *userDataStream;
};

class APIManager {

    const std::string hostSpot = "api.binance.com";
    const std::string hostFutures = "fapi.binance.com";
    const std::string hostFuturesWebsocket = "fstream.binance.com";
    const std::string hostFuturesWebsocketAPI = "ws-fapi.binance.com";
    std::string APIKey;
    std::string PrivateKey;
    boost::asio::io_context& ioc;
    boost::asio::ssl::context& ctx;

    ThreadSafeParser websocketParser;
    public:
    class SpotAPI {
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

    class FuturesAPI {
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

    class WebsocketStreams {
        public:
        class Futures{
            public:
            class TradeEventStream {
                std::string pair;
                const std::string target = "/ws/"+ pair +"@trade";
                WebsocketStreamHolder<TradeEvent,decltype(TradeEventCreation)> tradeEventStream;
                public:
                AstraLib::Buffers::AtomicRingBuffer<TradeEvent,1024>& accessToTradeEventStream() {
                    return tradeEventStream.bufferOut; 
                }

                TradeEventStream(APIManager& base_, std::string pair_) : pair(pair_), tradeEventStream(TradeEventCreation,base_.hostFuturesWebsocket,target,base_.websocketParser) {
                }
            };

            class AggregatedTradeEventStream {

            };

            class MarkPriceStream {
                const std::string pair;
                const std::string target = "/ws/"+pair+"@markPrice@1s";

                WebsocketStreamHolder<MarkPrice, decltype(MarkPriceCreation)> markPriceStream;
                public:
                AstraLib::Buffers::AtomicRingBuffer<MarkPrice,1024>& accessToTradeEventStream() {
                    return markPriceStream.bufferOut;
                }
                MarkPriceStream(APIManager& base_,std::string pair_) : pair(pair_) ,markPriceStream(MarkPriceCreation,base_.hostFuturesWebsocket,target,base_.websocketParser) {

                }
            };

        };


    };    

    class UserDataStreams {
        public:
        class UserFuturesStream {
            StreamHolder userDataStreamAPI;
            WebsocketStreamHolder<UserDataStream,decltype(UserDataStreamClassCreation)>* userDataStreamWebsocket = nullptr;
            std::string APIKey;
            std::string listenKey;
            simdjson::ondemand::parser parser;
            RequestParameter<std::string> listenKeyParameter;
            RequestParameter<std::string> requestId;
            RequestParameter<std::string> method;
            RequestParameter<std::string> parameters;
            AstraLib::Atomic::PaddedAtomic<bool> messagePresent;
            /*  
                Payload example for listen key 
                {"listenKey":"gwKzdioWPho490C2wogHUt9EF8rfkSxO5EVILWXV7gD0k94n7wP97EEfzA3DURPH"}
            */
            public:
            void getListenKey() {
                std::string target = "/fapi/v1/listenKey";
                auto json = userDataStreamAPI.sendRequest(target,http::verb::post,true,false,listenKeyParameter);
                auto doc = parser.iterate(json);
                doc.find_field("listenKey").get_string(listenKey,true);
            }

            void deleteListenKey() {
                std::string target = "/fapi/v1/listenKey";
                auto json = userDataStreamAPI.sendRequest(target,http::verb::delete_,true,false,listenKeyParameter);
            }

            // non blocking
            UserDataStream getLastMessage() {
                if(userDataStreamWebsocket->controlVariable.value.load(std::memory_order_acquire)) {
                    return userDataStreamWebsocket->bufferOut.dequeue();
                } else {
                    return UserDataStream();
                }
            }

            UserFuturesStream(APIManager& base_) : userDataStreamAPI(base_.ioc,base_.ctx,base_.hostFutures),
                APIKey(base_.APIKey),
                listenKeyParameter("X-MBX-APIKEY",base_.APIKey), 
                requestId("X-Request-ID", boost::uuids::to_string(boost::uuids::random_generator()())),
                method("method", "userDataStream.start"),
                parameters("parameters")
                {
                    getListenKey();
                    std::string websocketTarget = "/ws/" + listenKey;
                    userDataStreamWebsocket = new WebsocketStreamHolder<UserDataStream,decltype(UserDataStreamClassCreation)>(
                        UserDataStreamClassCreation,
                        base_.hostFuturesWebsocket,
                        websocketTarget,
                        base_.websocketParser);
            }

            ~UserFuturesStream() {
                delete userDataStreamWebsocket;
                std::cout << "I atleast destroyed the websocket" << std::endl;
                deleteListenKey();
                std::cout << "Closed it all off" << std::endl;
            }
        };
       
        // do not use
        class UserSpotStream {
            StreamHolder userDataStream;
            std::string APIKey;
            std::string listenKey;
            simdjson::ondemand::parser parser;
            RequestParameter<std::string> listenKeyParameter;

             /*
                Payload example for listen key 
                {"listenKey":"CFhC7fBYdyq2PoFnsk9U5lvpCEb5fEyQkjCR0DdvPCCzCZMEt9DCGrKzb"}
            */
            public:
            void getListenKey() {
                std::string target = "/fapi/v1/listenKey";
                auto json = userDataStream.sendRequest(target,http::verb::post,true,false,listenKeyParameter);
                auto doc = parser.iterate(json);
                doc.find_field("listenKey").get_string(listenKey,true);
            }

            UserSpotStream(APIManager& base_) : userDataStream(base_.ioc,base_.ctx,base_.hostFutures), APIKey(base_.APIKey),listenKeyParameter("X-MBX-APIKEY",base_.APIKey){
                
            }
        };
        };

        class PlaceOrderParameters {
            public:
            RequestParameter<std::string> positionSide;
            RequestParameter<std::string> price;
            RequestParameter<std::string> quantity;
            RequestParameter<std::string> side;
            RequestParameter<std::string> symbol;
            RequestParameter<std::string> timeInForce;
            RequestParameter<std::string> timestamp;
            RequestParameter<std::string> type;

            void prepareOrder(PositionSide posSide,
                  OrderSide orderSide,
                  TimeInForce tif,
                  OrderType orderType,
                  const std::string& pair,
                  const std::string& px,
                  const std::string& qty)
            {
                // Position side (BOTH, LONG, SHORT)
                switch (posSide) {
                    case PositionSide::BOTH:  positionSide.request = "BOTH"; break;
                    case PositionSide::LONG:  positionSide.request = "LONG"; break;
                    case PositionSide::SHORT: positionSide.request = "SHORT"; break;
                }

                // Side (BUY or SELL)
                side.request = (orderSide == OrderSide::BUY) ? "BUY" : "SELL";

                // Symbol
                symbol.request = pair;

                // Price & quantity
                price.request    = px;
                quantity.request = qty;

                // Order type
                switch (orderType) {
                    case OrderType::LIMIT:                type.request = "LIMIT"; break;
                    case OrderType::MARKET:               type.request = "MARKET"; break;
                    case OrderType::STOP:                 type.request = "STOP"; break;
                    case OrderType::STOP_MARKET:          type.request = "STOP_MARKET"; break;
                    case OrderType::TAKE_PROFIT:          type.request = "TAKE_PROFIT"; break;
                    case OrderType::TAKE_PROFIT_MARKET:   type.request = "TAKE_PROFIT_MARKET"; break;
                    case OrderType::TRAILING_STOP_MARKET: type.request = "TRAILING_STOP_MARKET"; break;
                    case OrderType::LIQUIDATION:          type.request = "LIQUIDATION"; break;
                }

                // Time in force
                switch (tif) {
                    case TimeInForce::GTC: timeInForce.request = "GTC"; break;
                    case TimeInForce::IOC: timeInForce.request = "IOC"; break;
                    case TimeInForce::FOK: timeInForce.request = "FOK"; break;
                    case TimeInForce::GTX: timeInForce.request = "GTX"; break;
                }

                // Timestamp (milliseconds)
                timestamp.request = std::to_string(AstraLib::Time::unixTimestampMS());
            }

            PlaceOrderParameters() : positionSide("positionSide"), 
            price("price"), quantity("quantity"), 
            side("side"), symbol("symbol"), 
            timeInForce("timeInForce"), timestamp("timestamp"),
            type("type") {

            }
        };

        // To use order responses and check out the data you sent use orderContext you can use a reference of it too
        class OrderStream {
            std::string target = "/ws-fapi/v1";
            WebsocketAPIStreamHolder orderStream;
            RequestParameter<std::string> APIKey;
            RequestParameter<std::string> HMACKey;
            std::string privateKey;
            PlaceOrderParameters placeOrder;
            RequestParameter<std::string> methodPlaceOrder;
            RequestParameter<std::string> requestId;
            OrderTracker orderTracker;
            simdjson::ondemand::parser parser;
            OrderContext orderContext;
            // Generates a RequestParameter<std::string> object from inputs
            RequestParameter<std::string> generateParamsForOrder(PositionSide posSide,
                  OrderSide orderSide,
                  TimeInForce tif,
                  OrderType orderType,
                  const std::string& pair,
                  const std::string& px,
                  const std::string& qty) {
                RequestParameter<std::string> out("params");
                
                placeOrder.prepareOrder(posSide,orderSide,tif,orderType,pair,px,qty);
                out.makeRequestFromRequestParameters(
                    APIKey,
                    placeOrder.positionSide,
                    placeOrder.price,
                    placeOrder.quantity,
                    placeOrder.side,
                    placeOrder.symbol,
                    placeOrder.timeInForce,
                    placeOrder.timestamp,
                    placeOrder.type  
                );
                HMACKey.request = hmac_sha256(privateKey,out.request);
                out.addParameterToRequest(HMACKey);
                return out;
            }

            // Generates a random id
            void generateID() {
                requestId.request = boost::uuids::to_string(boost::uuids::random_generator()());
            }
            
            public:
            // Sends an order to binance with all the given parameters and also adds the binance response to the Order class 
            // Returns true if the order has been registered returns false if it has not been registered 
            // If returns false check Order's response to see what happened
            // A new order id is created for each request and written to the given string
            bool sendNewOrder(PositionSide posSide,
                  OrderSide orderSide,
                  TimeInForce tif,
                  OrderType orderType,
                  const std::string& pair,
                  double px,
                  double qty,std::string& orderID) {
                    try{
                        generateID();
                        orderID = requestId.request;
                        
                        // Turn price and quantity to strings since generateParamsForOrder expects std::string
                        char bufPx[32];
                        char bufQty[32];
                        auto [ptrPx, ec1] = std::to_chars(bufPx, bufPx + sizeof(bufPx), px, std::chars_format::fixed, 6);
                        auto [ptrQty, ec2] = std::to_chars(bufQty, bufQty + sizeof(bufQty), qty, std::chars_format::fixed, 6);
                        std::string pxStr(bufPx, ptrPx);
                        std::string qtyStr(bufQty, ptrQty);

                        RequestParameter<std::string> params = generateParamsForOrder(posSide,orderSide,tif,orderType,pair,pxStr,qtyStr);
                        std::string payload = makeRequestFromRequestParameters(requestId,methodPlaceOrder,params);

                        std::cout << payload << std::endl;

                        auto json = orderStream.sendRequest(payload);
                        std::cout << "response: " << json << std::endl;
                        simdjson::ondemand::document doc = parser.iterate(json);

                        orderTracker.registerOrderSent(orderID,AstraLib::Time::unixTimestampMS(),px, qty,pair,tif,orderSide,posSide,orderType,doc);
                        int64_t status = orderTracker.getOrderStatusint(orderID);
                        if( status == 200) {
                            orderTracker.queueActiveOrderId(orderID);
                            return true;
                        } else {
                            std::cout << "status code: " << status << std::endl;
                            return false;
                        }
                        
                    } catch(std::exception& e ) {
                        std::cout << "Soo error is here: "<< e.what() << std::endl;
                        return false;
                    }
                    

            }

            OrderStream(APIManager& base_) : orderStream(base_.hostFuturesWebsocketAPI, target),
            privateKey(base_.PrivateKey), APIKey("apiKey",base_.APIKey), HMACKey("signature"),
            methodPlaceOrder("method","order.place"), requestId("id"), orderContext(orderTracker){

            }
        };

    public:
    APIManager(boost::asio::io_context& ioc_, boost::asio::ssl::context& ctx_) :
    ioc(ioc_), ctx(ctx_) {
        APIKey = std::getenv("API_KEY");
        PrivateKey = std::getenv("PRIVATE_KEY");
    };


};


