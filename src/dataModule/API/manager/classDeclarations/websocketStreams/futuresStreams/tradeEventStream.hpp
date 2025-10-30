#pragma once
#include "futures.hpp"
#include "TradeEventCreation.hpp"

class APIManager::WebsocketStreams::Futures::TradeEventStream{
        std::string pair;
        const std::string target = "/ws/"+ pair +"@trade";
        WebsocketStreamHolder<TradeEvent,decltype(TradeEventCreation)>* tradeEventStream;
        public:
        AstraLib::Buffers::AtomicRingBuffer<TradeEvent,1024>& accessToTradeEventStream() {
            return tradeEventStream->bufferOut; 
        }

        TradeEventStream(APIManager& base_, std::string pair_) : pair(pair_) {
            std::cout << target << std::endl;
            try {
                tradeEventStream = new 
                WebsocketStreamHolder<TradeEvent,decltype(TradeEventCreation)>
                (TradeEventCreation,base_.hostFuturesWebsocket,target,base_.websocketParser);
                ConnectionStatus status = tradeEventStream->getStatus();
                
            } catch(std::runtime_error& e) {

            } catch(std::exception& e) {

            }
            
        }   

        ~TradeEventStream() {
            delete tradeEventStream;
        }
    };
