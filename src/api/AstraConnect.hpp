#pragma once
// Core manager
#include "manager/apiManager.hpp"

// Futures API 
#include "api/futures/api.hpp"

// Futures streams
#include "api/futures/streams.hpp"
#include "api/futures/streams/aggregatedTradeEventStream.hpp"
#include "api/futures/streams/markPriceStream.hpp"
#include "api/futures/streams/tradeEventStream.hpp"

// Order Stream
#include "api/futures/order/orderStream.hpp"

// Spot API
#include "api/spot/api.hpp"

// User data streams
#include "api/user/userDataStreams.hpp"
#include "api/user/streams/userFuturesStream.hpp"
#include "api/user/streams/userSpotStream.hpp"

// Orderbook
#include "api/orderbook/orderbook.hpp"