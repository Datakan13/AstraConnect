#include "dataModule/apiManager.hpp"
#include "dataModule/candle.hpp"
#include "dataModule/openInterest.hpp"
#include <vector>
#include <AstraLib/AstraLib.hpp>
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
    timer.start();
    spotAPI.fetchCandles(vec,"BTCUSDT","5m");
    timer.write("Fetched candles from spot API in: ");
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
    timer.write("Fetched open interest history from futures API in: ");
    for(int i = 0; i < 20; i++) {
        OpenInterest oi = vec2.dequeue();

        //std::cout << std::fixed << std::setprecision(8)
        //<< oi.timestamp << " | "
        //<< oi.totalInterest << " | "
        //<< oi.totalInterestValue << " | " 
        //<< oi.circulation << std::endl;
    }

    timer.start();
    CurrentOpenInterest openInterest = futuresAPI.fetchOpenInterestCurrent("btcusdt");
    timer.write("Fetched current open interest from futures API in: ");
    //std::cout << std::fixed << std::setprecision(8)
    //<< openInterest.timestamp << " | "
    //<< openInterest.openInterest << " | "
    //<< std::endl;

    timer.start();
    AstraLib::Buffers::AtomicRingBuffer<TradeEvent,1024>& tradeEventStreamBuffer = tradeEventStream.accessToTradeEventStream();
    timer.write("Fetched event stream buffer refference in: ");
    timer.start();
    TradeEvent event =  tradeEventStreamBuffer.dequeue();
    timer.write("Dequeued a trade event from buffer in: ");
    timer.start();
    TradeEvent event2 = tradeEventStreamBuffer.dequeue();
    timer.write("Dequeued a trade event from buffer in: ");
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
}
