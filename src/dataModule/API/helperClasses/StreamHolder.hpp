#pragma once
#include "boost_include_helpers/includeBoost.hpp"
#include <AstraLib/AstraLib.hpp>
#include "simdjson.h"
#include "dataModule/dataTypes/requestParameter.hpp"

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
    void establishConnection(beast::ssl_stream<tcp::socket>& stream,const std::string host) {
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
        stream.handshake(ssl::stream_base::client);
    }

    public:


    //Returns a simdjson::padded_string that can be iterated
    auto sendRequest(const std::string& target, http::verb method = http::verb::get, bool keepAlive = true) {
        AstraLib::Atomic::SpinlockGuard guard(lock);
        http::request<http::string_body> req{method, target, version};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, "APIManager/1.0");
        if (keepAlive) {
            req.set(http::field::connection, "keep-alive");
        }
        http::write(stream, req);
        http::read(stream, buffer, res);
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
        establishConnection(stream,host);
    }
};
