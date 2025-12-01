#pragma once
#include <cstdint>
#include <string>
class Error {
    public:
    int64_t errorCode;
    std::string message = "";

    Error(int64_t errorCode_, std::string& message_) : errorCode(errorCode_),message(message_) {

    }
    Error(int64_t errorCode_) : errorCode(errorCode_) {

    }
};

enum class RequestStatus {
    SUCCESS,
    FAIL,
    UNKNOWN
};

enum class ConnectionStatus {
    SUCCESS,
    FAIL
};

enum class PriceValid {
    VALID,
    NOT_VALID
};
