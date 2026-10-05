#pragma once
#include <cinttypes>

// A plain index record, as in BreadMaker's orderbookModule/helperStructs.
//
// There is deliberately no validity flag here. The previous version carried an
// `uninitilized` bool and an `operator bool`, but `addEntry` only ever assigned
// `index` and `isBid`, so the flag stayed true for entries that were in the map.
// Every lookup then reported "not found", every update took a fresh pool index,
// and the pool drained until `getIndex()` span forever.
//
// Presence is the map's question to answer, not this struct's: see
// ThreadSafeIndexMap::tryGet.
struct IndexedBidAsk {
    bool isBid = false;
    int64_t index = -1;

    IndexedBidAsk() = default;
    IndexedBidAsk(bool isBid_, int64_t index_) : isBid(isBid_), index(index_) {}
};
