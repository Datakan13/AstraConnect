#include "boost_include_helpers/includeBoost.hpp"
#include "dataModule/tradeEvent.hpp"
#include "simdjson.h"

int main() {
    net::io_context ioc;
    net::ssl::context ctx(net::ssl::context::sslv23);
    tcp::resolver resolver(ioc);
    beast::websocket::stream<ssl::stream<tcp::socket>> ws(ioc,ctx);
    beast::flat_buffer buffer;
    std::string host = "fstream.binance.com";
    std::string target =  "/ws/btcusdt@trade";
    std::string port = "443";

    simdjson::ondemand::parser parser;

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
    std::cout << response << std::endl;

    ws.read(buffer);

    auto json = simdjson::padded_string(
        boost::beast::buffers_to_string(buffer.data())
    );
    std::cout << json << std::endl;
    buffer.consume(buffer.size());

    TradeEvent tradeOut;
        
    auto tradeEvent = parser.iterate(json);

    tradeOut.timestamp = tradeEvent["E"].get_int64().value();
    tradeOut.maker = tradeEvent["m"].get_bool().value();
    tradeOut.price = tradeEvent["p"].get_double_in_string().value();
    tradeOut.volume = tradeEvent["q"].get_double_in_string().value();

    std::cout << "timestamp: " << tradeOut.timestamp
          << ", price: " << tradeOut.price
          << ", volume: " << tradeOut.volume
          << ", maker: " << std::boolalpha << tradeOut.maker
          << std::endl;
}