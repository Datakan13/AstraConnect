#pragma once 
#include "api/common/error/apiError.hpp"
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
    // Use as soon as you called else if you need to hold onto the message create a copy
    std::string& getErrorMsgUnsafe() {
        if(msg) return *msg;
        msg = std::make_unique<std::string>("");
        return *msg;
    }

    constexpr bool getError() const noexcept { return error != APIError::SUCCESS; }
    constexpr explicit operator bool() const noexcept { return error != APIError::SUCCESS;}

    FetchError(APIError error_,const std::string& str) : error(error_) {
        msg = std::make_unique<std::string>(str);
    }
    FetchError(APIError error_, std::string&& str) : error(error_) {
        msg = std::make_unique<std::string>(std::move(str));
    }

    FetchError(int errorCode_, std::string& str) {
        error = fromErrorCode(errorCode_);
        msg = std::make_unique<std::string>(str);
    }

    FetchError(APIError error_) : error(error_) {

    }

};

inline FetchError getErrorFromSimdjson(simdjson::ondemand::object& obj) noexcept {
    std::string msg;
    if (auto err = obj["msg"].get_string(msg)) {
        // Missing "msg" field, or invalid type
        msg = "";
    }
    int64_t code;
    // .value() would throw here and noexcept turns that into std::terminate
    if (obj["code"].get_int64().get(code)) return FetchError(APIError::UNKNOWN,msg);
    return FetchError(fromErrorCode(static_cast<int>(code)),msg);
}

// Binance REST errors are a top-level {"code":-1121,"msg":"Invalid symbol."} sent in
// place of the expected payload. Returns SUCCESS when doc is anything else.
// Leaves doc rewound so the caller can parse it from the start.
inline FetchError getErrorFromDocument(simdjson::ondemand::document& doc) noexcept {
    FetchError result(APIError::SUCCESS);
    simdjson::ondemand::json_type type;
    simdjson::ondemand::object obj;
    int64_t code;
    if (!doc.type().get(type) && type == simdjson::ondemand::json_type::object
        && !doc.get_object().get(obj) && !obj["code"].get_int64().get(code)) {
        std::string_view msg;
        if (obj["msg"].get_string().get(msg)) msg = "";
        result = FetchError(fromErrorCode(static_cast<int>(code)),std::string(msg));
    }
    doc.rewind();
    return result;
}

// Iterates the response body into doc and checks it for a Binance error.
// Returns SUCCESS only when doc is ready to be parsed as the expected payload.
inline FetchError iterateResponse(simdjson::ondemand::parser& parser, simdjson::padded_string& body, simdjson::ondemand::document& doc) noexcept {
    if (auto ec = parser.iterate(body).get(doc)) {
        return FetchError(APIError::BAD_FIELD,std::string("Failed to parse response. error: ") + simdjson::error_message(ec));
    }
    return getErrorFromDocument(doc);
}

// Maps an HTTP status without a Binance error body (an HTML 5xx page, a bare 418)
// onto the closest APIError.
inline APIError fromHttpStatus(int httpStatus) noexcept {
    switch(httpStatus) {
        case 418:                   // IP auto-banned after ignoring 429s
        case 429: return APIError::TOO_MANY_REQUESTS;
        case 401:
        case 403: return APIError::UNAUTHORIZED;
        case 503: return APIError::SERVER_BUSY;
        default:  return APIError::UNEXPECTED_RESP;
    }
}

// As above, and also fails a non-2xx response. A Binance {"code","msg"} body wins
// because it says more; otherwise the status alone is reported.
inline FetchError iterateResponse(simdjson::ondemand::parser& parser, simdjson::padded_string& body, simdjson::ondemand::document& doc, int httpStatus) noexcept {
    FetchError err = iterateResponse(parser,body,doc);
    const bool httpFailed = httpStatus != 0 && httpStatus / 100 != 2;
    if(!httpFailed || (err && err.error != APIError::BAD_FIELD)) return err;
    return FetchError(fromHttpStatus(httpStatus),"HTTP " + std::to_string(httpStatus));
}
