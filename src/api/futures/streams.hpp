#pragma once
#include "manager/apiManager.hpp"

class APIManager::Futures::Streams{
    public:
    class TradeEventStream;

    class AggregatedTradeEventStream;

    class MarkPriceStream;

    class OrderStream;
};