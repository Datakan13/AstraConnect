#pragma once
#include "api/common/userData/userDataStream.hpp"
#include <simdjson/simdjson.h>
#include <memory>
#include <type_traits>

// Each parser returns a UserDataStream by value; nothing here owns heap memory.
//
// The event time is read into a local BEFORE the payload is constructed. Both read
// from the same on-demand document, and simdjson's on-demand cursor is forward only,
// so leaving them as two arguments of one call left their order unsequenced and the
// fields could be consumed out of order.
//
// inline: these are definitions in a header, and without it a second translation
// unit including this file is a duplicate symbol.

inline UserDataStream accountUpdate(simdjson::ondemand::document& doc) {
  const int64_t eventTime = doc["E"].get_int64().value();
  return UserDataStream(EventType::ACCOUNT_UPDATE, eventTime,
                        std::make_unique<AccountUpdate>(doc));
}

inline UserDataStream marginCallUpdate(simdjson::ondemand::document& doc) {
  const int64_t eventTime = doc["E"].get_int64().value();
  return UserDataStream(EventType::MARGIN_CALL, eventTime,
                        std::make_unique<MarginCall>(doc));
}

inline UserDataStream orderUpdate(simdjson::ondemand::document& doc) {
  const int64_t eventTime = doc["E"].get_int64().value();
  return UserDataStream(EventType::ORDER_UPDATE, eventTime,
                        std::make_unique<OrderUpdate>(doc));
}

inline UserDataStream tradeLite(simdjson::ondemand::document& doc) {
  const int64_t eventTime = doc["E"].get_int64().value();
  return UserDataStream(EventType::TRADE_LITE, eventTime,
                        std::make_unique<TradeLite>(doc));
}

inline UserDataStream accountConfigUpdate(simdjson::ondemand::document& doc) {
  const int64_t eventTime = doc["E"].get_int64().value();
  return UserDataStream(EventType::ACCOUNT_CONFIG_UPDATE, eventTime,
                        std::make_unique<AccountConfigUpdate>(doc));
}

inline UserDataStream strategyUpdate(simdjson::ondemand::document& doc) {
  const int64_t eventTime = doc["E"].get_int64().value();
  return UserDataStream(EventType::STRATEGY_UPDATE, eventTime,
                        std::make_unique<StrategyUpdate>(doc));
}

inline UserDataStream gridUpdate(simdjson::ondemand::document& doc) {
  const int64_t eventTime = doc["E"].get_int64().value();
  return UserDataStream(EventType::GRID_UPDATE, eventTime,
                        std::make_unique<GridUpdate>(doc));
}

inline UserDataStream conditionalOrderReject(simdjson::ondemand::document& doc) {
  const int64_t eventTime = doc["E"].get_int64().value();
  return UserDataStream(EventType::CONDITIONAL_ORDER_REJECT, eventTime,
                        std::make_unique<ConditionalOrderReject>(doc));
}
