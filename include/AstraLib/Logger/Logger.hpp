#pragma once
#include <thread>
#include <cstring>
#include <AstraLib/Buffers/atomicRingBuffer.hpp>
#include <AstraLib/Time/timeNow.hpp>
#include <iostream>
#include <AstraLib/Atomic/paddedAtomic.hpp>
#include <AstraLib/Atomic/spinlock.hpp>
#include <filesystem>
#include <chrono>
#include <AstraLib/Debug/debugFunc.hpp>
#include <AstraLib/Time/timer.hpp>
#include <AstraLib/Logger/FileHeader.hpp>
#include <AstraLib/Logger/LoggerConfig.hpp>
#include <AstraLib/Logger/LogTypes.hpp>
#include <AstraLib/Logger/LoggerErrors.hpp>
#include <AstraLib/Logger/LoggerLog.hpp>

namespace AstraLib {
    namespace Logger {

    

class Logger {
    private:
    AstraLib::Buffers::AtomicRingBuffer<Log,LoggerConfig::QUEUE_SIZE> queue;
    inline static std::array<char[LoggerConfig::MESSAGE_SIZE], LoggerConfig::QUEUE_SIZE> messageArray;
    std::string fileName = std::string(LoggerConfig::DEFAULT_FILE);
    FileHeader fileHeader;
    FILE* file = nullptr;
    AstraLib::Atomic::PaddedAtomic<int> logCount{0};
    AstraLib::Atomic::Spinlock spinlock;
    AstraLib::Atomic::AtomicFutex<> futex;
    AstraLib::Atomic::PaddedAtomic<bool> isWaiting;
    void setUpBuffer(){
        for(int i = 0; i<LoggerConfig::QUEUE_SIZE; i++) {
            Log* ptr = queue.claimPtrNoSync(i);
            ptr->setPtr(messageArray[i]);
        }
    }

    void prepareHeader() {
        fileHeader = LoggerConfig::header;
    }

    void modifyHeader() {
        // TODO: add support to change the header so different files can be logged into 
    }

    void setFile(std::string fileName = ""){
        AstraLib::Atomic::SpinlockGuard guard(spinlock);
        if(file) {
            fclose(file);
            file = nullptr;
        }
        if(fileName != "") {
            file = fopen(fileName.c_str(),"ab");
        }
        if(!file) {
            std::filesystem::create_directories("Logs");
            file = fopen(LoggerConfig::DEFAULT_FILE.data(),"ab");
            if(!file) {
                throw LoggerErrors::FILE_SETUP_UNSUCCESFUL;
            }
            fwrite(&fileHeader,sizeof(FileHeader),1,file);
        }
    }

    void writeLog(const Log log) {
        fwrite(&log.type,sizeof(LogType),1,file);
        fwrite(&log.timestamp,sizeof(uint64_t),1,file);
        fwrite(log.message,sizeof(char[LoggerConfig::MESSAGE_SIZE]),1,file);
    }

    public:
    void logMessage(LogType type, const char* msg) {
        queue.emplaceEnqueue(type,msg);
        int count = logCount.value.fetch_add(1,std::memory_order_release);
        if(isWaiting.value.load(std::memory_order_acquire)){
            futex.wake();
            isWaiting.value.store(false,std::memory_order_release);
        }
    }

    // Format specifiers for logMessages():
    //   %d   int
    //   %u   unsigned int
    //   %lld long long
    //   %llu unsigned long long
    //   %f   double
    //   %s   const char* (e.g. literals, ex.what(), string.c_str())
    //   %p   pointer
    //
    // Example:
    //   logger.logMessages(LogType::ERROR,
    //                      "Order %d failed at price %.2f after %llu µs",
    //                      orderId, price, latency);
    template <typename... Args>
    void logMessage(LogType type,const char* fmt,Args&&... args){
        queue.emplaceEnqueue(type,fmt,std::forward<Args>(args)...);
        int count = logCount.value.fetch_add(1,std::memory_order_release);
        if(isWaiting.value.load(std::memory_order_acquire)){
            futex.wake();
            isWaiting.value.store(false,std::memory_order_release);
        }
    }   

    private:
    Log currentLog;
    AstraLib::Time::Timer timer;
    void loggerLoop() {
        int looped = 0;
        while (true){
            int count = logCount.value.load(std::memory_order_acquire);
            if(count ==0 && looped == 10000){
                isWaiting.value.store(true, std::memory_order_release);
                futex.wait();
                looped = 0;
            } else if(count !=0){
                std::array<Log,50>* logArrayPtr = queue.batchDequeue( count <= 50 ? count : 50 );
                for (int i = 0; i<count; i++) {
                    writeLog((*logArrayPtr)[i]);
                }
                logCount.value.fetch_sub(count,std::memory_order_release);
                fflush(file);
                looped = 0;
            } else {
                looped++;
            }
        }
    }
    public:
    Logger(){
        setUpBuffer();
        prepareHeader();
        setFile();
        std::thread thread(&Logger::loggerLoop,this);
        thread.detach();
    }
};

} // namespace Logger
} // namespace AstraLib