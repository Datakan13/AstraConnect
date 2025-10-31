#include "dataModule/API/binanceAPI.hpp"
#include "dataModule/dataTypes/includeAllDataTypes.hpp"
#include <vector>
#include <AstraLib/AstraLib.hpp>
#include "dataModule/enums/enumClassesForAllStreams.hpp"
int main() {
    boost::asio::io_context ioc;
    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    APIManager manager(ioc,ctx);
    AstraLib::Buffers::AtomicRingBuffer<Candle,2048> vec;
    AstraLib::Buffers::AtomicRingBuffer<OpenInterest,1024> vec2;
    AstraLib::Time::Timer timer;
    APIManager::FuturesAPI futuresAPI(manager);
    APIManager::SpotAPI spotAPI(manager);
    APIManager::WebsocketStreams::Futures::TradeEventStream tradeEventStream(manager , "btcusdt");

    APIManager::UserDataStreams::UserFuturesStream userDataStreamFuturesAPI(manager);

    APIManager::OrderStream orderStream(manager);
    timer.start();
    spotAPI.fetchCandles(vec,"BTCUSDT","5m");
    timer.write("Fetched candles from spot API");
    for(int i = 0; i < 20; i++) {
        Candle candle = vec.dequeue();
        //std::cout << std::fixed << std::setprecision(8)
        //  << candle.timestampOpen << " | "
        //  << candle.open << " | "
        //  << candle.high << " | " 
        //  << candle.low << " | "
        //  << candle.close << " | "
        //  << candle.volume << " | "
        //  << candle.quoteVolume << " | "
        //  << candle.timestampClose << std::endl;
    }
    timer.start();
    futuresAPI.fetchOpenInterestHist(vec2,"BTCUSDT","5m");
    timer.write("Fetched open interest history from futures API");
    for(int i = 0; i < 20; i++) {
        OpenInterest oi = vec2.dequeue();

        //std::cout << std::fixed << std::setprecision(8)
        //<< oi.timestamp << " | "
        //<< oi.totalInterest << " | "
        //<< oi.totalInterestValue << " | " 
        //<< oi.circulation << std::endl;
    }
    CurrentOpenInterest openInterest;
    timer.start();
    futuresAPI.fetchOpenInterestCurrent(openInterest,"btcusdt");
    timer.write("Fetched current open interest from futures API");
    //std::cout << std::fixed << std::setprecision(8)
    //<< openInterest.timestamp << " | "
    //<< openInterest.openInterest << " | "
    //<< std::endl;

    timer.start();
    AstraLib::Buffers::AtomicRingBuffer<TradeEvent,1024>& tradeEventStreamBuffer = tradeEventStream.accessToTradeEventStream();
    timer.write("Fetched event stream buffer refference");

    timer.start();
    TradeEvent event =  tradeEventStreamBuffer.dequeue();
    timer.write("Dequeued a trade event from buffer");

    timer.start();
    TradeEvent event2 = tradeEventStreamBuffer.dequeue();
    timer.write("Dequeued a trade event from buffer");

    std::cout << "timestamp: " << event.timestamp
          << ", price: " << event.price
          << ", volume: " << event.volume
          << ", maker: " << std::boolalpha << event.maker
          << std::endl;

    std::cout << "timestamp: " << event2.timestamp
          << ", price: " << event2.price
          << ", volume: " << event2.volume
          << ", maker: " << std::boolalpha << event2.maker
          << std::endl;

    UserDataStream userData = userDataStreamFuturesAPI.getLastMessage();
    auto userptr = userData.returnPtr();
    if(!userptr) std::cout << "no message recieved" << std::endl;
    std::string orderID;
    timer.start();
    orderStream.sendNewOrder(PositionSide::LONG,OrderSide::SELL,TimeInForce::IOC,OrderType::LIMIT,"BTCUSDT",100000.32,0.2,orderID);
    timer.write("Took time for this order");
    
}
