#pragma once
#include <array>
#include <atomic>
#include <AstraLib/AstraLib.hpp>
#include "model/entry.hpp"

// Levels per side one depth event can carry. A larger event is dropped like a lost
// message, which makes the book resync.
constexpr int DEPTH_EVENT_CAPACITY = 1024;
using DepthEntries = std::array<Entry,DEPTH_EVENT_CAPACITY>;

// Backing storage for depth events in flight between the stream thread and the
// orderbook thread. A slot belongs to its event until the orderbook releases it.
// It used to be handed back as soon as the event was decoded, so a queue more than
// 127 events deep had its oldest events overwritten while still waiting.
//
// One producer (the decoder) and one consumer (the orderbook).
class OrderbookArrayHolder {
    public:
    static constexpr int SLOTS = 128;

    private:
    std::array<DepthEntries,SLOTS> bidArrays;
    std::array<DepthEntries,SLOTS> askArrays;
    AstraLib::Buffers::AtomicRingBuffer<int,SLOTS> freeSlots;
    // Counted separately so acquire() can fail instead of spinning on an empty ring
    std::atomic<int> freeCount{SLOTS};

    public:
    // Non-blocking. Returns -1 while every slot is still queued.
    int acquire() {
        if(freeCount.load(std::memory_order_acquire) == 0) return -1;
        freeCount.fetch_sub(1, std::memory_order_acq_rel);
        return freeSlots.dequeue();
    }

    // The slot is back in the ring before it is counted, so acquire() never waits on it
    void release(int slot) {
        freeSlots.enqueue(slot);
        freeCount.fetch_add(1, std::memory_order_release);
    }

    DepthEntries* bids(int slot) { return &bidArrays[slot]; }
    DepthEntries* asks(int slot) { return &askArrays[slot]; }

    OrderbookArrayHolder() {
        for(int i = 0; i < SLOTS; i++) freeSlots.enqueue(i);
    }
};

class OrderbookArrays{
    public:
    // Both null when the event was dropped: no free slot, oversized, or unparseable.
    // The orderbook treats a dropped event as a gap and resyncs.
    DepthEntries* bidArray = nullptr;
    DepthEntries* askArray = nullptr;
    int slot = -1;
    int64_t bidCount = 0;
    int64_t askCount = 0;
    int64_t u = 0;
    int64_t U = 0;
    int64_t pu = 0;

    bool dropped() const { return bidArray == nullptr; }

    OrderbookArrays(DepthEntries* bidArray_,DepthEntries* askArray_,int slot_,int64_t bidCount_,int64_t askCount_,int64_t u_,int64_t U_, int64_t pu_) :
     bidArray(bidArray_),askArray(askArray_),slot(slot_),bidCount(bidCount_),askCount(askCount_),u(u_), U(U_), pu(pu_) {
    }
    OrderbookArrays() {

    }
};
