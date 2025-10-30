#pragma once
#include "simdjson.h"
#include "userDataStream/userDataStreamClass.hpp"
#include "userDataStream/userDataStreamFunctions.hpp"

// Check userDataStreamClass.hpp for request payloads and implementation functions
auto UserDataStreamClassCreation = [](simdjson::padded_string& json, simdjson::ondemand::parser& parser) {
    auto doc = parser.iterate(json);
    std::string type;
    doc["e"].get_string(type);
    EventType eventType = returnEventType(type);
    UserDataStream* userDataStream;
    switch (eventType) {
        case EventType::ACCOUNT_UPDATE:
            accountUpdate(doc.value(),userDataStream);
            break;
            
        case EventType::MARGIN_CALL:
            marginCallUpdate(doc.value(),userDataStream);
            break;

        case EventType::ORDER_UPDATE:
            orderUpdate(doc.value(),userDataStream);
            break;

        case EventType::TRADE_LITE:
            tradeLite(doc.value(),userDataStream);
            break;

        case EventType::ACCOUNT_CONFIG_UPDATE:
            accountConfigUpdate(doc.value(),userDataStream);
            break;

        case EventType::STRATEGY_UPDATE:
            strategyUpdate(doc.value(),userDataStream);
            break;

        case EventType::GRID_UPDATE:
            gridUpdate(doc.value(),userDataStream);
            break;

        case EventType::CONDITIONAL_ORDER_REJECT:
            conditionalOrderReject(doc.value(),userDataStream);
            break;

        default:
            // Unknown or unsupported event
            userDataStream = new UserDataStream();
            std::cerr << "Unknown event type received in user data stream.\n";
            break;
    }
    return *userDataStream;
};
