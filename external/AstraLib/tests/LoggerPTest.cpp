#include <AstraLib/Logger/Logger.hpp>
#include <iostream>
#include <AstraLib/Time/timer.hpp>
int main() {
    AstraLib::Logger::Logger logger;
    AstraLib::Time::Timer timer;
    std::cout << "sending message" << std::endl;
    int orderId = 424242;
    double price = 12345.6789;
    unsigned long long latency = 987654321ull;

    timer.start();
    logger.logMessage(LogType::ERROR, 
        "Order %d failed at price %.2f after %llu µs", 
        orderId, price, latency);
    timer.write();

    const char* filename = "config.json";
    int line = 128;
    timer.start();
    logger.logMessage(LogType::ERROR, 
        "Failed to parse file %s at line %d", 
        filename, line);
    timer.write();

    int socketFd = 5;
    const char* reason = "Connection reset by peer";
    timer.start();
    logger.logMessage(LogType::ERROR, 
        "Socket %d closed unexpectedly (%s)", 
        socketFd, reason);
    timer.write();

    const char* symbol = "BTCUSDT";
    double spread = 0.00042;
    timer.start();
    logger.logMessage(LogType::ERROR, 
        "Spread anomaly detected on %s (spread=%.6f)", 
        symbol, spread);
    timer.write();

    int threadId = 7;
    unsigned long long timestamp = 1690001234567ull;
    timer.start();
    logger.logMessage(LogType::ERROR, 
        "Thread %d stalled at timestamp %llu", 
        threadId, timestamp);
    timer.write();

    

    timer.start();
    logger.logMessage(LogType::ERROR,
        "Order %d on symbol %s failed at price %.4f after %llu µs due to unexpected state in risk checks",
        orderId, symbol, price, latency);
    timer.write();
    std::cout << "sent message" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(50));
}
