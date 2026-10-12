#pragma once
#include <array>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include "model/entry.hpp"
#include "utils/decimal/fixedPoint.hpp"

// 128 consecutive ticks of one side. A set bit means a level exists at that tick, and
// slots maps the tick to the level's index in the side's Entry array.
struct PriceLevel {
    static constexpr int TICKS = 128;

    // bits[0] covers offsets 0-63, bits[1] offsets 64-127, so a higher bit is always a
    // higher price. The old layout searched 0-63 before 64-127 and returned prices
    // out of order whenever a level spanned both words.
    std::array<uint64_t,2> bits{};
    std::array<int16_t,TICKS> slots;
    int64_t blockId = 0;

    bool empty() const { return (bits[0] | bits[1]) == 0; }
    bool test(int offset) const { return (bits[offset >> 6] >> (offset & 63)) & 1ULL; }
    void set(int offset) { bits[offset >> 6] |= 1ULL << (offset & 63); }
    void reset(int offset) { bits[offset >> 6] &= ~(1ULL << (offset & 63)); }

    // Calls fn(slot) for each level, highest price first when Descending, until fn
    // returns false. Returns false if fn stopped the walk.
    template<bool Descending, typename Fn>
    bool forEach(Fn&& fn) const {
        if constexpr (Descending) {
            for(int word = 1; word >= 0; word--) {
                uint64_t remaining = bits[word];
                while(remaining) {
                    int bit = 63 - __builtin_clzll(remaining);
                    remaining &= ~(1ULL << bit);
                    if(!fn(slots[word * 64 + bit])) return false;
                }
            }
        } else {
            for(int word = 0; word <= 1; word++) {
                uint64_t remaining = bits[word];
                while(remaining) {
                    int bit = __builtin_ctzll(remaining);
                    remaining &= remaining - 1;
                    if(!fn(slots[word * 64 + bit])) return false;
                }
            }
        }
        return true;
    }
};

// One side of the book, kept sorted by price as bits.
//
// A price becomes a tick (price / tickSize, both in 1e-8 units), the tick a block id (tick / 128) and an
// offset in that block. Blocks inside the window are reached by direct index; blocks
// outside it sit in `fallback`, sorted by block id. When a level lands beyond the best
// edge of the window, or the window empties, the window recentres by moving pointers
// between the two. Levels themselves never move.
//
// Not thread safe: the orderbook serialises writers and readers with one lock.
template<bool IsBid>
class SortedBook {
    public:
    static constexpr int WINDOW = 128;          // blocks: 16384 ticks
    static constexpr int MAX_LEVELS = 8192;     // price levels per side
    static constexpr int MAX_BLOCKS = 1024;

    private:
    AstraConnect::Utils::Fixed tickSize = 0;
    int64_t windowBase = 0;                     // block id of window[0]
    bool centred = false;
    int windowBlocks = 0;                       // non-null entries in window

    std::array<PriceLevel*,WINDOW> window{};
    std::vector<PriceLevel*> fallback;          // reserved up front, never reallocates
    std::vector<PriceLevel*> scratch;           // used by recentre

    std::array<PriceLevel,MAX_BLOCKS> blocks;
    std::array<int16_t,MAX_BLOCKS> freeBlocks;
    int freeBlockCount = 0;

    std::array<Entry,MAX_LEVELS> entries;
    std::array<int16_t,MAX_LEVELS> freeSlots;
    int freeSlotCount = 0;

    // A bid beyond the window's top, or an ask beyond its bottom, is a new best edge
    static constexpr bool beyondBestEdge(int64_t blockId, int64_t base) {
        return IsBid ? blockId >= base + WINDOW : blockId < base;
    }

    // Exchange prices are tick multiples, so this is exact; anything else rounds
    // to the nearest tick, as llround(price / tickSize) did with doubles.
    int64_t toTick(AstraConnect::Utils::Fixed price) const {
        return (price + tickSize / 2) / tickSize;
    }

    std::vector<PriceLevel*>::iterator fallbackFind(int64_t blockId) {
        return std::lower_bound(fallback.begin(), fallback.end(), blockId,
            [](const PriceLevel* level, int64_t id) { return level->blockId < id; });
    }

    PriceLevel* findBlock(int64_t blockId) {
        int64_t offset = blockId - windowBase;
        if(offset >= 0 && offset < WINDOW) return window[offset];
        auto it = fallbackFind(blockId);
        return (it != fallback.end() && (*it)->blockId == blockId) ? *it : nullptr;
    }

    void place(PriceLevel* level) {
        int64_t offset = level->blockId - windowBase;
        if(offset >= 0 && offset < WINDOW) {
            window[offset] = level;
            windowBlocks++;
        } else {
            fallback.insert(fallbackFind(level->blockId), level);
        }
    }

    // Moves every block to its container for the new window. Pointers only.
    void recentre(int64_t centreBlock) {
        scratch.clear();
        for(PriceLevel*& level : window) {
            if(level) scratch.push_back(level);
            level = nullptr;
        }
        scratch.insert(scratch.end(), fallback.begin(), fallback.end());
        fallback.clear();
        windowBlocks = 0;
        windowBase = centreBlock - WINDOW / 2;
        centred = true;
        std::sort(scratch.begin(), scratch.end(),
            [](const PriceLevel* a, const PriceLevel* b) { return a->blockId < b->blockId; });
        for(PriceLevel* level : scratch) place(level);
    }

    PriceLevel* acquireBlock(int64_t blockId) {
        if(freeBlockCount == 0) return nullptr;
        PriceLevel* level = &blocks[freeBlocks[--freeBlockCount]];
        level->blockId = blockId;
        // The level must be in place before recentring, or recentre would not see it
        if(!centred || beyondBestEdge(blockId, windowBase)) {
            place(level);
            recentre(blockId);
        } else {
            place(level);
        }
        return level;
    }

    void releaseBlock(PriceLevel* level) {
        int64_t offset = level->blockId - windowBase;
        if(offset >= 0 && offset < WINDOW) {
            window[offset] = nullptr;
            windowBlocks--;
        } else {
            fallback.erase(fallbackFind(level->blockId));
        }
        freeBlocks[freeBlockCount++] = static_cast<int16_t>(level - blocks.data());
        // Price has moved off the window entirely: follow it to the best remaining block
        if(windowBlocks == 0 && !fallback.empty()) {
            recentre(IsBid ? fallback.back()->blockId : fallback.front()->blockId);
        }
    }

    public:
    SortedBook() {
        fallback.reserve(MAX_BLOCKS);
        scratch.reserve(MAX_BLOCKS);
        clear();
    }

    void setTickSize(AstraConnect::Utils::Fixed tickSize_) { tickSize = tickSize_; }
    AstraConnect::Utils::Fixed getTickSize() const { return tickSize; }

    // Drops every level. The window recentres on the first level added afterwards.
    void clear() {
        for(PriceLevel& level : blocks) level.bits = {};
        window.fill(nullptr);
        fallback.clear();
        windowBlocks = 0;
        centred = false;
        freeBlockCount = MAX_BLOCKS;
        for(int i = 0; i < MAX_BLOCKS; i++) freeBlocks[i] = static_cast<int16_t>(MAX_BLOCKS - 1 - i);
        freeSlotCount = MAX_LEVELS;
        for(int i = 0; i < MAX_LEVELS; i++) freeSlots[i] = static_cast<int16_t>(MAX_LEVELS - 1 - i);
    }

    // Applies one absolute level from a snapshot or depth diff; volume 0 removes it.
    // Returns false when a new level does not fit; the book is left unchanged.
    bool update(AstraConnect::Utils::Fixed price, AstraConnect::Utils::Fixed volume) {
        const int64_t tick = toTick(price);
        const int64_t blockId = tick >> 7;
        const int offset = static_cast<int>(tick & 127);

        PriceLevel* level = findBlock(blockId);
        const bool exists = level && level->test(offset);

        // Binance documents removals for levels we never had; they are ignored
        if(volume == 0) {
            if(exists) {
                level->reset(offset);
                freeSlots[freeSlotCount++] = level->slots[offset];
                if(level->empty()) releaseBlock(level);
            }
            return true;
        }

        if(exists) {
            entries[level->slots[offset]].assignValues(price, volume);
            return true;
        }

        if(freeSlotCount == 0) return false;
        if(!level) {
            level = acquireBlock(blockId);
            if(!level) return false;
        }
        int16_t slot = freeSlots[--freeSlotCount];
        level->slots[offset] = slot;
        level->set(offset);
        entries[slot].assignValues(price, volume);
        return true;
    }

    // Looks a price up directly through its block. Returns false if there is no level.
    bool find(AstraConnect::Utils::Fixed price, Entry& out) {
        const int64_t tick = toTick(price);
        PriceLevel* level = findBlock(tick >> 7);
        const int offset = static_cast<int>(tick & 127);
        if(!level || !level->test(offset)) return false;
        out = entries[level->slots[offset]];
        return true;
    }

    // Calls fn(entry) for each level, best first (highest bid, lowest ask), until fn
    // returns false.
    template<typename Fn>
    void forEachBest(Fn&& fn) {
        // Fallback splits around the window: [0, low) below it, [high, end) above it
        auto low = fallbackFind(windowBase);
        auto high = fallbackFind(windowBase + WINDOW);
        auto visit = [&](int16_t slot) { return fn(static_cast<const Entry&>(entries[slot])); };

        if constexpr (IsBid) {
            for(auto it = fallback.end(); it != high;) {
                if(!(*--it)->template forEach<true>(visit)) return;
            }
            for(int i = WINDOW - 1; i >= 0; i--) {
                if(window[i] && !window[i]->template forEach<true>(visit)) return;
            }
            for(auto it = low; it != fallback.begin();) {
                if(!(*--it)->template forEach<true>(visit)) return;
            }
        } else {
            for(auto it = fallback.begin(); it != low; ++it) {
                if(!(*it)->template forEach<false>(visit)) return;
            }
            for(int i = 0; i < WINDOW; i++) {
                if(window[i] && !window[i]->template forEach<false>(visit)) return;
            }
            for(auto it = high; it != fallback.end(); ++it) {
                if(!(*it)->template forEach<false>(visit)) return;
            }
        }
    }

    // Copies up to `count` levels into out, best first. Returns how many were written.
    size_t top(Entry* out, size_t count) {
        size_t written = 0;
        if(count == 0) return 0;
        forEachBest([&](const Entry& entry) {
            out[written++] = entry;
            return written < count;
        });
        return written;
    }

    size_t levelCount() const { return MAX_LEVELS - freeSlotCount; }
};
