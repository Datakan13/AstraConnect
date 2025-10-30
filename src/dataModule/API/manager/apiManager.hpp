#pragma once
#include "boost_include_helpers/includeBoost.hpp"
#include <vector>
#include "candle.hpp"
#include "simdjson.h"
#include "openInterest.hpp"
#include "API/helperFunctions/APIHelpers.hpp"
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
    class SpotAPI;

    class FuturesAPI;
    class WebsocketStreams;

    class UserDataStreams;

    class PlaceOrderParameters;

    // To use order responses and check out the data you sent use orderContext you can use a reference of it too
    class OrderStream;

    public:
    APIManager(boost::asio::io_context& ioc_, boost::asio::ssl::context& ctx_) :
    ioc(ioc_), ctx(ctx_) {
        APIKey = std::getenv("API_KEY");
        PrivateKey = std::getenv("PRIVATE_KEY");
    };


};


