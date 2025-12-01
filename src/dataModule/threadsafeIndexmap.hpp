#pragma once
#include <unordered_map>
#include <atomic>
#include <thread>
#include "dataModule/orderbook/indexedBidAsk.hpp"
#include <AstraLib/AstraLib.hpp>
#include <iostream>
#include <chrono>
class alignas(64) ThreadSafeIndexMap{
    alignas(64) std::unordered_map<double, IndexedBidAsk> priceToIndexMap;
    AstraLib::Atomic::Spinlock lock;
    

    public:
    IndexedBidAsk returnIndexedBidAsk(double price) {
        if(!contains(price)) return IndexedBidAsk();

        IndexedBidAsk result = priceToIndexMap[price];

        return result;
    }

    bool contains(double price){
        
        AstraLib::Atomic::SpinlockGuard guard(lock);
        return priceToIndexMap.find(price) == priceToIndexMap.end();
    }

    bool addEntry(double price,double index, bool isBid){
        AstraLib::Atomic::SpinlockGuard guard(lock);
        priceToIndexMap[price].index = index;
        priceToIndexMap[price].isBid = isBid;
        return true;
    }

    bool removeEntry(double price) {
        AstraLib::Atomic::SpinlockGuard guard(lock);
        priceToIndexMap.erase(price);
        return true;
    }

    bool clearMap() {
        AstraLib::Atomic::SpinlockGuard guard(lock);
        priceToIndexMap.clear();
        return true;
    }

    std::vector<IndexedBidAsk> returnAllIndexes(){
        AstraLib::Atomic::SpinlockGuard guard(lock);
        std::vector<IndexedBidAsk> vec;
        for (const auto& pair : priceToIndexMap ){
            vec.emplace_back(pair.second);
        }
        return vec;
    }
};