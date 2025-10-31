#pragma once
#include <string>

enum class APIError : int {
    SUCCESS                     = 0,
    BAD_FIELD                   = 1,
    BOOST_ERROR                 = 2,
    // 10xx - General Server or Network issues
    UNKNOWN                     = -1000,
    DISCONNECTED                = -1001,
    UNAUTHORIZED                = -1002,
    TOO_MANY_REQUESTS           = -1003,
    UNEXPECTED_RESP             = -1006,
    TIMEOUT                     = -1007,
    SERVER_BUSY                 = -1008,
    INVALID_MESSAGE             = -1013,
    UNKNOWN_ORDER_COMPOSITION   = -1014,
    TOO_MANY_ORDERS             = -1015,
    SERVICE_SHUTTING_DOWN       = -1016,
    UNSUPPORTED_OPERATION       = -1020,
    INVALID_TIMESTAMP           = -1021,
    INVALID_SIGNATURE           = -1022,
    COMP_ID_IN_USE              = -1033,
    TOO_MANY_CONNECTIONS        = -1034,
    LOGGED_OUT                  = -1035,

    // 11xx - Request issues
    ILLEGAL_CHARS               = -1100,
    TOO_MANY_PARAMETERS         = -1101,
    MANDATORY_PARAM_EMPTY_OR_MALFORMED = -1102,
    UNKNOWN_PARAM               = -1103,
    UNREAD_PARAMETERS           = -1104,
    PARAM_EMPTY                 = -1105,
    PARAM_NOT_REQUIRED          = -1106,
    PARAM_OVERFLOW              = -1108,
    BAD_PRECISION               = -1111,
    NO_DEPTH                    = -1112,
    TIF_NOT_REQUIRED            = -1114,
    INVALID_TIF                 = -1115,
    INVALID_ORDER_TYPE          = -1116,
    INVALID_SIDE                = -1117,
    EMPTY_NEW_CL_ORD_ID         = -1118,
    EMPTY_ORG_CL_ORD_ID         = -1119,
    BAD_INTERVAL                = -1120,
    BAD_SYMBOL                  = -1121,
    INVALID_SYMBOLSTATUS        = -1122,
    INVALID_LISTEN_KEY          = -1125,
    MORE_THAN_XX_HOURS          = -1127,
    OPTIONAL_PARAMS_BAD_COMBO   = -1128,
    INVALID_PARAMETER           = -1130,
    BAD_STRATEGY_TYPE           = -1134,
    INVALID_JSON                = -1135,
    INVALID_TICKER_TYPE         = -1139,
    INVALID_CANCEL_RESTRICTIONS = -1145,
    DUPLICATE_SYMBOLS           = -1151,
    INVALID_SBE_HEADER          = -1152,
    UNSUPPORTED_SCHEMA_ID       = -1153,
    SBE_DISABLED                = -1155,
    OCO_ORDER_TYPE_REJECTED     = -1158,
    OCO_ICEBERGQTY_TIMEINFORCE  = -1160,
    DEPRECATED_SCHEMA           = -1161,
    BUY_OCO_LIMIT_MUST_BE_BELOW = -1165,
    SELL_OCO_LIMIT_MUST_BE_ABOVE= -1166,
    BOTH_OCO_ORDERS_CANNOT_BE_LIMIT = -1168,
    INVALID_TAG_NUMBER          = -1169,
    TAG_NOT_DEFINED_IN_MESSAGE  = -1170,
    TAG_APPEARS_MORE_THAN_ONCE  = -1171,
    TAG_OUT_OF_ORDER            = -1172,
    GROUP_FIELDS_OUT_OF_ORDER   = -1173,
    INVALID_COMPONENT           = -1174,
    RESET_SEQ_NUM_SUPPORT       = -1175,
    ALREADY_LOGGED_IN           = -1176,
    GARBLED_MESSAGE             = -1177,
    BAD_SENDER_COMPID           = -1178,
    BAD_SEQ_NUM                 = -1179,
    EXPECTED_LOGON              = -1180,
    TOO_MANY_MESSAGES           = -1181,
    PARAMS_BAD_COMBO            = -1182,
    NOT_ALLOWED_IN_DROP_COPY_SESSIONS = -1183,
    DROP_COPY_SESSION_NOT_ALLOWED = -1184,
    DROP_COPY_SESSION_REQUIRED  = -1185,
    NOT_ALLOWED_IN_ORDER_ENTRY_SESSIONS = -1186,
    NOT_ALLOWED_IN_MARKET_DATA_SESSIONS = -1187,
    INCORRECT_NUM_IN_GROUP_COUNT = -1188,
    DUPLICATE_ENTRIES_IN_A_GROUP = -1189,
    INVALID_REQUEST_ID          = -1190,
    TOO_MANY_SUBSCRIPTIONS      = -1191,
    INVALID_TIME_UNIT           = -1194,
    BUY_OCO_STOP_LOSS_MUST_BE_ABOVE = -1196,
    SELL_OCO_STOP_LOSS_MUST_BE_BELOW = -1197,
    BUY_OCO_TAKE_PROFIT_MUST_BE_BELOW = -1198,
    SELL_OCO_TAKE_PROFIT_MUST_BE_ABOVE = -1199,
    INVALID_PEG_PRICE_TYPE      = -1210,
    INVALID_PEG_OFFSET_TYPE     = -1211,
    SYMBOL_DOES_NOT_MATCH_STATUS = -1220,

    // 20xx - Trading / Order issues
    NEW_ORDER_REJECTED          = -2010,
    CANCEL_REJECTED             = -2011,
    NO_SUCH_ORDER               = -2013,
    BAD_API_KEY_FMT             = -2014,
    REJECTED_MBX_KEY            = -2015,
    NO_TRADING_WINDOW           = -2016,
    ORDER_ARCHIVED              = -2026,
    SUBSCRIPTION_ACTIVE         = -2035,
    SUBSCRIPTION_INACTIVE       = -2036,
    CLIENT_ORDER_ID_INVALID     = -2039,
    MAXIMUM_SUBSCRIPTION_IDS    = -2042
};

constexpr APIError fromErrorCode(int code) noexcept {
    switch (code) {
        // 10xx - General Server or Network issues
        case -1000: return APIError::UNKNOWN;
        case -1001: return APIError::DISCONNECTED;
        case -1002: return APIError::UNAUTHORIZED;
        case -1003: return APIError::TOO_MANY_REQUESTS;
        case -1006: return APIError::UNEXPECTED_RESP;
        case -1007: return APIError::TIMEOUT;
        case -1008: return APIError::SERVER_BUSY;
        case -1013: return APIError::INVALID_MESSAGE;
        case -1014: return APIError::UNKNOWN_ORDER_COMPOSITION;
        case -1015: return APIError::TOO_MANY_ORDERS;
        case -1016: return APIError::SERVICE_SHUTTING_DOWN;
        case -1020: return APIError::UNSUPPORTED_OPERATION;
        case -1021: return APIError::INVALID_TIMESTAMP;
        case -1022: return APIError::INVALID_SIGNATURE;
        case -1033: return APIError::COMP_ID_IN_USE;
        case -1034: return APIError::TOO_MANY_CONNECTIONS;
        case -1035: return APIError::LOGGED_OUT;

        // 11xx - Request issues
        case -1100: return APIError::ILLEGAL_CHARS;
        case -1101: return APIError::TOO_MANY_PARAMETERS;
        case -1102: return APIError::MANDATORY_PARAM_EMPTY_OR_MALFORMED;
        case -1103: return APIError::UNKNOWN_PARAM;
        case -1104: return APIError::UNREAD_PARAMETERS;
        case -1105: return APIError::PARAM_EMPTY;
        case -1106: return APIError::PARAM_NOT_REQUIRED;
        case -1108: return APIError::PARAM_OVERFLOW;
        case -1111: return APIError::BAD_PRECISION;
        case -1112: return APIError::NO_DEPTH;
        case -1114: return APIError::TIF_NOT_REQUIRED;
        case -1115: return APIError::INVALID_TIF;
        case -1116: return APIError::INVALID_ORDER_TYPE;
        case -1117: return APIError::INVALID_SIDE;
        case -1118: return APIError::EMPTY_NEW_CL_ORD_ID;
        case -1119: return APIError::EMPTY_ORG_CL_ORD_ID;
        case -1120: return APIError::BAD_INTERVAL;
        case -1121: return APIError::BAD_SYMBOL;
        case -1122: return APIError::INVALID_SYMBOLSTATUS;
        case -1125: return APIError::INVALID_LISTEN_KEY;
        case -1127: return APIError::MORE_THAN_XX_HOURS;
        case -1128: return APIError::OPTIONAL_PARAMS_BAD_COMBO;
        case -1130: return APIError::INVALID_PARAMETER;
        case -1134: return APIError::BAD_STRATEGY_TYPE;
        case -1135: return APIError::INVALID_JSON;
        case -1139: return APIError::INVALID_TICKER_TYPE;
        case -1145: return APIError::INVALID_CANCEL_RESTRICTIONS;
        case -1151: return APIError::DUPLICATE_SYMBOLS;
        case -1152: return APIError::INVALID_SBE_HEADER;
        case -1153: return APIError::UNSUPPORTED_SCHEMA_ID;
        case -1155: return APIError::SBE_DISABLED;
        case -1158: return APIError::OCO_ORDER_TYPE_REJECTED;
        case -1160: return APIError::OCO_ICEBERGQTY_TIMEINFORCE;
        case -1161: return APIError::DEPRECATED_SCHEMA;
        case -1165: return APIError::BUY_OCO_LIMIT_MUST_BE_BELOW;
        case -1166: return APIError::SELL_OCO_LIMIT_MUST_BE_ABOVE;
        case -1168: return APIError::BOTH_OCO_ORDERS_CANNOT_BE_LIMIT;
        case -1169: return APIError::INVALID_TAG_NUMBER;
        case -1170: return APIError::TAG_NOT_DEFINED_IN_MESSAGE;
        case -1171: return APIError::TAG_APPEARS_MORE_THAN_ONCE;
        case -1172: return APIError::TAG_OUT_OF_ORDER;
        case -1173: return APIError::GROUP_FIELDS_OUT_OF_ORDER;
        case -1174: return APIError::INVALID_COMPONENT;
        case -1175: return APIError::RESET_SEQ_NUM_SUPPORT;
        case -1176: return APIError::ALREADY_LOGGED_IN;
        case -1177: return APIError::GARBLED_MESSAGE;
        case -1178: return APIError::BAD_SENDER_COMPID;
        case -1179: return APIError::BAD_SEQ_NUM;
        case -1180: return APIError::EXPECTED_LOGON;
        case -1181: return APIError::TOO_MANY_MESSAGES;
        case -1182: return APIError::PARAMS_BAD_COMBO;
        case -1183: return APIError::NOT_ALLOWED_IN_DROP_COPY_SESSIONS;
        case -1184: return APIError::DROP_COPY_SESSION_NOT_ALLOWED;
        case -1185: return APIError::DROP_COPY_SESSION_REQUIRED;
        case -1186: return APIError::NOT_ALLOWED_IN_ORDER_ENTRY_SESSIONS;
        case -1187: return APIError::NOT_ALLOWED_IN_MARKET_DATA_SESSIONS;
        case -1188: return APIError::INCORRECT_NUM_IN_GROUP_COUNT;
        case -1189: return APIError::DUPLICATE_ENTRIES_IN_A_GROUP;
        case -1190: return APIError::INVALID_REQUEST_ID;
        case -1191: return APIError::TOO_MANY_SUBSCRIPTIONS;
        case -1194: return APIError::INVALID_TIME_UNIT;
        case -1196: return APIError::BUY_OCO_STOP_LOSS_MUST_BE_ABOVE;
        case -1197: return APIError::SELL_OCO_STOP_LOSS_MUST_BE_BELOW;
        case -1198: return APIError::BUY_OCO_TAKE_PROFIT_MUST_BE_BELOW;
        case -1199: return APIError::SELL_OCO_TAKE_PROFIT_MUST_BE_ABOVE;
        case -1210: return APIError::INVALID_PEG_PRICE_TYPE;
        case -1211: return APIError::INVALID_PEG_OFFSET_TYPE;
        case -1220: return APIError::SYMBOL_DOES_NOT_MATCH_STATUS;

        // 20xx - Trading / Order issues
        case -2010: return APIError::NEW_ORDER_REJECTED;
        case -2011: return APIError::CANCEL_REJECTED;
        case -2013: return APIError::NO_SUCH_ORDER;
        case -2014: return APIError::BAD_API_KEY_FMT;
        case -2015: return APIError::REJECTED_MBX_KEY;
        case -2016: return APIError::NO_TRADING_WINDOW;
        case -2026: return APIError::ORDER_ARCHIVED;
        case -2035: return APIError::SUBSCRIPTION_ACTIVE;
        case -2036: return APIError::SUBSCRIPTION_INACTIVE;
        case -2039: return APIError::CLIENT_ORDER_ID_INVALID;
        case -2042: return APIError::MAXIMUM_SUBSCRIPTION_IDS;

        default: return APIError::UNKNOWN;
    }
}

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
