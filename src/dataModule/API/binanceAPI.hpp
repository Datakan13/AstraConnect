#pragma once
// Core manager
#include "manager/apiManager.hpp"

// Futures API 
#include "manager/classDeclarations/futuresAPI.hpp"

// Order Stream
#include "manager/classDeclarations/OrderStream.hpp"

// Spot API
#include "manager/classDeclarations/spotAPI.hpp"

// User data streams
#include "manager/classDeclarations/userDataStreams.hpp"
#include "manager/classDeclarations/userDataStreams/userFuturesStream.hpp"
#include "manager/classDeclarations/userDataStreams/userSpotStream.hpp"

// Websocket streams
#include "manager/classDeclarations/websocketStreams.hpp"
#include "manager/classDeclarations/websocketStreams/futures.hpp"
#include "manager/classDeclarations/websocketStreams/futuresStreams/AggregatedTradeEventStream.hpp"
#include "manager/classDeclarations/websocketStreams/futuresStreams/markPriceStream.hpp"
#include "manager/classDeclarations/websocketStreams/futuresStreams/tradeEventStream.hpp"
