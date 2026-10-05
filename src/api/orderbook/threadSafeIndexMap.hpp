#pragma once
#include <unordered_map>
#include <atomic>
#include <thread>
#include "api/orderbook/indexedBidAsk.hpp"
#include <AstraLib/AstraLib.hpp>
#include <iostream>
#include <chrono>

class alignas(64) ThreadSafeIndexMap{
    alignas(64) std::unordered_map<double, IndexedBidAsk> priceToIndexMapBids;
    alignas(64) std::unordered_map<double, IndexedBidAsk> priceToIndexMapAsks;
    AstraLib::Atomic::Spinlock lock;
    
    public:
    // Returns true and fills `out` when the price is present.
    // One lock for the whole lookup, and `find` rather than `operator[]` so a miss
    // does not silently insert a default entry.
    bool tryGet(double price, bool isBid, IndexedBidAsk& out) {
        AstraLib::Atomic::SpinlockGuard guard(lock);
        const auto& map = isBid ? priceToIndexMapBids : priceToIndexMapAsks;
        auto it = map.find(price);
        if(it == map.end()) return false;
        out = it->second;
        return true;
    }

    bool contains(double price,bool isBid){
        AstraLib::Atomic::SpinlockGuard guard(lock);
        return (isBid) ? !(priceToIndexMapBids.find(price) == priceToIndexMapBids.end()): !(priceToIndexMapAsks.find(price) == priceToIndexMapAsks.end());
    }

    void addEntry(double price,int64_t index, bool isBid){
        AstraLib::Atomic::SpinlockGuard guard(lock);
        if(isBid) {
            priceToIndexMapBids[price].index = index;
            priceToIndexMapBids[price].isBid = isBid;
        } else {
            priceToIndexMapAsks[price].index = index;
            priceToIndexMapAsks[price].isBid = isBid;
        }
    }

    void removeEntry(double price,bool isBid) {
        AstraLib::Atomic::SpinlockGuard guard(lock);
        (isBid) ? priceToIndexMapBids.erase(price) : priceToIndexMapAsks.erase(price);
    }

    void clearMap() {
        AstraLib::Atomic::SpinlockGuard guard(lock);
        priceToIndexMapBids.clear();
        priceToIndexMapAsks.clear();
    }

    std::vector<IndexedBidAsk> returnAllIndexes(bool isBid){
        AstraLib::Atomic::SpinlockGuard guard(lock);
        std::vector<IndexedBidAsk> vec;
        for (const auto& pair : (isBid) ? priceToIndexMapBids : priceToIndexMapAsks){
            vec.emplace_back(pair.second);
        }
        return vec;
    }
};