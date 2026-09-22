#pragma once
#include <simdjson/simdjson.h>
#include "api/common/userData/userDataStream.hpp"
#include "api/common/userData/userDataStreamParsers.hpp"
// Check userDataStreamClass.hpp for request payloads and implementation functions
//
// Each branch returns a UserDataStream by value. The previous version allocated one
// with new, returned a copy of it, and leaked the original on every event.
inline auto parseUserDataStream = [](simdjson::padded_string& json, simdjson::ondemand::parser& parser) {
    auto doc = parser.iterate(json);
    std::string type;
    doc["e"].get_string(type);
    EventType eventType = returnEventType(type);

    switch (eventType) {
        case EventType::ACCOUNT_UPDATE:
            return accountUpdate(doc.value());

        case EventType::MARGIN_CALL:
            return marginCallUpdate(doc.value());

        case EventType::ORDER_UPDATE:
            return orderUpdate(doc.value());

        case EventType::TRADE_LITE:
            return tradeLite(doc.value());

        case EventType::ACCOUNT_CONFIG_UPDATE:
            return accountConfigUpdate(doc.value());

        case EventType::STRATEGY_UPDATE:
            return strategyUpdate(doc.value());

        case EventType::GRID_UPDATE:
            return gridUpdate(doc.value());

        case EventType::CONDITIONAL_ORDER_REJECT:
            return conditionalOrderReject(doc.value());

        default:
            // Unknown or unsupported event; the caller sees EventType::UNKNOWN
            // and a null returnPtr().
            return UserDataStream();
    }
};
