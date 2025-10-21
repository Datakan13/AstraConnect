#pragma once 
#include "boost_include_helpers/includeBoost.hpp"
#include <AstraLib/AstraLib.hpp>
#include "simdjson.h"
#include "tradeEvent.hpp"


class dataWebsocketAPI {
    boost::asio::io_context& ioc;
    boost::asio::ssl::context& ctx;
    simdjson::ondemand::parser parser;
    const std::string host = "fstream.binance.com";
    const std::string port = "443";
    /*
    Payload example
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
    public:
    AstraLib::Buffers::AtomicRingBuffer<TradeEvent,1024> tradeBuffer;
    TradeEvent tradeOut;
    void websocketLoop(std::string pair) {
        const std::string target = "/ws/"+pair+"@trade";
        tcp::resolver resolver(ioc);
        
        beast::websocket::stream<ssl::stream<tcp::socket>> ws(ioc, ctx);
        beast::flat_buffer buffer;

        auto const results = resolver.resolve(host, port);
        for(;;){
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
        ws.handshake(host, target);
        for(;;) {
            try {
                ws.read(buffer);
                auto json = simdjson::padded_string(
                    boost::beast::buffers_to_string(buffer.data())
                );
                buffer.consume(buffer.size());
                auto tradeEvent = parser.iterate(json);

                tradeOut.timestamp = tradeEvent["E"].get_int64().value();
                tradeOut.maker = tradeEvent["m"].get_bool().value();
                tradeOut.price = tradeEvent["p"].get_double_in_string().value();
                tradeOut.volume = tradeEvent["q"].get_double_in_string().value();
                tradeBuffer.noMoveEnqueue(tradeOut);
            } catch(std::exception& e) {
                std::cout << e.what() << std::endl;
            }
            
        }
        
    }
    }
    public:
    dataWebsocketAPI(boost::asio::io_context& ioc_, boost::asio::ssl::context& ctx_): ctx(ctx_), ioc(ioc_){
        
    }
};

