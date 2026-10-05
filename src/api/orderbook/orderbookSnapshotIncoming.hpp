#pragma once
#include <AstraLib/AstraLib.hpp>
#include "model/entry.hpp"
class OrderbookSnapshotIncoming {
    std::array<Entry,8192> bidArray;
    std::array<Entry,8192> askArray;
    // Both were uninitialised: registerEntry indexes with them on the very first
    // snapshot entry, so a garbage value wrote out of bounds.
    int64_t bidCount{0};
    int64_t askCount{0};
    int64_t lastBidGiven{0};
    int64_t lastAskGiven{0};
    AstraLib::Atomic::PaddedAtomic<int> lastGivenIndex{0};
    public:
    int64_t updateId;
    void registerEntry(double price, double volume,bool isBid){
        if(isBid) {
            // Drop the overflow rather than wrapping to 0, which used to overwrite
            // the top of the book with the tail of an oversized snapshot.
            if(bidCount >= 8192) return;
            bidArray[bidCount].price = price;
            bidArray[bidCount].volume = volume;
            bidCount++;
        } else {
            if(askCount >= 8192) return;
            askArray[askCount].price = price;
            askArray[askCount].volume = volume;
            askCount++;
        }
    } 

    std::pair<Entry,Entry> getEntries() {
        int index = lastGivenIndex.value.fetch_add(1,std::memory_order_acq_rel);
        return {bidArray[index],askArray[index]};
    }

    // Both return a zero Entry once drained; callers stop on price == 0.
    Entry getBid() {
        if(lastBidGiven >= bidCount) return Entry();
        return bidArray[lastBidGiven++];
    }

    Entry getAsk() {
        if(lastAskGiven >= askCount) return Entry();
        return askArray[lastAskGiven++];
    }

    // Clears the buffer so a re-sync starts from an empty snapshot.
    // Nothing used to reset the read cursors, so the second and every later
    // snapshot resumed where the first stopped, read zeros, and applied nothing:
    // the book could never recover from a dropped update.
    void reset() {
        bidCount = 0;
        askCount = 0;
        lastBidGiven = 0;
        lastAskGiven = 0;
        lastGivenIndex.value.store(0,std::memory_order_release);
    }

};