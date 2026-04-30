#pragma once
#include "api/common/userData/userDataStream.hpp"
#include <simdjson/simdjson.h>
#include <type_traits>

void accountUpdate(simdjson::ondemand::document& doc , UserDataStream*& stream) {
  stream = new UserDataStream(EventType::ACCOUNT_UPDATE,
    doc["E"].get_int64().value(),new AccountUpdate(doc)
  );
}

void marginCallUpdate(simdjson::ondemand::document& doc , UserDataStream*& stream) {
  stream = new UserDataStream(EventType::MARGIN_CALL, 
    doc["E"].get_int64().value(), new MarginCall(doc)
  );
}

void orderUpdate(simdjson::ondemand::document& doc , UserDataStream*& stream) {
  stream = new UserDataStream(EventType::ORDER_UPDATE, 
    doc["E"].get_int64().value(), 
    new OrderUpdate(doc)
);
}

void tradeLite(simdjson::ondemand::document& doc , UserDataStream*& stream) {
  stream = new UserDataStream(EventType::TRADE_LITE, 
    doc["E"].get_int64().value(), 
    new TradeLite(doc)
);
}

void accountConfigUpdate(simdjson::ondemand::document& doc , UserDataStream*& stream) {
  stream = new UserDataStream(EventType::ACCOUNT_CONFIG_UPDATE, 
    doc["E"].get_int64().value(), 
    new AccountConfigUpdate(doc)
);
}

void strategyUpdate(simdjson::ondemand::document& doc , UserDataStream*& stream) {
  stream = new UserDataStream(EventType::STRATEGY_UPDATE, 
    doc["E"].get_int64().value(), 
    new StrategyUpdate(doc)
);
}

void gridUpdate(simdjson::ondemand::document& doc , UserDataStream*& stream) {
  stream = new UserDataStream(EventType::GRID_UPDATE, 
    doc["E"].get_int64().value(), 
    new GridUpdate(doc)
);
}

void conditionalOrderReject(simdjson::ondemand::document& doc , UserDataStream*& stream) {
  stream = new UserDataStream(EventType::CONDITIONAL_ORDER_REJECT, 
    doc["E"].get_int64().value(), 
    new ConditionalOrderReject(doc)
);
}