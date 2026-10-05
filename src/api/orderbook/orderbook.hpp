#pragma once
#include <memory>
#include <AstraLib/AstraLib.hpp>
#include "model/entry.hpp"
#include "api/orderbook/threadSafeIndexMap.hpp"
#include "api/orderbook/orderbookSnapshotIncoming.hpp"
#include "model/error.hpp"
#include "api/orderbook/orderbookArrays.hpp"
#include "api/orderbook/decodeEntries.hpp"
#include "core/streams/websocketStreamHolder.hpp"
#include "manager/apiManager.hpp"
#include "api/futures/api.hpp"
#include "api/common/error/includeErrors.hpp"
#include "core/types/status.hpp"


class APIManager::Orderbook {
    WebsocketStreamHolder<OrderbookArrays,decltype(DecodeEntries)> websocketStream;
    OrderbookSnapshotIncoming orderbookSnapshot;
    APIManager::Futures::API& futuresApi;
    AstraLib::Atomic::Spinlock updatingLock;
    std::thread thread;
    std::string pair;
    AstraLib::Atomic::PaddedAtomic<bool> running{true};
    // Why the book stopped. Owned here; executionLoop has no caller to return to.
    std::unique_ptr<FetchError> lastError;

    // Records why we are stopping and halts the loop. Throwing from executionLoop
    // escaped the thread and called std::terminate instead of reaching anyone.
    void stop(FetchError err) {
        lastError = std::make_unique<FetchError>(std::move(err));
        running.value.store(false,std::memory_order_release);
    }
    public:
    // The reason the book stopped, or nullptr while it is still running.
    FetchError* error() {
        return lastError.get();
    }

    int64_t updateID;
    // Orderbook arrays
    alignas(64) std::array<Entry, 8192> bids;
    char pad0[64];
    alignas(64) std::array<Entry, 8192> asks;
    
    // Maps price -> array index
    ThreadSafeIndexMap priceToIndexMap;

    private:
    // Free index pool for depth entries
    AstraLib::Pools::ThreadSafeIndexPool<8192> freeIndexPoolBid;
    AstraLib::Pools::ThreadSafeIndexPool<8192> freeIndexPoolAsk;

    PriceValid getEntryWithPrice(double price,Entry& out,bool isBid) {
        AstraLib::Atomic::SpinlockGuard guard(updatingLock);
        IndexedBidAsk ref;
        if(!priceToIndexMap.tryGet(price,isBid,ref)) return PriceValid::NOT_VALID;
        const auto& book = isBid ? bids : asks;
        out.price = book[ref.index].price;
        out.volume = book[ref.index].volume;
        return PriceValid::VALID;
    }

    public:
    void processEntry(Entry& entry, bool isBid) {
        std::atomic_thread_fence(std::memory_order_acquire);

        IndexedBidAsk existing;
        const bool found = priceToIndexMap.tryGet(entry.price,isBid,existing);

        // A depth event carries the ABSOLUTE quantity for a level, so quantity 0
        // means remove it. Binance documents that a removal can arrive for a level
        // we never had, which is normal and must be ignored rather than handing a
        // bogus index back to the pool.
        if(entry.volume == 0) {
            if(found) removeEntry(existing.index, entry.price, isBid);
            return;
        }

        int64_t index;
        if(found) {
            index = existing.index;
        } else {
            index = isBid ? freeIndexPoolBid.getIndex() : freeIndexPoolAsk.getIndex();
            priceToIndexMap.addEntry(entry.price,index,isBid);
        }

        // Absolute quantity: assign, never accumulate.
        auto& book = isBid ? bids : asks;
        book[index].assignValues(entry.price, entry.volume);
    }

    private:
    // Removes an entry from both index map and requeues it
    // `price` must stay a double: taking it as int64_t truncated 86106.90 to 86106,
    // so the map erase missed and the index went back to the pool while the map
    // still pointed at it.
    void removeEntry(int64_t index, double price,bool isBid) {
        priceToIndexMap.removeEntry(price,isBid);
        if(isBid){
            freeIndexPoolBid.returnIndex(index);
        } else {
            freeIndexPoolAsk.returnIndex(index);
        }
    }

    FetchError applySnapshot() {
        AstraLib::Atomic::SpinlockGuard guard(updatingLock);
        try {
            Entry lastEntry(1,1);
            while(lastEntry.price != 0) {
                lastEntry = orderbookSnapshot.getBid();
                if(lastEntry.price != 0) processEntry(lastEntry,true);
            }
            lastEntry.price = 1;
            while(lastEntry.price != 0) {
                lastEntry = orderbookSnapshot.getAsk();
                if(lastEntry.price != 0) processEntry(lastEntry,false);
            }
            updateID = orderbookSnapshot.updateId;
            return FetchError(APIError::SUCCESS);
        } catch(std::exception& e) {
            return FetchError(APIError::UNKNOWN,e.what());
        }
        
    }

    FetchError applyWebsocket(bool isFirst = true) {
        AstraLib::Atomic::SpinlockGuard guard(updatingLock);
        // first update
        if(isFirst) {
            try {
                OrderbookArrays arrays= websocketStream.bufferOut.dequeue();
                // Drop any outdated updates
                if(arrays.u < updateID) {
                    return APIError::RETRY;
                }
                // Apply the actual update
                if(arrays.U  <= updateID&& arrays.u >= updateID + 1){
                    updateID = arrays.u;
                    std::array<Entry,8192>& bidArrayRef = *arrays.bidArray;
                    std::array<Entry,8192>& askArrayRef = *arrays.askArray;
                    for(int i = 0; i < arrays.bidCount; i++) {
                        
                        processEntry(bidArrayRef[i],true);
                    }
                    for(int i = 0; i < arrays.askCount; i++) {
                        processEntry(askArrayRef[i],false);
                    }
                } else{
                    return FetchError(APIError::RETRY);
                }
                return FetchError(APIError::SUCCESS);
            } catch(std::exception& e) {
                return FetchError(APIError::UNKNOWN,e.what());
            }
        } else {
            try{
                OrderbookArrays arrays = websocketStream.getLatestData();
                if(arrays.pu == updateID){
                    std::array<Entry,8192>& bidArrayRef = *arrays.bidArray;
                    std::array<Entry,8192>& askArrayRef = *arrays.askArray;
                    updateID = arrays.u;
                    for(int i = 0; i < arrays.bidCount; i++) {
                        processEntry(bidArrayRef[i],true);
                    }
                    for(int i = 0; i < arrays.askCount; i++) {
                        processEntry(askArrayRef[i],false);
                    }
                    return FetchError(APIError::SUCCESS);
                } else {
                    return FetchError(APIError::ORDERBOOK_OUTDATED);
                }
            } catch(std::exception& e) {
                return FetchError(APIError::UNKNOWN,e.what());
            }
        }
        
    }

    void orderbookInitiate() {
        FetchError error = futuresApi.fetchOrderbookSnapshot(pair,orderbookSnapshot);
        if(error) {
            error = futuresApi.fetchOrderbookSnapshot(pair,orderbookSnapshot);
            if(error) {
                stop(std::move(error)); return;
            }
        }
        FetchError status = applySnapshot();
        if(status) {
            stop(std::move(status)); return;
        }
    }

    void executionLoop() {
        // This line eliminates the edge case of snapshot being ordered before the connection over the websocket is established.
        websocketStream.bufferOut.dequeue(); 
        FetchError error = futuresApi.fetchOrderbookSnapshot(pair,orderbookSnapshot);
        if(error) {
            error = futuresApi.fetchOrderbookSnapshot(pair,orderbookSnapshot);
            if(error) {
                stop(std::move(error)); return;
            }
        }
        FetchError status = applySnapshot();
        if(status) {
            stop(std::move(status)); return;
        }
        while(running.value.load(std::memory_order_acquire)) {
            status = applyWebsocket();
            while(status.error == APIError::RETRY) {
                status = applyWebsocket();
                if(status.error == APIError::ORDERBOOK_OUTDATED) {
                    orderbookInitiate();
                }
                if(status.error == APIError::UNKNOWN) {
                    stop(std::move(status)); return;
                }
            }
            for(;;) {
                status = applyWebsocket(false);
                if(status) {
                    if(status.error == APIError::ORDERBOOK_OUTDATED){
                        break;
                    } else {
                        break;
                    }
                }
            }
            error = futuresApi.fetchOrderbookSnapshot(pair,orderbookSnapshot);
            if(error) {
                    error = futuresApi.fetchOrderbookSnapshot(pair,orderbookSnapshot);
                if(error) {
                }
            }
            status = applySnapshot();
            if(status) {
            }
        }
    }
    
    void getFullOrderbook() {
        AstraLib::Atomic::SpinlockGuard guard(updatingLock);
        
    }

    public:
    Orderbook(APIManager& base_,std::string pair_,APIManager::Futures::API& api) : pair(pair_),
    websocketStream(DecodeEntries,base_.hostFuturesWebsocket,"/stream?streams="+ pair_ +"@depth",base_.websocketParser),
    futuresApi(api){
        thread = std::thread(&Orderbook::executionLoop,this);
    };
    ~Orderbook() {
        running.value.store(false, std::memory_order_release);
        if(thread.joinable()) thread.join();
    }
};