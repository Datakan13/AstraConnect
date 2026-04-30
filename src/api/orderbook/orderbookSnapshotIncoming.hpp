#pragma once
#include <AstraLib/AstraLib.hpp>
#include "model/entry.hpp"
class OrderbookSnapshotIncoming {
    std::array<Entry,8192> bidArray;
    std::array<Entry,8192> askArray;
    int64_t bidCount;
    int64_t askCount;
    int64_t lastBidGiven{0};
    int64_t lastAskGiven{0};
    AstraLib::Atomic::PaddedAtomic<int> lastGivenIndex{0};
    public:
    int64_t updateId;
    void registerEntry(double price, double volume,bool isBid){
        if(isBid) {
            if(bidCount>= 8192) bidCount = 0;
            bidArray[bidCount].price = price;
            bidArray[bidCount].volume = volume;
            bidCount++;
        } else {
            if(askCount>= 8192) askCount = 0;
            askArray[askCount].price = price;
            askArray[askCount].volume = volume;
            askCount++;
        }
    } 

    std::pair<Entry,Entry> getEntries() {
        int index = lastGivenIndex.value.fetch_add(1,std::memory_order_acq_rel);
        return {bidArray[index],askArray[index]};
    }

    Entry getBid() {
        return bidArray[lastBidGiven++];
    }

    Entry getAsk() {
        return askArray[lastAskGiven++];
    }

};