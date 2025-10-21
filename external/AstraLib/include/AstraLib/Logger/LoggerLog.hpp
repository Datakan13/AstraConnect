#pragma once

#include <AstraLib/Logger/LogTypes.hpp>
#include <cstring>
#include <AstraLib/Logger/LoggerConfig.hpp>
#include <AstraLib/Time/timeNow.hpp>
class Log {
    public:
    uint64_t timestamp;
    LogType type;
    char* message;
    void setPtr(char* ptr){
        message = ptr;
    }

    explicit Log(LogType type_, const char* msg){
        type = type_;
        timestamp = AstraLib::Time::unixTimestampNS();
        std::strncpy(message, msg, LoggerConfig::MESSAGE_SIZE - 1);
        message[LoggerConfig::MESSAGE_SIZE - 1] = '\0';
    }

    template <typename... Args>
    explicit Log(LogType type_,const char* fmt,Args&&... args){
        type = type_;
        timestamp = AstraLib::Time::unixTimestampNS();
        snprintf(message, LoggerConfig::MESSAGE_SIZE,
                fmt, std::forward<Args>(args)...);
    }  
    Log(){}
};