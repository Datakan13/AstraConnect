#pragma once
#include "net/includeBoost.hpp"
#include <AstraLib/AstraLib.hpp>
#include <simdjson/simdjson.h>
#include "core/protocol/requestParameter.hpp"
#include "core/types/status.hpp"
#include "utils/net/exponentialBackOff.hpp"
#include <cstdint>
#include <memory>

// Error handling DONE

class StreamHolder {
    net::io_context& ioc;
    net::ssl::context& ctx;
    tcp::resolver resolver;
    std::string port = "443";
    std::string host;
    const int version = 11;
    // Rebuilt on every connection attempt: an SSL* that has failed or been closed
    // by the peer cannot be handshaken again, so reconnecting on the old object
    // never recovered.
    std::unique_ptr<beast::ssl_stream<tcp::socket>> stream;
    beast::flat_buffer buffer;
    AstraLib::Atomic::Spinlock lock;
    ConnectionStatus connectionStatus;
    // backoff schedule, see utils/net/exponentialBackOff.hpp
    AstraConnect::Utils::BackoffPolicy backoffPolicy;

    // Beast's response parser stops at 8 MB by default; exchangeInfo is about 1 MB
    // today and grows with every listing.
    static constexpr std::uint64_t bodyLimit = 64ull * 1024 * 1024;

    // establishes a connection to the main host on a fresh stream
    ConnectionStatus establishConnection(const std::string& host) {
        beast::error_code ec;
        if(stream) stream->next_layer().close(ec);
        stream = std::make_unique<beast::ssl_stream<tcp::socket>>(ioc, ctx);
        buffer.consume(buffer.size());
        ctx.set_default_verify_paths();
        stream->set_verify_mode(boost::asio::ssl::verify_peer);
        stream->set_verify_callback(boost::asio::ssl::host_name_verification(host));

        // Resolve and connect
        auto const result = resolver.resolve(host, port,ec);
        if(ec) {
            return ConnectionStatus::FAIL;
        }
        net::connect(stream->next_layer(), result,ec);
        if(ec) {
            return ConnectionStatus::FAIL;
        }

        // Set SNI 
        if(!SSL_set_tlsext_host_name(stream->native_handle(), host.c_str())) return ConnectionStatus::FAIL;

        // Perform TLS handshake
        stream->handshake(ssl::stream_base::client,ec);
        if(ec) {
            return ConnectionStatus::FAIL;
        } 
        
        return ConnectionStatus::SUCCESS;
    }

    ConnectionStatus exponentialBackOff(const std::string& host) {
        return AstraConnect::Utils::exponentialBackOff(
            [&]{ return establishConnection(host); },
            backoffPolicy);
    }

    // One write + read on the current connection. Fills data on success.
    beast::error_code exchange(http::request<http::string_body>& req, StreamData& data) {
        beast::error_code ec;
        http::write(*stream, req, ec);
        if(ec) return ec;
        http::response_parser<http::dynamic_body> parser;
        parser.body_limit(bodyLimit);
        http::read(*stream, buffer, parser, ec);
        if(ec) return ec;
        auto& res = parser.get();
        data.httpStatus = res.result_int();
        data.string = simdjson::padded_string(
            boost::beast::buffers_to_string(res.body().data())
        );
        // The server may close after this response; reconnect up front next time
        if(!res.keep_alive()) connectionStatus = ConnectionStatus::FAIL;
        return ec;
    }

    // Sends req, reconnecting first if the last request broke the connection.
    // Binance drops idle keep-alive connections, which only shows up as a write or
    // read error on the next request, so a failed exchange gets one reconnect and
    // one retry before it is reported.
    StreamData perform(http::request<http::string_body>& req) {
        StreamData data;
        if(connectionStatus != ConnectionStatus::SUCCESS) connectionStatus = establishConnection(host);
        beast::error_code ec;
        if(connectionStatus == ConnectionStatus::SUCCESS) {
            ec = exchange(req, data);
            if(!ec) {
                data.status = ConnectionStatus::SUCCESS;
                return data;
            }
            buffer.consume(buffer.size());
        }
        connectionStatus = establishConnection(host);
        if(connectionStatus != ConnectionStatus::SUCCESS) {
            data.ec = ec ? ec : beast::error_code(net::error::not_connected);
            return data;
        }
        data = StreamData();
        ec = exchange(req, data);
        if(ec) {
            data.ec = ec;
            buffer.consume(buffer.size());
            connectionStatus = ConnectionStatus::FAIL;
            return data;
        }
        data.status = ConnectionStatus::SUCCESS;
        return data;
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
        AstraLib::Atomic::SpinlockGuard guard(lock);
        if (isExponentialBackoff) {
            connectionStatus = establishConnection(host);
            if(connectionStatus != ConnectionStatus::SUCCESS) connectionStatus = exponentialBackOff(host);
            return connectionStatus;
        } else {
            connectionStatus = establishConnection(host);
            return connectionStatus;
        }
    }
    // Will return a StreamData struct with components
    // status: ConnectionStatus object 
    // string: simdjson::padded_string object
    // httpStatus: the HTTP status code, set for every response including 4xx/5xx
    // ec: boost::beast::error_code
    // the returned StreamData should be used as if(streamData) string valid otherwise invalid check ec
    StreamData sendRequest(const std::string& target, http::verb method = http::verb::get, bool keepAlive = true) {
        AstraLib::Atomic::SpinlockGuard guard(lock);
        http::request<http::string_body> req{method, target, version};

        req.set(http::field::host, host);
        req.set(http::field::user_agent, "APIManager/1.0");
        if (keepAlive) {
            req.set(http::field::connection, "keep-alive");
        }

        return perform(req);
    }

    // Will return a StreamData struct with components
    // status: ConnectionStatus object 
    // string: simdjson::padded_string object
    // httpStatus: the HTTP status code, set for every response including 4xx/5xx
    // ec: boost::beast::error_code
    // the returned StreamData should be used as if(streamData) string valid otherwise invalid check ec
    template<typename... Args>
    StreamData sendRequest(const std::string& target, http::verb method = http::verb::get,
        bool keepAlive = true, bool needBody = false,
        RequestParameter<Args>... requestParameters) {
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
        
        return perform(req);
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
    , host(host_),resolver(ioc_){
        connectionStatus = establishConnection(host);
        if(connectionStatus != ConnectionStatus::SUCCESS) connectionStatus = exponentialBackOff(host);
    }

    // WARNING: the caller needs to check if the connection is alive with connectionAlive() since it is non-throwing
    // Definitions
    // Delay: initial delay in ms
    // increasePerTry: the multiplicator per fail
    // maxTryCount: power of desired max delay in the formula delay * increasePerTry^maxTryCount
    StreamHolder(net::io_context& ioc_, net::ssl::context& ctx_, std::string host_,int delay_,int increasePerTry_,int maxTryCount_) : 
    ioc(ioc_),ctx(ctx_) 
    , host(host_),resolver(ioc_),
    backoffPolicy{delay_, increasePerTry_, maxTryCount_} {
        connectionStatus = establishConnection(host);
        if(connectionStatus != ConnectionStatus::SUCCESS) connectionStatus = exponentialBackOff(host);
    }
};
