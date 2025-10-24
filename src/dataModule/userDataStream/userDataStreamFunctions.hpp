#pragma once
#include "userDataStreanClass.hpp"
#include "helperClassesUserDataStreamClass.hpp"
#include <simdjson.h>
#include "userDataStreamHelperFunctions.hpp"

void accountUpdate(simdjson::fallback::ondemand::document_stream::iterator::value_type& doc , UserDataStream* stream) {
  stream = new UserDataStream(EventType::ACCOUNT_UPDATE,
    doc["E"].get_int64().value(),new AccountUpdate(doc)
  );
}

void marginCallUpdate(simdjson::fallback::ondemand::document_stream::iterator::value_type& doc , UserDataStream* stream) {
  stream = new UserDataStream(EventType::MARGIN_CALL, 
    doc["E"].get_int64().value(), new MarginCall(doc)
  );
}

void orderUpdate(simdjson::fallback::ondemand::document_stream::iterator::value_type& doc , UserDataStream* stream) {
  stream = new UserDataStream(EventType::ORDER_UPDATE, 
    doc["E"].get_int64().value(), 
    new OrderUpdate(doc)
);
}
