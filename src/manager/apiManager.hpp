#pragma once
#include "net/includeBoost.hpp"
#include <vector>
#include <utility>
#include <AstraLib/AstraLib.hpp>
#include <iostream>
#include <fstream>
#include <charconv>
#include <stdexcept>
#include "core/parsing/threadSafeParser.hpp"

// Binance API credentials. Only order entry and user data streams need them;
// REST market data and market streams work without.
struct Credentials {
    std::string apiKey;
    std::string privateKey;
};

class APIManager {
    public:
    const std::string hostSpot = "api.binance.com";
    const std::string hostFutures = "fapi.binance.com";
    const std::string hostFuturesWebsocket = "fstream.binance.com";
    const std::string hostFuturesWebsocketAPI = "ws-fapi.binance.com";

    const Credentials credentials;

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
    // Components that authenticate call these from their first member initializer,
    // so a missing key fails at construction instead of as an auth error from Binance.
    const std::string& requireAPIKey(const char* component) const {
        if(credentials.apiKey.empty()) throw std::invalid_argument(std::string(component) + " needs an API key: construct APIManager with Credentials");
        return credentials.apiKey;
    }
    const std::string& requirePrivateKey(const char* component) const {
        if(credentials.privateKey.empty()) throw std::invalid_argument(std::string(component) + " needs a private key: construct APIManager with Credentials");
        return credentials.privateKey;
    }

    // Market data only; constructing an order or user data stream will throw
    APIManager() :
     ctx(boost::asio::ssl::context::tls_client) {
    };

    explicit APIManager(Credentials credentials_) :
     credentials(std::move(credentials_)), ctx(boost::asio::ssl::context::tls_client) {
    };


};


