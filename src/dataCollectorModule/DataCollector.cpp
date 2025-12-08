#include <dataModule/API/binanceAPI.hpp>
#include <dataModule/orderbook/orderbook.hpp>

int main() {
    APIManager manager;
    APIManager::FuturesAPI futuresAPI(manager);
    APIManager::WebsocketStreams::Futures::TradeEventStream futuresWebsocket(manager,"ethusdc");
    Orderbook orderbook(manager,"ethusdc",futuresAPI);
    
}