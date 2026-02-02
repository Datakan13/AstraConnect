#pragma once
#include "net/includeBoost.hpp"
#include <vector>
#include <utility>
#include <AstraLib/AstraLib.hpp>
#include <iostream>
#include <fstream>
#include <charconv>
#include "core/parsing/threadSafeParser.hpp"

class APIManager {
    public:
    const std::string hostSpot = "api.binance.com";
    const std::string hostFutures = "fapi.binance.com";
    const std::string hostFuturesWebsocket = "fstream.binance.com";
    const std::string hostFuturesWebsocketAPI = "ws-fapi.binance.com";

    std::string APIKey;
    std::string PrivateKey;

    boost::asio::io_context ioc;
    boost::asio::ssl::context ctx;

    ThreadSafeParser websocketParser;
    public:
    class Futures{
        public:
        class API;
        class Streams;
        class OrderStream;
    };

    class Spot{
        public:
        class API;
    };

    class Orderbook;

    class User;

    public:
    APIManager() :
     ctx(boost::asio::ssl::context::sslv23) {
        APIKey = std::getenv("API_KEY");
        PrivateKey = std::getenv("PRIVATE_KEY");
    };


};


