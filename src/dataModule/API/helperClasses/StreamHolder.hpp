#pragma once
#include "boost_include_helpers/includeBoost.hpp"
#include <AstraLib/AstraLib.hpp>
#include "simdjson.h"
#include "dataModule/dataTypes/requestParameter.hpp"
#include "dataModule/error.hpp"

// Error handling DONE

class StreamHolder {
    net::io_context& ioc;
    net::ssl::context& ctx;
    tcp::resolver resolver;
    std::string port = "443";
    std::string host;
    const int version = 11;
    beast::ssl_stream<tcp::socket> stream;
    beast::flat_buffer buffer;
    http::response<http::dynamic_body> res;
    AstraLib::Atomic::Spinlock lock;

    // establishes a connection to the main host
    ConnectionStatus establishConnection(beast::ssl_stream<tcp::socket>& stream,const std::string& host) {
        try {
            beast::error_code ec;
            ctx.set_default_verify_paths();
            stream.set_verify_mode(boost::asio::ssl::verify_peer);
            stream.set_verify_callback(boost::asio::ssl::rfc2818_verification(host));

            // Resolve and connect
            auto const result = resolver.resolve(host, port);
            net::connect(stream.next_layer(), result);

            // Set SNI 
            if(!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str()))
                throw beast::system_error(
                    beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()),
                    "Failed to set SNI Host Name");

            // Perform TLS handshake
            stream.handshake(ssl::stream_base::client,ec);
            if(!ec) {
                return ConnectionStatus::SUCCESS;
            }  else {
                return ConnectionStatus::FAIL;
            }
        } catch(std::exception& e) {
            return ConnectionStatus::FAIL;
        }
    }

    ConnectionStatus exponentialBackOff(beast::ssl_stream<tcp::socket>& stream,const std::string& host) {
    const int delay = 500;
    const int increasePerTry = 2;
    const int maxTryCount = 20;
    const int maxDelay = static_cast<int>(delay * std::pow(increasePerTry, maxTryCount));
    chrono::milliseconds backoff(delay);
    ConnectionStatus status;
    for(;;) {
        std::this_thread::sleep_for(backoff);
        status = establishConnection(stream,host);
        if(status == ConnectionStatus::SUCCESS) {
            return ConnectionStatus::SUCCESS;
        }
        backoff *= increasePerTry;
        if(backoff.count() >= maxDelay) {
            return ConnectionStatus::FAIL;
        }
    }
        
    }

    public:


    // Returns a simdjson::padded_string that can be iterated
    // Will throw runtime_error when boost cannot connect or fetch due to any problem
    auto sendRequest(const std::string& target, http::verb method = http::verb::get, bool keepAlive = true) {
        AstraLib::Atomic::SpinlockGuard guard(lock);
        http::request<http::string_body> req{method, target, version};
        try {
            req.set(http::field::host, host);
            req.set(http::field::user_agent, "APIManager/1.0");
            if (keepAlive) {
                req.set(http::field::connection, "keep-alive");
            }
            http::write(stream, req);
            http::read(stream, buffer, res);
        } catch(std::exception& e) {
            throw std::runtime_error(std::string{"Boost error while sending request: " + std::string(e.what())});
        }
        auto json = simdjson::padded_string(
            boost::beast::buffers_to_string(res.body().data())
        );
        buffer.consume(buffer.size());
        res.body().clear(); 
        res.clear();
        return json;
    }

    template<typename... Args>
    auto sendRequest(const std::string& target, http::verb method = http::verb::get,
        bool keepAlive = true, bool needBody = false,
        RequestParameter<Args>... requestParameters) {

        AstraLib::Atomic::SpinlockGuard guard(lock);
        http::request<http::string_body> req{method, target, version};

        try {
            req.set(http::field::host, host);
            req.set(http::field::user_agent, "APIManager/1.0");
            if (keepAlive) {
                req.set(http::field::connection, "keep-alive");
            }

            if(needBody) {
                std::string test = makeRequestFromRequestParameters(std::forward<RequestParameter<Args>>(requestParameters)...);
                req.body() = test;
                req.prepare_payload();
            } else {
                ((req.set(requestParameters.requestField, requestParameters.request)), ...);
            }
            
            http::write(stream, req);
            http::read(stream, buffer, res);
        } catch(std::exception& e) {
            throw std::runtime_error(std::string{"Boost error while sending request: " + std::string(e.what())});
        }
        auto json = simdjson::padded_string(
            boost::beast::buffers_to_string(res.body().data())
        );
        buffer.consume(buffer.size());
        res.body().clear(); 
        res.clear();
        return json;
    }

    StreamHolder(net::io_context& ioc_, net::ssl::context& ctx_, std::string host_) : 
    ioc(ioc_),ctx(ctx_) 
    ,stream(ioc_,ctx_) , host(host_),resolver(ioc_){
        ConnectionStatus status = establishConnection(stream,host);
        if(status != ConnectionStatus::SUCCESS) status = exponentialBackOff(stream,host);
        if(status != ConnectionStatus::SUCCESS) {
            std::string error = "Could not establish connection to" + host ;
            throw std::runtime_error(error);
        }
    }
};
