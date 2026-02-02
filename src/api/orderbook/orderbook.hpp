#pragma once
#include <AstraLib/AstraLib.hpp>
#include "model/Entry.hpp"
#include "api/orderbook/threadsafeIndexmap.hpp"
#include "api/orderbook/orderbookSnapshotIncoming.hpp"
#include "model/error.hpp"
#include "api/orderbook/orderbookArrays.hpp"
#include "api/orderbook/DecodeEntries.hpp"
#include "core/streams/WebsocketStreamHolder.hpp"
#include "manager/apiManager.hpp"
#include "api/futures/api.hpp"
#include "api/common/error/includeErrors.hpp"
#include "core/types/Status.hpp"


class APIManager::Orderbook {
    WebsocketStreamHolder<OrderbookArrays,decltype(DecodeEntries)> websocketStream;
    OrderbookSnapshotIncoming orderbookSnapshot;
    APIManager::Futures::API& futuresApi;
    AstraLib::Atomic::Spinlock updatingLock;
    std::thread thread;
    std::string pair;
    AstraLib::Atomic::PaddedAtomic<bool> running{true};
    public:
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
        IndexedBidAsk ref = priceToIndexMap.returnIndexedBidAsk(price,isBid);
        if(ref) return PriceValid::NOT_VALID;
        out.price = bids[ref.index].price;
        out.volume = asks[ref.index].volume;
        return PriceValid::VALID;
    }

    public:
    void processEntry(Entry& entry, bool isBid) {
        int64_t index;
        bool isNewEntry = false;
        std::atomic_thread_fence(std::memory_order_acquire);

        try {
            // Check if entry exists
            IndexedBidAsk indexedBidAsk = priceToIndexMap.returnIndexedBidAsk(entry.price,isBid);
            if (!indexedBidAsk) {
                isNewEntry = true;
                if(isBid ){
                    index = freeIndexPoolBid.getIndex();
                } else {
                    index = freeIndexPoolAsk.getIndex();
                }
                priceToIndexMap.addEntry(entry.price,index,isBid);
            } else {
                index = indexedBidAsk.index;
            }

            // If volume is non-zero, process normally
            if (entry.volume != 0) {    
                auto& book = isBid ? bids : asks;
                if (isNewEntry) {
                    book[index].assignValues(entry.price, entry.volume);
                    return;
                } 
                if(indexedBidAsk.isBid == isBid) {
                    book[index].volume += entry.volume;
                } else {
                    removeEntry(index,entry.price,indexedBidAsk.isBid);
                    if(isBid ){
                        index = freeIndexPoolBid.getIndex();
                    } else {
                        index = freeIndexPoolAsk.getIndex();
                    }
                    priceToIndexMap.addEntry(entry.price,index,isBid);
                    book[index].assignValues(entry.price, entry.volume);
                }
                

            } else {
                // Volume 0 = removal
                removeEntry(index, entry.price,isBid);
            }
        } catch (std::runtime_error& e) {
            std::cout << e.what();
        }catch (std::exception& e) {
            std::cout << "Unexpected error: " << e.what();
        } 
    }

    private:
    // Removes an entry from both index map and requeues it
    void removeEntry(int64_t& index, int64_t price,bool isBid) {
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
                    std::cout << updateID << std::endl;
                    std::array<Entry,8192>& bidArrayRef = *arrays.bidArray;
                    std::array<Entry,8192>& askArrayRef = *arrays.askArray;
                    for(int i = 0; i < arrays.bidCount; i++) {
                        
                        processEntry(bidArrayRef[i],true);
                    }
                    for(int i = 0; i < arrays.askCount; i++) {
                        processEntry(askArrayRef[i],true);
                    }
                } else{
                    return FetchError(APIError::RETRY);
                }
                isFirst = false;
                return FetchError(APIError::SUCCESS);
            } catch(std::exception& e) {
                std::cout << e.what();
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
                        processEntry(askArrayRef[i],true);
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
            std::cout << *error.msg << std::endl;
            error = futuresApi.fetchOrderbookSnapshot(pair,orderbookSnapshot);
            if(error) {
                throw std::runtime_error(*error.msg);
            }
        }
        FetchError status = applySnapshot();
        if(status) {
            throw std::runtime_error(status.getErrorMsg());
        }
    }

    void executionLoop() {
        // This line eliminates the edge case of snapshot being ordered before the connection over the websocket is established.
        websocketStream.bufferOut.dequeue(); 
        FetchError error = futuresApi.fetchOrderbookSnapshot(pair,orderbookSnapshot);
        if(error) {
            std::cout << *error.msg << std::endl;
            error = futuresApi.fetchOrderbookSnapshot(pair,orderbookSnapshot);
            if(error) {
                throw std::runtime_error(*error.msg);
            }
        }
        FetchError status = applySnapshot();
        if(status) {
            throw std::runtime_error(status.getErrorMsg());
        }
        while(running.value.load(std::memory_order_acquire)) {
            status = applyWebsocket();
            while(status.error == APIError::RETRY) {
                status = applyWebsocket();
                if(status.error == APIError::ORDERBOOK_OUTDATED) {
                    orderbookInitiate();
                }
                if(status.error == APIError::UNKNOWN) {
                    throw std::runtime_error(status.getErrorMsg());
                }
            }
            std::cout << "Orderbook initiated" << std::endl;
            for(;;) {
                status = applyWebsocket(false);
                if(status) {
                    if(status.error == APIError::ORDERBOOK_OUTDATED){
                        std::cout << status.getErrorMsg() << std::endl;
                        break;
                    } else {
                        std::cout << status.getErrorMsg() << std::endl;
                        break;
                    }
                }
            }
            error = futuresApi.fetchOrderbookSnapshot(pair,orderbookSnapshot);
            if(error) {
                std::cout << *error.msg << std::endl;
                error = futuresApi.fetchOrderbookSnapshot(pair,orderbookSnapshot);
                if(error) {
                    std::cout << error.getErrorMsg()<< std::endl;
                }
            }
            status = applySnapshot();
            if(status) {
                std::cout << status.getErrorMsg()<< std::endl;
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