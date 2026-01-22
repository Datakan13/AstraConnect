#pragma once
#include "dataModule/API/manager/lambdaFunctions/markPriceCreation.hpp"
#include "dataModule/API/manager/classDeclarations/websocketStreams/futures.hpp"
#include <AstraLib/AstraLib.hpp>
class APIManager::Futures::MarkPriceStream{
    const std::string pair;
    const std::string target = "/ws/"+pair+"@markPrice@1s";

    WebsocketStreamHolder<MarkPrice, decltype(MarkPriceCreation)>* markPriceStream;
    AstraLib::Atomic::PaddedAtomic<bool> connected = false;
    AstraLib::Atomic::PaddedAtomic<bool> connecting = true;
    public:
    AstraLib::Buffers::AtomicRingBuffer<MarkPrice,1024>& accessToMarkPriceStream() {
        return markPriceStream->bufferOut;
    }

    ConnectionStatus getStatus() {
        while(connecting.value.load(std::memory_order_acquire)) {
            _mm_pause();
        }
        if(!(connected.value.load(std::memory_order_acquire)) ) return ConnectionStatus::FAIL;
        return ConnectionStatus::SUCCESS;
    }

    MarkPriceStream(APIManager& base_,std::string pair_) : pair(pair_) {
        try {

            markPriceStream = new 
            WebsocketStreamHolder<MarkPrice,decltype(MarkPriceCreation)>
            (MarkPriceCreation,base_.hostFuturesWebsocket,target,base_.websocketParser);
            ConnectionStatus status = markPriceStream->getStatus();
            if(status == ConnectionStatus::SUCCESS) {
                connected.value.store(true,std::memory_order_release);
            } else {
                status = markPriceStream->reEstablishExecution();
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

    ~MarkPriceStream() {
        delete markPriceStream;
    }
};
