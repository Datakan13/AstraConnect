#pragma once
#include <simdjson/simdjson.h>
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

struct StreamData {
    public:
    ConnectionStatus status = ConnectionStatus::FAIL;
    simdjson::padded_string string;
    beast::error_code ec;
    explicit operator bool ( ) {
        return status == ConnectionStatus::SUCCESS;
    }
};