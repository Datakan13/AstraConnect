#pragma once
#include "net/includeBoost.hpp"
#include <AstraLib/AstraLib.hpp>
#include <simdjson/simdjson.h>
#include "core/protocol/requestParameter.hpp"
#include "core/types/status.hpp"

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
    ConnectionStatus connectionStatus;
    // backoff values
    const int delay = 500;
    const int increasePerTry = 2;
    const int maxTryCount = 20;
    const int maxDelay = static_cast<int>(delay * std::pow(increasePerTry, maxTryCount));

    // establishes a connection to the main host
    ConnectionStatus establishConnection(beast::ssl_stream<tcp::socket>& stream,const std::string& host) {
        beast::error_code ec;
        ctx.set_default_verify_paths();
        stream.set_verify_mode(boost::asio::ssl::verify_peer);
        stream.set_verify_callback(boost::asio::ssl::rfc2818_verification(host));

        // Resolve and connect
        auto const result = resolver.resolve(host, port,ec);
        if(ec) {
            return ConnectionStatus::FAIL;
        }
        net::connect(stream.next_layer(), result,ec);
        if(ec) {
            return ConnectionStatus::FAIL;
        }

        // Set SNI 
        if(!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) return ConnectionStatus::FAIL;

        // Perform TLS handshake
        stream.handshake(ssl::stream_base::client,ec);
        if(ec) {
            return ConnectionStatus::FAIL;
        } 
        
        return ConnectionStatus::SUCCESS;
    }

    ConnectionStatus exponentialBackOff(beast::ssl_stream<tcp::socket>& stream,const std::string& host) {
    std::chrono::milliseconds backoff(delay);
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

    void clearBuffer() {
        buffer.consume(buffer.size());
        res.body().clear(); 
        res.clear();
    }

    public:
    
    // It will try to connect to the host 
    // If given true it'll use exponential backoff if not it will only try to connect once 
    //
    // Default exponential backoff values are defined as 
    // Delay initial: 500ms
    // Max delay: 500^20ms
    // increase per fail: delay*=2 
    ConnectionStatus retryConnection(bool isExponentialBackoff) {
        if (isExponentialBackoff) {
            connectionStatus = exponentialBackOff(stream, host);
            return connectionStatus;
        } else {
            connectionStatus = establishConnection(stream, host);
            return connectionStatus;
        }
    }
    // Will return a StreamData struct with components
    // status: ConnectionStatus object 
    // string: simdjson::padded_string object
    // ec: boost::beast::error_code
    // the returned StreamData should be used as if(streamData) string valid otherwise invalid check ec
    StreamData sendRequest(const std::string& target, http::verb method = http::verb::get, bool keepAlive = true) {
        StreamData data;
        beast::error_code ec;
        AstraLib::Atomic::SpinlockGuard guard(lock);
        http::request<http::string_body> req{method, target, version};

        req.set(http::field::host, host);
        req.set(http::field::user_agent, "APIManager/1.0");
        if (keepAlive) {
            req.set(http::field::connection, "keep-alive");
        }

        http::write(stream, req,ec);
        if(ec) {
            data.ec = ec;
            clearBuffer();
            connectionStatus = ConnectionStatus::FAIL;
            return data;
        }
        http::read(stream, buffer, res,ec);
        if(ec) {
            data.ec = ec;
            clearBuffer();
            connectionStatus = ConnectionStatus::FAIL;
            return data;
        }

        data.string = simdjson::padded_string(
            boost::beast::buffers_to_string(res.body().data())
        );

        clearBuffer();
        data.status = ConnectionStatus::SUCCESS;
        return data;
    }

    // Will return a StreamData struct with components
    // status: ConnectionStatus object 
    // string: simdjson::padded_string object
    // ec: boost::beast::error_code
    // the returned StreamData should be used as if(streamData) string valid otherwise invalid check ec
    template<typename... Args>
    StreamData sendRequest(const std::string& target, http::verb method = http::verb::get,
        bool keepAlive = true, bool needBody = false,
        RequestParameter<Args>... requestParameters) {
        StreamData data;
        beast::error_code ec;
        AstraLib::Atomic::SpinlockGuard guard(lock);
        http::request<http::string_body> req{method, target, version};


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
        
        http::write(stream, req,ec);
        if(ec) {
            data.ec = ec;
            clearBuffer();
            connectionStatus = ConnectionStatus::FAIL;
            return data;
        }
        http::read(stream, buffer, res,ec);
        if(ec) {
            data.ec = ec;
            clearBuffer();
            connectionStatus = ConnectionStatus::FAIL;
            return data;
        }
        data.string = simdjson::padded_string(
            boost::beast::buffers_to_string(res.body().data())
        );
        clearBuffer();
        data.status = ConnectionStatus::SUCCESS;
        return data;
    }

    // Returns true if the connection is alive
    // LIMITATION: It can only have the current state of the connection if it's been newly constructed or had a request through it
    bool connectionAlive() {
        return connectionStatus == ConnectionStatus::SUCCESS;
    }

    // WARNING: the caller needs to check if the connection is alive with connectionAlive() since it is non-throwing
    // Default exponential backoff values are defined as 
    // Delay initial: 500ms
    // Max delay: 500 * 2^20ms
    // increase per fail: delay*=2 
    StreamHolder(net::io_context& ioc_, net::ssl::context& ctx_, std::string host_) : 
    ioc(ioc_),ctx(ctx_) 
    ,stream(ioc_,ctx_) , host(host_),resolver(ioc_){
        connectionStatus = establishConnection(stream,host);
        if(connectionStatus != ConnectionStatus::SUCCESS) connectionStatus = exponentialBackOff(stream,host);
    }

    // WARNING: the caller needs to check if the connection is alive with connectionAlive() since it is non-throwing
    // Definitions
    // Delay: initial delay in ms
    // increasePerTry: the multiplicator per fail
    // maxTryCount: power of desired max delay in the formula delay * increasePerTry^maxTryCount
    StreamHolder(net::io_context& ioc_, net::ssl::context& ctx_, std::string host_,int delay_,int increasePerTry_,int maxTryCount_) : 
    ioc(ioc_),ctx(ctx_) 
    ,stream(ioc_,ctx_) , host(host_),resolver(ioc_),
    delay(delay_), increasePerTry(increasePerTry_), maxTryCount(maxTryCount_) {
        connectionStatus = establishConnection(stream,host);
        if(connectionStatus != ConnectionStatus::SUCCESS) connectionStatus = exponentialBackOff(stream,host);
    }
};
