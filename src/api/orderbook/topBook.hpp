#pragma once
#include <array>
#include <cstdint>
#include <AstraLib/AstraLib.hpp>
#include "api/orderbook/threadsafeIndexmap.hpp"
#include <cmath>
class PriceLevel {
    AstraLib::Atomic::Spinlock spinlock;
    double priceLevelBase;
    uint64_t upperPrices = 0;
    uint64_t lowerPrices = 0;
    double tickRate;

    void flipBit(int index,uint64_t& toFlip,bool& wantedState) {
        if(!(( (toFlip >> index) & 1ULL ) == wantedState)) {
            uint64_t mask = (1ULL << index);
            toFlip ^= mask;   
        }
    }

    void flipBit(int index,uint64_t& toFlip) {
        uint64_t mask = (1ULL << index);
        toFlip ^= mask;
    }
    
    double bitToPrice(int bit,bool isUpper) {
        return isUpper ? tickRate*bit + priceLevelBase : tickRate*(bit+64) + priceLevelBase;
    }

    public:
    void flipPriceBit(double price,bool wantedState) {
        int baseBit = std::llround((price-priceLevelBase)/tickRate);
        if(baseBit >= 64) {
            flipBit(baseBit-64,lowerPrices,wantedState);
        } else {
            flipBit(baseBit,upperPrices,wantedState);
        }
    }

    // give a vector to get price levels until output vector is filled to given count or no more price exists
    // WARNING: If used before initializing OR clearing the level it will result in undefined behaviour due to clzll
    void findTopOfLevel(int count, std::vector<double>& outVec) {
        AstraLib::Atomic::SpinlockGuard guard(spinlock);
        uint64_t copy = upperPrices;
        bool isUpper = true;
        while (count > outVec.size()){
            if(copy != 0) {
                int bit = 63 - __builtin_clzll(copy);
                outVec.push_back(bitToPrice(bit,isUpper));
                flipBit(bit,copy);
            } else if(isUpper) {
                isUpper = false;
                copy = lowerPrices;
            } else {
                break;
            }
        }
    }
    
    // Accepted format is std::pair<double,bool> with intention double price, bool wantedState
    // Accepts both a single price level for entry and multiple
    template<typename... Args>
    void modifyPriceLevels(Args&&... args) {
        AstraLib::Atomic::SpinlockGuard guard(spinlock);
        ((flipPriceBit(args.first,args.second)),...);
        std::cout << "upper: " << upperPrices << " lower: " << lowerPrices << std::endl;
    }

    PriceLevel(double priceLevelBase_, double tickRate_) : priceLevelBase(priceLevelBase_), tickRate(tickRate_) {}
};

class SortedBook {
    // Each price level holds 128 price levels of data
    alignas(64) std::array<PriceLevel,128> bids;
    char pad0[64];
    alignas(64) std::array<PriceLevel,128> asks;
    
    AstraLib::Pools::ThreadSafeIndexPool<128> bidPool;
    AstraLib::Pools::ThreadSafeIndexPool<128> askPool;
    ThreadSafeIndexMap priceToIndex;

    void addPriceLevel(double price, bool isBid) {
        priceToIndex.addEntry(price,(isBid) ? bidPool.getIndex() : askPool.getIndex(),isBid);
    }
    

};

