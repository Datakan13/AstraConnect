#pragma once
#include "dataModule/API/manager/classDeclarations/websocketStreams/futures.hpp"
#include "dataModule/API/manager/lambdaFunctions/TradeEventCreation.hpp"

class APIManager::Futures::TradeEventStream{
        std::string pair;
        const std::string target = "/ws/"+ pair +"@trade";
        WebsocketStreamHolder<TradeEvent,decltype(TradeEventCreation)>* tradeEventStream;
        AstraLib::Atomic::PaddedAtomic<bool> connected = false;
        AstraLib::Atomic::PaddedAtomic<bool> connecting = true;
        public:
        AstraLib::Buffers::AtomicRingBuffer<TradeEvent,1024>& accessToTradeEventStream() {
            return tradeEventStream->bufferOut; 
        }

        ConnectionStatus getStatus() {
            while(connecting.value.load(std::memory_order_acquire)) {
                _mm_pause();
            }
            if(!(connected.value.load(std::memory_order_acquire)) ) return ConnectionStatus::FAIL;
            return ConnectionStatus::SUCCESS;
        }

        TradeEventStream(APIManager& base_, std::string pair_) : pair(pair_) {
            std::cout << target << std::endl;
            try {
                tradeEventStream = new 
                WebsocketStreamHolder<TradeEvent,decltype(TradeEventCreation)>
                (TradeEventCreation,base_.hostFuturesWebsocket,target,base_.websocketParser);
                ConnectionStatus status = tradeEventStream->getStatus();
                if(status == ConnectionStatus::SUCCESS) {
                    connected.value.store(true,std::memory_order_release);
                } else {
                    status = tradeEventStream->reEstablishExecution();
                    if(status == ConnectionStatus::SUCCESS) {
                        connected.value.store(true,std::memory_order_release);
                    } else {
                        connecting.value.store(false,std::memory_order_release);
                        connected.value.store(false,std::memory_order_release);
                        return;
                    }
                }
                connecting.value.store(false,std::memory_order_release);
            } catch(std::runtime_error& e) {
                connecting.value.store(false,std::memory_order_release);
            } catch(std::exception& e) {
                connecting.value.store(false,std::memory_order_release);
            }
            
        }   

        ~TradeEventStream() {
            delete tradeEventStream;
        }
    };
