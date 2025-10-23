#include <AstraLib/AstraLib.hpp>
#include "dataModule/dataWebsocket.hpp"
#include "dataModule/APIHelpers.hpp"
#include "dataModule/tradeEvent.hpp"
int main() {
    net::io_context ioc;
    net::ssl::context ctx(net::ssl::context::tlsv12_client);
    std::string target = "/ws/btcusdt@trade";
    std::string host = "fstream.binance.com";
    ThreadSafeParser parser;
    auto TradeEventCreation = [](simdjson::padded_string& json,simdjson::ondemand::parser& parser){
        TradeEvent tradeOut;
        
        auto tradeEvent = parser.iterate(json);

        tradeOut.timestamp = tradeEvent["E"].get_int64().value();
        tradeOut.maker = tradeEvent["m"].get_bool().value();
        tradeOut.price = tradeEvent["p"].get_double_in_string().value();
        tradeOut.volume = tradeEvent["q"].get_double_in_string().value();
        return tradeOut;
    };
    
    WebsocketStreamHolder<TradeEvent,decltype(TradeEventCreation)> manager(ioc,ctx,TradeEventCreation,host,target,parser);
    std::cout << "waiting on event " << std::endl;
    TradeEvent event =  manager.bufferOut.dequeue();
    std::cout << "timestamp: " << event.timestamp
          << ", price: " << event.price
          << ", volume: " << event.volume
          << ", maker: " << std::boolalpha << event.maker
          << std::endl;
    
}