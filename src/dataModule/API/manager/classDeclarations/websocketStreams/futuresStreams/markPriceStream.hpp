#pragma once
#include "dataModule/API/manager/lambdaFunctions/markPriceCreation.hpp"
#include "dataModule/API/manager/classDeclarations/websocketStreams/futures.hpp"

class APIManager::WebsocketStreams::Futures::MarkPriceStream{
    const std::string pair;
    const std::string target = "/ws/"+pair+"@markPrice@1s";

    WebsocketStreamHolder<MarkPrice, decltype(MarkPriceCreation)> markPriceStream;
    public:
    AstraLib::Buffers::AtomicRingBuffer<MarkPrice,1024>& accessToTradeEventStream() {
        return markPriceStream.bufferOut;
    }
    MarkPriceStream(APIManager& base_,std::string pair_) : pair(pair_) ,markPriceStream(MarkPriceCreation,base_.hostFuturesWebsocket,target,base_.websocketParser) {

    }
};
