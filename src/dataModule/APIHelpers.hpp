#pragma once
#include "boost_include_helpers/includeBoost.hpp"
#include "simdjson.h"
#include "threadSafeParser.hpp"
#include <AstraLib/AstraLib.hpp>
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
    auto sendRequest(const std::string& target) {
        AstraLib::Atomic::SpinlockGuard guard(lock);
        http::request<http::string_body> req{http::verb::get, target, version};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, "CandleFetcher/1.0");
        req.set(http::field::connection, "keep-alive");
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

// The first template is the data you want to keep e.g. TradeEvent 
// The second template is your data construction function which must return your data type 
// The second template should also accept simdjson::padded_string
//e.g. [simdjson::padded_string json](){  TradeEvent event = json; return event; }
template<typename Data,typename Func>
class WebsocketStreamHolder {
    net::io_context& ioc;
    net::ssl::context& ctx;
    tcp::resolver resolver;
    std::string target;
    std::string host;
    std::string port = "443";
    beast::websocket::stream<ssl::stream<tcp::socket>> ws;
    beast::flat_buffer buffer;
    Func func;
    ThreadSafeParser& parserWrapper;

    void setupConnection() {
        try {
            auto const results = resolver.resolve(host, port);
            ctx.set_default_verify_paths();
            net::connect(ws.next_layer().next_layer(), results.begin(), results.end());

            // Set SNI Hostname (required by many TLS servers)
            if(!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str())) {
                beast::error_code ec{static_cast<int>(::ERR_get_error()),
                                        net::error::get_ssl_category()};
                throw beast::system_error{ec};
            }

            
            // Perform the SSL handshake
            ws.next_layer().handshake(ssl::stream_base::client);

            // Now perform the WebSocket handshake
            boost::beast::websocket::response_type response;
            ws.handshake(response,host, target);
        } catch(std::exception& e) {
            std::cout << e.what() << std::endl;
        }
    }

    public:
    AstraLib::Buffers::AtomicRingBuffer<Data,1024> bufferOut;
    auto getLatestMessage() {
        try {
            ws.read(buffer);
            auto json = simdjson::padded_string(
                boost::beast::buffers_to_string(buffer.data())
            );
            buffer.consume(buffer.size());
            return json;
        } catch(std::exception& e) {
            std::cout << e.what() << std::endl;
            auto string = simdjson::padded_string();
            return string;
        }
        
    }

    void executionLoop() {
        setupConnection();
        Data data;
        for(;;) {
            auto json = getLatestMessage();
            ThreadSafeParserRAII parser(parserWrapper);

            data = func(json,parser.parser);
            bufferOut.noMoveEnqueue(data);
        }
    }

    WebsocketStreamHolder(net::io_context& ioc_, net::ssl::context& ctx_, 
    Func func_, 
    std::string host_, std::string target_,ThreadSafeParser& parser_) : 
    resolver(ioc_), ws(ioc_,ctx_),
    target(target_) ,host(host_), 
    ioc(ioc_), ctx(ctx_), parserWrapper(parser_), 
    func(std::move(func_)) {
        std::thread thread(&WebsocketStreamHolder::executionLoop,this);
        thread.detach();
    }
};