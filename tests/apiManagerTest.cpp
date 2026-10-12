#include "api/AstraConnect.hpp"
#include <chrono>
#include <vector>
#include <AstraLib/AstraLib.hpp>
using AstraConnect::Utils::Fixed;
using AstraConnect::Utils::FIXED_SCALE;
using AstraConnect::Utils::formatFixed;

// Truncates a 1e-8 value to `decimals` decimals
Fixed setPrecision(Fixed value, int decimals) {
    Fixed step = FIXED_SCALE;
    for(int i = 0; i < decimals; i++) step /= 10;
    return value / step * step;
}

int main() {
    // The library does not read the environment; this test chooses to
    const char* apiKey = std::getenv("API_KEY");
    const char* privateKey = std::getenv("PRIVATE_KEY");
    APIManager manager(Credentials{apiKey ? apiKey : "", privateKey ? privateKey : ""});
    APIManager::Futures::API futuresAPI(manager);
    AstraLib::Buffers::AtomicRingBuffer<Candle,2048> vec;
    AstraLib::Buffers::AtomicRingBuffer<OpenInterest,1024> vec2;
    AstraLib::Time::Timer timer;
    APIManager::Spot::API spotAPI(manager);
    APIManager::Futures::Streams::TradeEventStream tradeEventStream(manager , "ethusdc");
    APIManager::Futures::Streams::MarkPriceStream markPriceStream(manager,"ethusdc");
    APIManager::User::FuturesStream userDataStreamFuturesAPI(manager);

    APIManager::Futures::OrderStream orderStream(manager);

    APIManager::Orderbook orderbook(manager,"ethusdc",futuresAPI);

    PairInfo info;
    timer.start();
    futuresAPI.fetchExchangeInfoForPair("ETHUSDT",info);
    timer.write();
      std::cout << "minNotional: " << formatFixed(info.minNotional)
          << ", minPrice: " << formatFixed(info.minPrice)
          << ", maxPrice: " << formatFixed(info.maxPrice)
          << ", tickSize: " << formatFixed(info.tickSize)
          << ", minQuantity: " << formatFixed(info.minQuantity)
          << ", maxQuantity: " << formatFixed(info.maxQuantity)
          << ", stepSize: " << formatFixed(info.stepsize)
          << std::endl;
    timer.start();
    spotAPI.fetchCandles(vec,"ETHUSDT","5m");
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
    futuresAPI.fetchOpenInterestHist(vec2,"ETHUSDT","5m");
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
    futuresAPI.fetchOpenInterestCurrent(openInterest,"ethusdc");
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
          << ", price: " << formatFixed(event.price)
          << ", volume: " << formatFixed(event.volume)
          << ", maker: " << std::boolalpha << event.maker
          << std::endl;

    std::cout << "timestamp: " << event2.timestamp
          << ", price: " << formatFixed(event2.price)
          << ", volume: " << formatFixed(event2.volume)
          << ", maker: " << std::boolalpha << event2.maker
          << std::endl;

    timer.start();
    AstraLib::Buffers::AtomicRingBuffer<MarkPrice,1024>& markPriceStreamBuffer = markPriceStream.accessToMarkPriceStream();
    timer.write("Fetched event stream buffer refference");

    timer.start();
    MarkPrice markPriceEvent =  markPriceStreamBuffer.dequeue();
    timer.write("Dequeued a trade markPriceEvent from buffer");

    timer.start();
    MarkPrice markPriceEvent2 = markPriceStreamBuffer.dequeue();
    timer.write("Dequeued a trade markPriceEvent from buffer");

    std::cout << "timestamp: " << markPriceEvent.timestamp
          << ", index price: " << formatFixed(markPriceEvent.indexPrice)
          << ", mark price: " << formatFixed(markPriceEvent.markPrice)
          << ", funding rate: " << std::boolalpha << markPriceEvent.fundingRate
          << std::endl;

    std::cout << "timestamp: " << markPriceEvent2.timestamp
          << ", index price: " << formatFixed(markPriceEvent2.indexPrice)
          << ", mark price: " << formatFixed(markPriceEvent2.markPrice)
          << ", funding rate: " << std::boolalpha << markPriceEvent2.fundingRate
          << std::endl;

    UserDataStream userData;
    RequestStatus userDataStatus  = userDataStreamFuturesAPI.getLastMessage(userData);
    auto userptr = userData.returnPtr();
    if(!userptr) std::cout << "no message recieved" << std::endl;
    std::string orderID;

    timer.start();
    orderStream.sendNewOrder(PositionSide::LONG,OrderSide::BUY,TimeInForce::GTC,OrderType::LIMIT,"ETHUSDT",setPrecision(markPriceEvent.markPrice,1),2 * FIXED_SCALE / 10,orderID);
    timer.write("Took time for this order");

    int64_t orderId = orderStream.getLastOrderId();

    std::string orderIdStr = std::to_string(orderId);

    timer.start();
    orderStream.sendModifyOrder(PositionSide::LONG,OrderSide::SELL,PriceMatchMode::NONE,OrderType::MARKET,"ETHUSDT",1000023 * FIXED_SCALE / 100,FIXED_SCALE / 10,orderIdStr,orderID);
    timer.write();

    timer.start();
    orderStream.sendCancelOrder("ETHUSDT",orderIdStr,orderID);
    timer.write();

    timer.start();
    orderStream.sendQueryOrder("ETHUSDT",orderIdStr,orderID);
    timer.write();
    std::string accountInfoId;
    timer.start();
    RequestStatus accountInfoStatus = orderStream.sendAccountInfoRequest(accountInfoId);
    timer.write();
    if(accountInfoStatus == RequestStatus::SUCCESS) std::cout << "Succeeded" << std::endl;
    

    AccountInfoLite accountInfoHere;
     orderStream.getAccountInfo(accountInfoId,accountInfoHere);
    std::cout << "asset: " << accountInfoHere.asset
          << ", balance: " << formatFixed(accountInfoHere.balance)
          << ", crossWalletBalance: " << formatFixed(accountInfoHere.crossWalletBalance)
          << ", availableBalance: " << formatFixed(accountInfoHere.availableBalance)
          << ", updateTime: " << accountInfoHere.updateTime
          << std::endl;
     std::this_thread::sleep_for(std::chrono::hours(3));
}
