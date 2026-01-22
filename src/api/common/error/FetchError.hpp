#pragma once 
#include "APIError.hpp"
#include <string>
#include <memory>
#include <simdjson/simdjson.h>

class FetchError {
    public:
    APIError error;
    std::unique_ptr<std::string> msg;

    std::string getErrorMsg() {
        if(msg) return *msg;
        return "";
    }

    // Returns the reference to the msg
    // Be careful about lifetime of FetchError class if it is out of scope you will get a segmentation fault
    // Use as soon as you called if you need to hold onto the message create a copy
    std::string& getErrorMsgUnsafe() {
        if(msg) return *msg;
        msg = std::make_unique<std::string>("");
        return *msg;
    }

    constexpr bool getError() const noexcept { return error != APIError::SUCCESS; }
    constexpr explicit operator bool() const noexcept { return error != APIError::SUCCESS;}

    FetchError(APIError error_, std::string& str) : error(error_) {
        msg = std::make_unique<std::string>(str);
    }
    FetchError(APIError error_, std::string&& str) : error(error_) {
        msg = std::make_unique<std::string>(std::move(str));
    }

    FetchError(int& errorCode_, std::string& str) {
        error = fromErrorCode(errorCode_);
        msg = std::make_unique<std::string>(str);
    }

    FetchError(APIError error_) : error(error_) {

    }

};

FetchError getErrorFromSimdjson(simdjson::ondemand::object& obj) noexcept {
    std::string msg;
    if (auto err = obj["msg"].get_string(msg)) {
        // Missing "msg" field, or invalid type
        msg = "";
    }
    return FetchError(fromErrorCode(obj["code"].get_int64().value()),msg);
}
