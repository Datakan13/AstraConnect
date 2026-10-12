#pragma once
#include <simdjson/simdjson.h>
enum class RequestStatus {
    SUCCESS,
    FAIL,
    UNKNOWN
};

enum class ConnectionStatus {
    SUCCESS,
    FAIL,
    CLOSED
};

enum class PriceValid {
    VALID,
    NOT_VALID
};

struct StreamData {
    public:
    ConnectionStatus status = ConnectionStatus::FAIL;
    simdjson::padded_string string;
    // HTTP status of the response; 0 when there was none (websocket, or no response)
    int httpStatus = 0;
    beast::error_code ec;
    explicit operator bool ( ) {
        return status == ConnectionStatus::SUCCESS;
    }
};