#pragma once
#include <memory>
#include "api/futures/streams.hpp"
#include "core/streams/websocketStreamHolder.hpp"
#include "core/types/status.hpp"
#include "api/common/factory/tradeEventFactory.hpp"

class APIManager::Futures::Streams::TradeEventStream{
        std::string pair;
        const std::string target = "/ws/"+ pair +"@trade";
        std::unique_ptr<WebsocketStreamHolder<TradeEvent,decltype(parseTradeEvent)>> tradeEventStream;
        AstraLib::Atomic::PaddedAtomic<bool> connected = false;
        AstraLib::Atomic::PaddedAtomic<bool> connecting = true;
        // Owned by this class. Holds the reason propagated up from the stream holder.
        std::unique_ptr<FetchError> lastError;
        // FAIL (could not connect) vs CLOSED (holder is shutting down)
        AstraLib::Atomic::PaddedAtomic<ConnectionStatus> lastStatus{ConnectionStatus::FAIL};
        public:
        AstraLib::Buffers::AtomicRingBuffer<TradeEvent,1024>& accessToTradeEventStream() {
            return tradeEventStream->bufferOut; 
        }

        // Returns the connection status paired with the reason it failed.
        // The FetchError* is owned by this class and is reset on every connection attempt.
        // On SUCCESS the pointer is nullptr.
        std::pair<ConnectionStatus,FetchError*> getStatus() {
            while(connecting.value.load(std::memory_order_acquire)) {
                _mm_pause();
            }
            if(!(connected.value.load(std::memory_order_acquire)) ) return {lastStatus.value.load(std::memory_order_acquire), lastError.get()};
            return {ConnectionStatus::SUCCESS, nullptr};
        }

        TradeEventStream(APIManager& base_, std::string pair_) : pair(pair_) {
            try {
                tradeEventStream = std::make_unique<
                WebsocketStreamHolder<TradeEvent,decltype(parseTradeEvent)>>
                (parseTradeEvent,base_.hostFuturesWebsocket,target,base_.websocketParser);
                auto [status, err] = tradeEventStream->getStatus();
                if(status == ConnectionStatus::SUCCESS) {
                    connected.value.store(true,std::memory_order_release);
                } else if(status == ConnectionStatus::CLOSED) {
                    // holder is shutting down, retrying would only fight the teardown
                    lastStatus.value.store(ConnectionStatus::CLOSED,std::memory_order_release);
                    if(err) lastError = std::make_unique<FetchError>(err->error,err->getErrorMsg());
                    connecting.value.store(false,std::memory_order_release);
                    connected.value.store(false,std::memory_order_release);
                    return;
                } else {
                    status = tradeEventStream->reEstablishExecution();
                    if(status == ConnectionStatus::SUCCESS) {
                        connected.value.store(true,std::memory_order_release);
                    } else {
                        // adopt the reason from down the stream before a reconnect can reset it
                        auto [retryStatus, retryErr] = tradeEventStream->getStatus();
                        lastStatus.value.store(retryStatus,std::memory_order_release);
                        FetchError* src = retryErr ? retryErr : err;
                        if(src) lastError = std::make_unique<FetchError>(src->error,src->getErrorMsg());
                        connecting.value.store(false,std::memory_order_release);
                        connected.value.store(false,std::memory_order_release);
                        return;
                    }
                }
                connecting.value.store(false,std::memory_order_release);
            } catch(std::runtime_error& e) {
                lastError = std::make_unique<FetchError>(APIError::BOOST_ERROR,std::string(e.what()));
                connecting.value.store(false,std::memory_order_release);
            } catch(std::exception& e) {
                lastError = std::make_unique<FetchError>(APIError::BOOST_ERROR,std::string(e.what()));
                connecting.value.store(false,std::memory_order_release);
            }
            
        }   

        ~TradeEventStream() {
            }
    };
