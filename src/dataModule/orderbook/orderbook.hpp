#pragma once
#include <AstraLib/AstraLib.hpp>
#include "dataModule/dataTypes/Entry.hpp"
#include "dataModule/threadsafeIndexmap.hpp"
#include "dataModule/API/helperClasses/orderbookSnapshotIncoming.hpp"
#include "dataModule/error.hpp"
#include "dataModule/API/binanceAPI.hpp"
#include "dataModule/orderbook/orderbookArrays.hpp"
#include "dataModule/orderbook/DecodeEntries.hpp"


class Orderbook {
    WebsocketStreamHolder<OrderbookArrays,decltype(DecodeEntries)> websocketStream;
    OrderbookSnapshotIncoming orderbookSnapshot;
    APIManager::FuturesAPI& futuresApi;
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
    AstraLib::Pools::ThreadSafeIndexPool<8192> freeIndexPool;

    PriceValid getEntryWithPrice(double price,Entry& out) {
        AstraLib::Atomic::SpinlockGuard guard(updatingLock);
        IndexedBidAsk ref = priceToIndexMap.returnIndexedBidAsk(price);
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
            
            if (priceToIndexMap.returnIndexedBidAsk(entry.price)) {
                isNewEntry = true;
                index = freeIndexPool.getIndex();
                priceToIndexMap.addEntry(entry.price,index,isBid);
            }

            // If volume is non-zero, process normally
            if (entry.volume != 0) {
                auto& book = isBid ? bids : asks;

                if (isNewEntry) {
                    book[index].assignValues(entry.price, entry.volume);
                    
                } else {
                    
                    book[index].price += entry.price;
                    book[index].volume += entry.volume;
                }

            } else {
                // Volume 0 = removal
                removeEntry(index, entry.price);
            }
        } catch (std::runtime_error& e) {
            std::cout << e.what();
        }catch (std::exception& e) {
            std::cout << "Unexpected error: " << e.what();
        } 
    }

    private:
    // Removes an entry from both index map and requeues it
    void removeEntry(int64_t& index, int64_t price) {
        priceToIndexMap.removeEntry(price);
        freeIndexPool.returnIndex(index);
    }

    FetchError applySnapshot() {
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
    
    public:
    Orderbook(APIManager& base_,std::string pair_,APIManager::FuturesAPI& api) : pair(pair_),
    websocketStream(DecodeEntries,base_.hostFuturesWebsocket,"/stream?streams="+ pair_ +"@depth",base_.websocketParser),
    futuresApi(api){
        thread = std::thread(&Orderbook::executionLoop,this);
    };
    ~Orderbook() {
        running.value.store(false, std::memory_order_release);
        if(thread.joinable()) thread.join();
    }
};