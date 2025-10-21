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
    timer.start();
    manager.fetchCandles(vec,"BTCUSDT","5m");
    timer.write();
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
    manager.fetchOpenInterestHist(vec2,"BTCUSDT","5m");
    timer.write();
    for(int i = 0; i < 20; i++) {
        OpenInterest oi = vec2.dequeue();

        //std::cout << std::fixed << std::setprecision(8)
        //<< oi.timestamp << " | "
        //<< oi.totalInterest << " | "
        //<< oi.totalInterestValue << " | " 
        //<< oi.circulation << std::endl;
    }

    timer.start();
    CurrentOpenInterest openInterest = manager.fetchOpenInterestCurrent("BTCUSDT");
    timer.write();
    std::cout << std::fixed << std::setprecision(8)
    << openInterest.timestamp << " | "
    << openInterest.openInterest << " | "
    << std::endl;

}
